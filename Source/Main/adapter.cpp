/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    adapter.cpp

Abstract:

    Setup and miniport installation.  No resources are used by simple audio sample.
    This sample is to demonstrate how to develop a full featured audio miniport driver.
--*/

#pragma warning (disable : 4127)

//
// All the GUIDS for all the miniports end up in this object.
//
#define PUT_GUIDS_HERE

#include "definitions.h"
#include "endpoints.h"
#include "minipairs.h"
#include "loopback.h"
#include "ioctl.h"

typedef void (*fnPcDriverUnload) (PDRIVER_OBJECT);
fnPcDriverUnload gPCDriverUnloadRoutine = NULL;
extern "C" DRIVER_UNLOAD DriverUnload;

// Saved original IRP_MJ_CREATE/CLOSE/DEVICE_CONTROL handlers from PortCls
static PDRIVER_DISPATCH g_OriginalCreate = NULL;
static PDRIVER_DISPATCH g_OriginalClose = NULL;
static PDRIVER_DISPATCH g_OriginalDeviceControl = NULL;

// Pre-opened registry key handle for persistent settings.
// Opened in DriverEntry (system thread, PreviousMode=KernelMode) to bypass
// access checks when IOCTL handler writes from user-mode thread context.
static HANDLE g_hParametersKey = NULL;

// Standalone control device (separate from PortCls device stack)
static PDEVICE_OBJECT g_ControlDevice = NULL;

// Control device names
#if defined(CABLE_A)
#define AO_CONTROL_DEVICE_NAME   L"\\Device\\AOCableAControl"
#define AO_CONTROL_SYMLINK_NAME  L"\\DosDevices\\AOCableA"
#elif defined(CABLE_B)
#define AO_CONTROL_DEVICE_NAME   L"\\Device\\AOCableBControl"
#define AO_CONTROL_SYMLINK_NAME  L"\\DosDevices\\AOCableB"
#else
#define AO_CONTROL_DEVICE_NAME   L"\\Device\\AOVirtualCableControl"
#define AO_CONTROL_SYMLINK_NAME  L"\\DosDevices\\AOVirtualCable"
#endif

//-----------------------------------------------------------------------------
// Referenced forward.
//-----------------------------------------------------------------------------

DRIVER_ADD_DEVICE AddDevice;

_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH AoDeviceControlHandler;

// Control device handlers
static NTSTATUS AoControlCreate(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);
static NTSTATUS AoControlClose(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);
static NTSTATUS AoControlDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);
static NTSTATUS AoCreateControlDevice(_In_ PDRIVER_OBJECT DriverObject);
static VOID     AoDeleteControlDevice(VOID);

// Forward declarations for registry persistence
static NTSTATUS AoOpenParametersKey(_In_ ACCESS_MASK DesiredAccess, _Out_ PHANDLE phKey);
static VOID AoReadRegistryConfig(_Out_ PULONG pInternalRate, _Out_ PULONG pMaxLatencyMs);
static VOID AoWriteRegistryValue(_In_ PCWSTR ValueName, _In_ ULONG Value);

NTSTATUS
StartDevice
( 
    _In_  PDEVICE_OBJECT,      
    _In_  PIRP,                
    _In_  PRESOURCELIST        
); 

_Dispatch_type_(IRP_MJ_PNP)
DRIVER_DISPATCH PnpHandler;

//
// Rendering streams are not saved to a file by default. Use the registry value 
// DoNotCreateDataFiles (DWORD) = 0 to override this default.
//
DWORD g_DoNotCreateDataFiles = 1;  // default is off.
DWORD g_DisableToneGenerator = 1;  // default is to not generate tones.
UNICODE_STRING g_RegistryPath;      // This is used to store the registry settings path for the driver

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

#pragma code_seg("PAGE")
void ReleaseRegistryStringBuffer()
{
    PAGED_CODE();

    if (g_RegistryPath.Buffer != NULL)
    {
        ExFreePool(g_RegistryPath.Buffer);
        g_RegistryPath.Buffer = NULL;
        g_RegistryPath.Length = 0;
        g_RegistryPath.MaximumLength = 0;
    }
}

//=============================================================================
#pragma code_seg("PAGE")
extern "C"
void DriverUnload 
(
    _In_ PDRIVER_OBJECT DriverObject
)
/*++

Routine Description:

  Our driver unload routine. This just frees the WDF driver object.

Arguments:

  DriverObject - pointer to the driver object

Environment:

    PASSIVE_LEVEL

--*/
{
    PAGED_CODE(); 

    DPF(D_TERSE, ("[DriverUnload]"));

    if (DriverObject == NULL)
    {
        goto Done;
    }

    // Close registry key
    if (g_hParametersKey)
    {
        ZwClose(g_hParametersKey);
        g_hParametersKey = NULL;
    }

    // Delete control device before PortCls unload
    AoDeleteControlDevice();

    ReleaseRegistryStringBuffer();

    //
    // Invoke first the port unload.
    //
    if (gPCDriverUnloadRoutine != NULL)
    {
        gPCDriverUnloadRoutine(DriverObject);
    }

    //
    // Unload WDF driver object. 
    //
    if (WdfGetDriver() != NULL)
    {
        WdfDriverMiniportUnload(WdfGetDriver());
    }

    //
    // Cleanup loopback buffers.
    //
#if defined(CABLE_A)
    LoopbackCleanup(&g_CableALoopback);
#elif defined(CABLE_B)
    LoopbackCleanup(&g_CableBLoopback);
#else
    LoopbackCleanup(&g_CableALoopback);
    LoopbackCleanup(&g_CableBLoopback);
#endif

Done:
    return;
}

//=============================================================================
#pragma code_seg("INIT")
__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
CopyRegistrySettingsPath(
    _In_ PUNICODE_STRING RegistryPath
)
/*++

Routine Description:

Copies the following registry path to a global variable.

\REGISTRY\MACHINE\SYSTEM\ControlSetxxx\Services\<driver>\Parameters

Arguments:

RegistryPath - Registry path passed to DriverEntry

Returns:

NTSTATUS - SUCCESS if able to configure the framework

--*/

{
    // Initializing the unicode string, so that if it is not allocated it will not be deallocated too.
    RtlInitUnicodeString(&g_RegistryPath, NULL);

    g_RegistryPath.MaximumLength = RegistryPath->Length + sizeof(WCHAR);

    g_RegistryPath.Buffer = (PWCH)ExAllocatePool2(POOL_FLAG_PAGED, g_RegistryPath.MaximumLength, MINADAPTER_POOLTAG);

    if (g_RegistryPath.Buffer == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlAppendUnicodeToString(&g_RegistryPath, RegistryPath->Buffer);

    return STATUS_SUCCESS;
}

//=============================================================================
#pragma code_seg("INIT")
__drv_requiresIRQL(PASSIVE_LEVEL)
NTSTATUS
GetRegistrySettings(
    _In_ PUNICODE_STRING RegistryPath
   )
/*++

Routine Description:

    Initialize Driver Framework settings from the driver
    specific registry settings under

    \REGISTRY\MACHINE\SYSTEM\ControlSetxxx\Services\<driver>\Parameters

Arguments:

    RegistryPath - Registry path passed to DriverEntry

Returns:

    NTSTATUS - SUCCESS if able to configure the framework

--*/

{
    NTSTATUS                    ntStatus;
    PDRIVER_OBJECT              DriverObject;
    HANDLE                      DriverKey;
    RTL_QUERY_REGISTRY_TABLE    paramTable[] = {
    // QueryRoutine     Flags                                               Name                     EntryContext             DefaultType                                                    DefaultData              DefaultLength
        { NULL,   RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_TYPECHECK, L"DoNotCreateDataFiles", &g_DoNotCreateDataFiles, (REG_DWORD << RTL_QUERY_REGISTRY_TYPECHECK_SHIFT) | REG_DWORD, &g_DoNotCreateDataFiles, sizeof(ULONG)},
        { NULL,   RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_TYPECHECK, L"DisableToneGenerator", &g_DisableToneGenerator, (REG_DWORD << RTL_QUERY_REGISTRY_TYPECHECK_SHIFT) | REG_DWORD, &g_DisableToneGenerator, sizeof(ULONG)},
        { NULL,   0,                                                        NULL,                    NULL,                    0,                                                             NULL,                    0}
    };

    DPF(D_TERSE, ("[GetRegistrySettings]"));

    PAGED_CODE();
    UNREFERENCED_PARAMETER(RegistryPath);

    DriverObject = WdfDriverWdmGetDriverObject(WdfGetDriver());
    DriverKey = NULL;
    ntStatus = IoOpenDriverRegistryKey(DriverObject, 
                                 DriverRegKeyParameters,
                                 KEY_READ,
                                 0,
                                 &DriverKey);

    if (!NT_SUCCESS(ntStatus))
    {
        return ntStatus;
    }

    ntStatus = RtlQueryRegistryValues(RTL_REGISTRY_HANDLE,
                                  (PCWSTR) DriverKey,
                                  &paramTable[0],
                                  NULL,
                                  NULL);

    if (!NT_SUCCESS(ntStatus)) 
    {
        DPF(D_VERBOSE, ("RtlQueryRegistryValues failed, using default values, 0x%x", ntStatus));
        //
        // Don't return error because we will operate with default values.
        //
    }

    //
    // Dump settings.
    //
    DPF(D_VERBOSE, ("DoNotCreateDataFiles: %u", g_DoNotCreateDataFiles));
    DPF(D_VERBOSE, ("DisableToneGenerator: %u", g_DisableToneGenerator));

    if (DriverKey)
    {
        ZwClose(DriverKey);
    }

    return STATUS_SUCCESS;
}

#pragma code_seg("INIT")
extern "C" DRIVER_INITIALIZE DriverEntry;
extern "C" NTSTATUS
DriverEntry
( 
    _In_  PDRIVER_OBJECT          DriverObject,
    _In_  PUNICODE_STRING         RegistryPathName
)
{
/*++

Routine Description:

  Installable driver initialization entry point.
  This entry point is called directly by the I/O system.

  All audio adapter drivers can use this code without change.

Arguments:

  DriverObject - pointer to the driver object

  RegistryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:

  STATUS_SUCCESS if successful,
  STATUS_UNSUCCESSFUL otherwise.

--*/
    NTSTATUS                    ntStatus;
    WDF_DRIVER_CONFIG           config;

#if defined(CABLE_A)
    DPF(D_TERSE, ("[DriverEntry] AO Cable A"));
#elif defined(CABLE_B)
    DPF(D_TERSE, ("[DriverEntry] AO Cable B"));
#else
    DPF(D_TERSE, ("[DriverEntry]"));
#endif

    // Copy registry Path name in a global variable to be used by modules inside driver.
    // !! NOTE !! Inside this function we are initializing the registrypath, so we MUST NOT add any failing calls
    // before the following call.
    ntStatus = CopyRegistrySettingsPath(RegistryPathName);
    IF_FAILED_ACTION_JUMP(
        ntStatus,
        DPF(D_ERROR, ("Registry path copy error 0x%x", ntStatus)),
        Done);
    
    WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);
    //
    // Set WdfDriverInitNoDispatchOverride flag to tell the framework
    // not to provide dispatch routines for the driver. In other words,
    // the framework must not intercept IRPs that the I/O manager has
    // directed to the driver. In this case, they will be handled by Audio
    // port driver.
    //
    config.DriverInitFlags |= WdfDriverInitNoDispatchOverride;
    config.DriverPoolTag    = MINADAPTER_POOLTAG;

    ntStatus = WdfDriverCreate(DriverObject,
                               RegistryPathName,
                               WDF_NO_OBJECT_ATTRIBUTES,
                               &config,
                               WDF_NO_HANDLE);
    IF_FAILED_ACTION_JUMP(
        ntStatus,
        DPF(D_ERROR, ("WdfDriverCreate failed, 0x%x", ntStatus)),
        Done);

    //
    // Get registry configuration.
    //
    ntStatus = GetRegistrySettings(RegistryPathName);
    IF_FAILED_ACTION_JUMP(
        ntStatus,
        DPF(D_ERROR, ("Registry Configuration error 0x%x", ntStatus)),
        Done);

    //
    // Tell the class driver to initialize the driver.
    //
    ntStatus =  PcInitializeAdapterDriver(DriverObject,
                                          RegistryPathName,
                                          (PDRIVER_ADD_DEVICE)AddDevice);
    IF_FAILED_ACTION_JUMP(
        ntStatus,
        DPF(D_ERROR, ("PcInitializeAdapterDriver failed, 0x%x", ntStatus)),
        Done);

    //
    // To intercept stop/remove/surprise-remove.
    //
    DriverObject->MajorFunction[IRP_MJ_PNP] = PnpHandler;

    //
    // Create standalone control device for control panel IOCTL access.
    // This is separate from the PortCls device stack so device interface
    // enumeration works reliably.
    //
    ntStatus = AoCreateControlDevice(DriverObject);
    if (!NT_SUCCESS(ntStatus))
    {
        DPF(D_TERSE, ("[DriverEntry] Control device creation failed 0x%x (non-fatal)", ntStatus));
        ntStatus = STATUS_SUCCESS;  // Non-fatal
    }

    //
    // Hook IRP_MJ_CREATE/CLOSE/DEVICE_CONTROL to route control device IRPs.
    //
    g_OriginalCreate = DriverObject->MajorFunction[IRP_MJ_CREATE];
    g_OriginalClose = DriverObject->MajorFunction[IRP_MJ_CLOSE];
    g_OriginalDeviceControl = DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];
    DriverObject->MajorFunction[IRP_MJ_CREATE] = AoControlCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = AoControlClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = AoControlDeviceControl;

    //
    // Hook the port class unload function
    //
    gPCDriverUnloadRoutine = DriverObject->DriverUnload;
    DriverObject->DriverUnload = DriverUnload;

    //
    // Initialize loopback buffers.
    //
#if defined(CABLE_A)
    ntStatus = LoopbackInit(&g_CableALoopback);
    IF_FAILED_ACTION_JUMP(
        ntStatus,
        DPF(D_ERROR, ("LoopbackInit CableA failed, 0x%x", ntStatus)),
        Done);
#elif defined(CABLE_B)
    ntStatus = LoopbackInit(&g_CableBLoopback);
    IF_FAILED_ACTION_JUMP(
        ntStatus,
        DPF(D_ERROR, ("LoopbackInit CableB failed, 0x%x", ntStatus)),
        Done);
#else
    ntStatus = LoopbackInit(&g_CableALoopback);
    IF_FAILED_ACTION_JUMP(
        ntStatus,
        DPF(D_ERROR, ("LoopbackInit CableA failed, 0x%x", ntStatus)),
        Done);

    ntStatus = LoopbackInit(&g_CableBLoopback);
    IF_FAILED_ACTION_JUMP(
        ntStatus,
        DPF(D_ERROR, ("LoopbackInit CableB failed, 0x%x", ntStatus)),
        Done);
#endif

    //
    // Open registry Parameters key for persistent settings.
    // Must be done here (system thread context) so Zw* calls bypass access checks.
    //
    {
        NTSTATUS regStatus = AoOpenParametersKey(KEY_READ | KEY_WRITE, &g_hParametersKey);
        if (!NT_SUCCESS(regStatus))
        {
            DPF(D_TERSE, ("[DriverEntry] AoOpenParametersKey failed 0x%x (non-fatal)", regStatus));
            g_hParametersKey = NULL;
        }
    }

    //
    // Apply saved registry settings to loopback buffers.
    //
    {
        ULONG savedRate = LB_DEFAULT_INTERNAL_RATE;
        ULONG savedLatency = LB_DEFAULT_LATENCY_MS;
        AoReadRegistryConfig(&savedRate, &savedLatency);

#if defined(CABLE_A)
        LoopbackSetInternalRate(&g_CableALoopback, savedRate);
        LoopbackResizeBuffer(&g_CableALoopback, savedLatency);
#elif defined(CABLE_B)
        LoopbackSetInternalRate(&g_CableBLoopback, savedRate);
        LoopbackResizeBuffer(&g_CableBLoopback, savedLatency);
#else
        LoopbackSetInternalRate(&g_CableALoopback, savedRate);
        LoopbackResizeBuffer(&g_CableALoopback, savedLatency);
        LoopbackSetInternalRate(&g_CableBLoopback, savedRate);
        LoopbackResizeBuffer(&g_CableBLoopback, savedLatency);
#endif
        DPF(D_TERSE, ("[DriverEntry] Registry config: Rate=%u, Latency=%ums", savedRate, savedLatency));
    }

    //
    // All done.
    //
    ntStatus = STATUS_SUCCESS;

Done:

    if (!NT_SUCCESS(ntStatus))
    {
        if (g_hParametersKey)
        {
            ZwClose(g_hParametersKey);
            g_hParametersKey = NULL;
        }
        AoDeleteControlDevice();

#if defined(CABLE_A)
        LoopbackCleanup(&g_CableALoopback);
#elif defined(CABLE_B)
        LoopbackCleanup(&g_CableBLoopback);
#else
        LoopbackCleanup(&g_CableALoopback);
        LoopbackCleanup(&g_CableBLoopback);
#endif

        if (WdfGetDriver() != NULL)
        {
            WdfDriverMiniportUnload(WdfGetDriver());
        }

        ReleaseRegistryStringBuffer();
    }

    return ntStatus;
} // DriverEntry

#pragma code_seg()
// disable prefast warning 28152 because 
// DO_DEVICE_INITIALIZING is cleared in PcAddAdapterDevice
#pragma warning(disable:28152)
#pragma code_seg("PAGE")
//=============================================================================
NTSTATUS AddDevice
( 
    _In_  PDRIVER_OBJECT    DriverObject,
    _In_  PDEVICE_OBJECT    PhysicalDeviceObject 
)
/*++

Routine Description:

  The Plug & Play subsystem is handing us a brand new PDO, for which we
  (by means of INF registration) have been asked to provide a driver.

  We need to determine if we need to be in the driver stack for the device.
  Create a function device object to attach to the stack
  Initialize that device object
  Return status success.

  All audio adapter drivers can use this code without change.

Arguments:

  DriverObject - pointer to a driver object

  PhysicalDeviceObject -  pointer to a device object created by the
                            underlying bus driver.

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    NTSTATUS        ntStatus;
    ULONG           maxObjects;

    DPF(D_TERSE, ("[AddDevice]"));

    maxObjects = g_MaxMiniports;

    // Tell the class driver to add the device.
    //
    ntStatus = 
        PcAddAdapterDevice
        ( 
            DriverObject,
            PhysicalDeviceObject,
            PCPFNSTARTDEVICE(StartDevice),
            maxObjects,
            0
        );

    return ntStatus;
} // AddDevice

#pragma code_seg()
NTSTATUS
_IRQL_requires_max_(DISPATCH_LEVEL)
PowerControlCallback
(
    _In_        LPCGUID PowerControlCode,
    _In_opt_    PVOID   InBuffer,
    _In_        SIZE_T  InBufferSize,
    _Out_writes_bytes_to_(OutBufferSize, *BytesReturned) PVOID OutBuffer,
    _In_        SIZE_T  OutBufferSize,
    _Out_opt_   PSIZE_T BytesReturned,
    _In_opt_    PVOID   Context
)
{
    UNREFERENCED_PARAMETER(PowerControlCode);
    UNREFERENCED_PARAMETER(InBuffer);
    UNREFERENCED_PARAMETER(InBufferSize);
    UNREFERENCED_PARAMETER(OutBuffer);
    UNREFERENCED_PARAMETER(OutBufferSize);
    UNREFERENCED_PARAMETER(BytesReturned);
    UNREFERENCED_PARAMETER(Context);
    
    return STATUS_NOT_IMPLEMENTED;
}

#pragma code_seg("PAGE")
NTSTATUS 
InstallEndpointRenderFilters(
    _In_ PDEVICE_OBJECT     _pDeviceObject, 
    _In_ PIRP               _pIrp, 
    _In_ PADAPTERCOMMON     _pAdapterCommon,
    _In_ PENDPOINT_MINIPAIR _pAeMiniports
    )
{
    NTSTATUS                    ntStatus                = STATUS_SUCCESS;
    PUNKNOWN                    unknownTopology         = NULL;
    PUNKNOWN                    unknownWave             = NULL;
    PPORTCLSETWHELPER           pPortClsEtwHelper       = NULL;
#ifdef _USE_IPortClsRuntimePower
    PPORTCLSRUNTIMEPOWER        pPortClsRuntimePower    = NULL;
#endif // _USE_IPortClsRuntimePower
    PPORTCLSStreamResourceManager pPortClsResMgr        = NULL;
    PPORTCLSStreamResourceManager2 pPortClsResMgr2      = NULL;

    PAGED_CODE();
    
    UNREFERENCED_PARAMETER(_pDeviceObject);

    ntStatus = _pAdapterCommon->InstallEndpointFilters(
        _pIrp,
        _pAeMiniports,
        NULL,
        &unknownTopology,
        &unknownWave,
        NULL, NULL);

    if (unknownWave) // IID_IPortClsEtwHelper and IID_IPortClsRuntimePower interfaces are only exposed on the WaveRT port.
    {
        ntStatus = unknownWave->QueryInterface (IID_IPortClsEtwHelper, (PVOID *)&pPortClsEtwHelper);
        if (NT_SUCCESS(ntStatus))
        {
            _pAdapterCommon->SetEtwHelper(pPortClsEtwHelper);
            ASSERT(pPortClsEtwHelper != NULL);
            pPortClsEtwHelper->Release();
        }

#ifdef _USE_IPortClsRuntimePower
        // Let's get the runtime power interface on PortCls.  
        ntStatus = unknownWave->QueryInterface(IID_IPortClsRuntimePower, (PVOID *)&pPortClsRuntimePower);
        if (NT_SUCCESS(ntStatus))
        {
            // This interface would typically be stashed away for later use.  Instead,
            // let's just send an empty control with GUID_NULL.
            NTSTATUS ntStatusTest =
                pPortClsRuntimePower->SendPowerControl
                (
                    _pDeviceObject,
                    &GUID_NULL,
                    NULL,
                    0,
                    NULL,
                    0,
                    NULL
                );

            if (NT_SUCCESS(ntStatusTest) || STATUS_NOT_IMPLEMENTED == ntStatusTest || STATUS_NOT_SUPPORTED == ntStatusTest)
            {
                ntStatus = pPortClsRuntimePower->RegisterPowerControlCallback(_pDeviceObject, &PowerControlCallback, NULL);
                if (NT_SUCCESS(ntStatus))
                {
                    ntStatus = pPortClsRuntimePower->UnregisterPowerControlCallback(_pDeviceObject);
                }
            }
            else
            {
                ntStatus = ntStatusTest;
            }

            pPortClsRuntimePower->Release();
        }
#endif // _USE_IPortClsRuntimePower

        //
        // Test: add and remove current thread as streaming audio resource.  
        // In a real driver you should only add interrupts and driver-owned threads 
        // (i.e., do NOT add the current thread as streaming resource).
        //
        // testing IPortClsStreamResourceManager:
        ntStatus = unknownWave->QueryInterface(IID_IPortClsStreamResourceManager, (PVOID *)&pPortClsResMgr);
        if (NT_SUCCESS(ntStatus))
        {
            PCSTREAMRESOURCE_DESCRIPTOR res;
            PCSTREAMRESOURCE hRes = NULL;
            PDEVICE_OBJECT pdo = NULL;

            PcGetPhysicalDeviceObject(_pDeviceObject, &pdo);
            PCSTREAMRESOURCE_DESCRIPTOR_INIT(&res);
            res.Pdo = pdo;
            res.Type = ePcStreamResourceThread;
            res.Resource.Thread = PsGetCurrentThread();
            
            NTSTATUS ntStatusTest = pPortClsResMgr->AddStreamResource(NULL, &res, &hRes);
            if (NT_SUCCESS(ntStatusTest))
            {
                pPortClsResMgr->RemoveStreamResource(hRes);
                hRes = NULL;
            }

            pPortClsResMgr->Release();
            pPortClsResMgr = NULL;
        }
        
        // testing IPortClsStreamResourceManager2:
        ntStatus = unknownWave->QueryInterface(IID_IPortClsStreamResourceManager2, (PVOID *)&pPortClsResMgr2);
        if (NT_SUCCESS(ntStatus))
        {
            PCSTREAMRESOURCE_DESCRIPTOR res;
            PCSTREAMRESOURCE hRes = NULL;
            PDEVICE_OBJECT pdo = NULL;

            PcGetPhysicalDeviceObject(_pDeviceObject, &pdo);
            PCSTREAMRESOURCE_DESCRIPTOR_INIT(&res);
            res.Pdo = pdo;
            res.Type = ePcStreamResourceThread;
            res.Resource.Thread = PsGetCurrentThread();
            
            NTSTATUS ntStatusTest = pPortClsResMgr2->AddStreamResource2(pdo, NULL, &res, &hRes);
            if (NT_SUCCESS(ntStatusTest))
            {
                pPortClsResMgr2->RemoveStreamResource(hRes);
                hRes = NULL;
            }

            pPortClsResMgr2->Release();
            pPortClsResMgr2 = NULL;
        }
    }

    SAFE_RELEASE(unknownTopology);
    SAFE_RELEASE(unknownWave);

    return ntStatus;
}

#pragma code_seg("PAGE")
NTSTATUS 
InstallAllRenderFilters(
    _In_ PDEVICE_OBJECT _pDeviceObject, 
    _In_ PIRP           _pIrp, 
    _In_ PADAPTERCOMMON _pAdapterCommon
    )
{
    NTSTATUS            ntStatus;
    PENDPOINT_MINIPAIR* ppAeMiniports   = g_RenderEndpoints;
    
    PAGED_CODE();

    for(ULONG i = 0; i < g_cRenderEndpoints; ++i, ++ppAeMiniports)
    {
        ntStatus = InstallEndpointRenderFilters(_pDeviceObject, _pIrp, _pAdapterCommon, *ppAeMiniports);
        IF_FAILED_JUMP(ntStatus, Exit);
    }
    
    ntStatus = STATUS_SUCCESS;

Exit:
    return ntStatus;
}

#pragma code_seg("PAGE")
NTSTATUS
InstallEndpointCaptureFilters(
    _In_ PDEVICE_OBJECT     _pDeviceObject,
    _In_ PIRP               _pIrp,
    _In_ PADAPTERCOMMON     _pAdapterCommon,
    _In_ PENDPOINT_MINIPAIR _pAeMiniports
)
{
    NTSTATUS    ntStatus = STATUS_SUCCESS;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(_pDeviceObject);

    ntStatus = _pAdapterCommon->InstallEndpointFilters(
        _pIrp,
        _pAeMiniports,
        NULL,
        NULL,
        NULL,
        NULL, NULL);

    return ntStatus;
}

#pragma code_seg("PAGE")
NTSTATUS
InstallAllCaptureFilters(
    _In_ PDEVICE_OBJECT _pDeviceObject,
    _In_ PIRP           _pIrp,
    _In_ PADAPTERCOMMON _pAdapterCommon
)
{
    NTSTATUS            ntStatus;
    PENDPOINT_MINIPAIR* ppAeMiniports = g_CaptureEndpoints;

    PAGED_CODE();

    for (ULONG i = 0; i < g_cCaptureEndpoints; ++i, ++ppAeMiniports)
    {
        ntStatus = InstallEndpointCaptureFilters(_pDeviceObject, _pIrp, _pAdapterCommon, *ppAeMiniports);
        IF_FAILED_JUMP(ntStatus, Exit);
    }

    ntStatus = STATUS_SUCCESS;

Exit:
    return ntStatus;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
StartDevice
( 
    _In_  PDEVICE_OBJECT          DeviceObject,     
    _In_  PIRP                    Irp,              
    _In_  PRESOURCELIST           ResourceList      
)  
{
/*++

Routine Description:

  This function is called by the operating system when the device is 
  started.
  It is responsible for starting the miniports.  This code is specific to    
  the adapter because it calls out miniports for functions that are specific 
  to the adapter.                                                            

Arguments:

  DeviceObject - pointer to the driver object

  Irp - pointer to the irp 

  ResourceList - pointer to the resource list assigned by PnP manager

Return Value:

  NT status code.

--*/
    UNREFERENCED_PARAMETER(ResourceList);

    PAGED_CODE();

    ASSERT(DeviceObject);
    ASSERT(Irp);
    ASSERT(ResourceList);

    NTSTATUS                    ntStatus        = STATUS_SUCCESS;

    PADAPTERCOMMON              pAdapterCommon  = NULL;
    PUNKNOWN                    pUnknownCommon  = NULL;
    PortClassDeviceContext*     pExtension      = static_cast<PortClassDeviceContext*>(DeviceObject->DeviceExtension);

    DPF_ENTER(("[StartDevice]"));

    //
    // create a new adapter common object
    //
    ntStatus = NewAdapterCommon( 
                                &pUnknownCommon,
                                IID_IAdapterCommon,
                                NULL,
                                POOL_FLAG_NON_PAGED 
                                );
    IF_FAILED_JUMP(ntStatus, Exit);

    ntStatus = pUnknownCommon->QueryInterface( IID_IAdapterCommon,(PVOID *) &pAdapterCommon);
    IF_FAILED_JUMP(ntStatus, Exit);

    ntStatus = pAdapterCommon->Init(DeviceObject);
    IF_FAILED_JUMP(ntStatus, Exit);

    //
    // register with PortCls for power-management services
    ntStatus = PcRegisterAdapterPowerManagement( PUNKNOWN(pAdapterCommon), DeviceObject);
    IF_FAILED_JUMP(ntStatus, Exit);

    //
    // Determine which endpoints to install based on hardware ID.
    // ROOT\AOCableA  -> Cable A only
    // ROOT\AOCableB  -> Cable B only
    // ROOT\AOVirtualAudio (or legacy ROOT\VirtualAudioDriver) -> Speaker + Mic
    //
    {
        PDEVICE_OBJECT pdo = NULL;
        ntStatus = PcGetPhysicalDeviceObject(DeviceObject, &pdo);
        IF_FAILED_JUMP(ntStatus, Exit);

        WCHAR hardwareId[256] = {0};
        ULONG resultLen = 0;
        ntStatus = IoGetDeviceProperty(
            pdo,
            DevicePropertyHardwareID,
            sizeof(hardwareId),
            hardwareId,
            &resultLen);
        IF_FAILED_JUMP(ntStatus, Exit);

        if (wcsstr(hardwareId, L"AOCableA") != NULL)
        {
            DPF(D_TERSE, ("[StartDevice] Hardware ID: AOCableA"));
            ntStatus = InstallEndpointRenderFilters(DeviceObject, Irp, pAdapterCommon, &CableASpeakerMiniports);
            IF_FAILED_JUMP(ntStatus, Exit);
            ntStatus = InstallEndpointCaptureFilters(DeviceObject, Irp, pAdapterCommon, &CableAMicMiniports);
            IF_FAILED_JUMP(ntStatus, Exit);
        }
        else if (wcsstr(hardwareId, L"AOCableB") != NULL)
        {
            DPF(D_TERSE, ("[StartDevice] Hardware ID: AOCableB"));
            ntStatus = InstallEndpointRenderFilters(DeviceObject, Irp, pAdapterCommon, &CableBSpeakerMiniports);
            IF_FAILED_JUMP(ntStatus, Exit);
            ntStatus = InstallEndpointCaptureFilters(DeviceObject, Irp, pAdapterCommon, &CableBMicMiniports);
            IF_FAILED_JUMP(ntStatus, Exit);
        }
        else
        {
            // AOVirtualAudio or legacy VirtualAudioDriver - install Speaker + Mic
            DPF(D_TERSE, ("[StartDevice] Hardware ID: AOVirtualAudio (default)"));
            ntStatus = InstallEndpointRenderFilters(DeviceObject, Irp, pAdapterCommon, &SpeakerMiniports);
            IF_FAILED_JUMP(ntStatus, Exit);
            ntStatus = InstallEndpointCaptureFilters(DeviceObject, Irp, pAdapterCommon, &MicArray1Miniports);
            IF_FAILED_JUMP(ntStatus, Exit);
        }
    }

Exit:

    //
    // Stash the adapter common object in the device extension so
    // we can access it for cleanup on stop/removal.
    //
    if (pAdapterCommon)
    {
        ASSERT(pExtension != NULL);
        pExtension->m_pCommon = pAdapterCommon;
    }

    //
    // Release the adapter IUnknown interface.
    //
    SAFE_RELEASE(pUnknownCommon);
    
    return ntStatus;
} // StartDevice

//=============================================================================
// Registry persistence for driver settings
//=============================================================================
#pragma code_seg("PAGE")
static NTSTATUS AoOpenParametersKey(
    _In_  ACCESS_MASK DesiredAccess,
    _Out_ PHANDLE     phKey
)
{
    PAGED_CODE();

    // Open (or create) the "Parameters" subkey under the driver's service key.
    // g_RegistryPath = "\Registry\Machine\SYSTEM\CurrentControlSet\Services\AOCableX"
    // This key is always accessible from kernel mode regardless of calling thread context.
    *phKey = NULL;

    if (g_RegistryPath.Buffer == NULL)
        return STATUS_UNSUCCESSFUL;

    // Open the service key first
    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, &g_RegistryPath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    HANDLE hServiceKey = NULL;
    NTSTATUS st = ZwOpenKey(&hServiceKey, KEY_CREATE_SUB_KEY | DesiredAccess, &oa);
    if (!NT_SUCCESS(st))
        return st;

    // Open or create "Parameters" subkey
    UNICODE_STRING subKey;
    RtlInitUnicodeString(&subKey, L"Parameters");
    InitializeObjectAttributes(&oa, &subKey, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, hServiceKey, NULL);

    ULONG disposition = 0;
    st = ZwCreateKey(phKey, DesiredAccess, &oa, 0, NULL, REG_OPTION_NON_VOLATILE, &disposition);
    ZwClose(hServiceKey);
    return st;
}

static VOID AoWriteRegistryValue(
    _In_ PCWSTR ValueName,
    _In_ ULONG  Value
)
{
    PAGED_CODE();

    // Use the pre-opened handle from DriverEntry (system thread context).
    // This bypasses access checks when called from user-mode IOCTL context.
    if (!g_hParametersKey)
        return;

    UNICODE_STRING valueName;
    RtlInitUnicodeString(&valueName, ValueName);
    ZwSetValueKey(g_hParametersKey, &valueName, 0, REG_DWORD, &Value, sizeof(ULONG));
}

#pragma code_seg("PAGE")
static VOID AoReadRegistryConfig(
    _Out_ PULONG pInternalRate,
    _Out_ PULONG pMaxLatencyMs
)
{
    PAGED_CODE();

    *pInternalRate = LB_DEFAULT_INTERNAL_RATE;
    *pMaxLatencyMs = LB_DEFAULT_LATENCY_MS;

    // Use the pre-opened handle from DriverEntry.
    if (!g_hParametersKey)
        return;
    HANDLE hKey = g_hParametersKey;

    // Read InternalRate
    {
        UNICODE_STRING valName;
        RtlInitUnicodeString(&valName, AO_REG_INTERNAL_RATE);

        UCHAR buf[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONG)];
        ULONG resultLen = 0;
        NTSTATUS st = ZwQueryValueKey(hKey, &valName, KeyValuePartialInformation, buf, sizeof(buf), &resultLen);
        if (NT_SUCCESS(st))
        {
            PKEY_VALUE_PARTIAL_INFORMATION info = (PKEY_VALUE_PARTIAL_INFORMATION)buf;
            if (info->Type == REG_DWORD && info->DataLength == sizeof(ULONG))
            {
                ULONG val = *(PULONG)info->Data;
                if (val == 44100 || val == 48000 || val == 96000 || val == 192000)
                    *pInternalRate = val;
            }
        }
    }

    // Read MaxLatencyMs
    {
        UNICODE_STRING valName;
        RtlInitUnicodeString(&valName, AO_REG_MAX_LATENCY);

        UCHAR buf[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONG)];
        ULONG resultLen = 0;
        NTSTATUS st = ZwQueryValueKey(hKey, &valName, KeyValuePartialInformation, buf, sizeof(buf), &resultLen);
        if (NT_SUCCESS(st))
        {
            PKEY_VALUE_PARTIAL_INFORMATION info = (PKEY_VALUE_PARTIAL_INFORMATION)buf;
            if (info->Type == REG_DWORD && info->DataLength == sizeof(ULONG))
            {
                ULONG val = *(PULONG)info->Data;
                if (val >= LB_MIN_LATENCY_MS && val <= LB_MAX_LATENCY_MS)
                    *pMaxLatencyMs = val;
            }
        }
    }

    // Do NOT close hKey -- it is the shared g_hParametersKey handle.
}

//=============================================================================
// IOCTL dispatch for AO Virtual Cable control panel communication
//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
AoDeviceControlHandler(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    PAGED_CODE();

    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONG ioctl = irpSp->Parameters.DeviceIoControl.IoControlCode;
    NTSTATUS status = STATUS_NOT_SUPPORTED;
    ULONG bytesReturned = 0;

    // Only handle our custom IOCTLs
    switch (ioctl)
    {
    case IOCTL_AO_GET_CONFIG:
    {
        if (irpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(AO_CONFIG))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        AO_CONFIG* pConfig = (AO_CONFIG*)Irp->AssociatedIrp.SystemBuffer;
#if defined(CABLE_A)
        pConfig->InternalRate = g_CableALoopback.InternalRate;
        pConfig->MaxLatencyMs = g_CableALoopback.MaxLatencyMs;
#elif defined(CABLE_B)
        pConfig->InternalRate = g_CableBLoopback.InternalRate;
        pConfig->MaxLatencyMs = g_CableBLoopback.MaxLatencyMs;
#else
        pConfig->InternalRate = g_CableALoopback.InternalRate;
        pConfig->MaxLatencyMs = g_CableALoopback.MaxLatencyMs;
#endif
        pConfig->InternalBits = LB_INTERNAL_BITS;
        pConfig->InternalChannels = LB_INTERNAL_CHANNELS;
        bytesReturned = sizeof(AO_CONFIG);
        status = STATUS_SUCCESS;
        break;
    }

    case IOCTL_AO_SET_INTERNAL_RATE:
    {
        if (irpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        ULONG newRate = *(ULONG*)Irp->AssociatedIrp.SystemBuffer;
#if defined(CABLE_A)
        status = LoopbackSetInternalRate(&g_CableALoopback, newRate);
#elif defined(CABLE_B)
        status = LoopbackSetInternalRate(&g_CableBLoopback, newRate);
#else
        status = LoopbackSetInternalRate(&g_CableALoopback, newRate);
        if (NT_SUCCESS(status))
            status = LoopbackSetInternalRate(&g_CableBLoopback, newRate);
#endif
        if (NT_SUCCESS(status))
            AoWriteRegistryValue(AO_REG_INTERNAL_RATE, newRate);
        break;
    }

    case IOCTL_AO_SET_MAX_LATENCY:
    {
        if (irpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        ULONG newLatency = *(ULONG*)Irp->AssociatedIrp.SystemBuffer;
#if defined(CABLE_A)
        status = LoopbackResizeBuffer(&g_CableALoopback, newLatency);
#elif defined(CABLE_B)
        status = LoopbackResizeBuffer(&g_CableBLoopback, newLatency);
#else
        status = LoopbackResizeBuffer(&g_CableALoopback, newLatency);
        if (NT_SUCCESS(status))
            status = LoopbackResizeBuffer(&g_CableBLoopback, newLatency);
#endif
        if (NT_SUCCESS(status))
            AoWriteRegistryValue(AO_REG_MAX_LATENCY, newLatency);
        break;
    }

    case IOCTL_AO_GET_STREAM_STATUS:
    {
        if (irpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof(AO_STREAM_STATUS))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        AO_STREAM_STATUS* pStatus = (AO_STREAM_STATUS*)Irp->AssociatedIrp.SystemBuffer;
        RtlZeroMemory(pStatus, sizeof(AO_STREAM_STATUS));

#if defined(CABLE_A) || !defined(CABLE_B)
        pStatus->CableA_Speaker.Active      = g_CableALoopback.SpeakerActive;
        pStatus->CableA_Speaker.SampleRate   = g_CableALoopback.SpeakerFormat.SampleRate;
        pStatus->CableA_Speaker.BitsPerSample= g_CableALoopback.SpeakerFormat.BitsPerSample;
        pStatus->CableA_Speaker.Channels     = g_CableALoopback.SpeakerFormat.nChannels;
        pStatus->CableA_Mic.Active          = g_CableALoopback.MicActive;
        pStatus->CableA_Mic.SampleRate       = g_CableALoopback.MicFormat.SampleRate;
        pStatus->CableA_Mic.BitsPerSample    = g_CableALoopback.MicFormat.BitsPerSample;
        pStatus->CableA_Mic.Channels         = g_CableALoopback.MicFormat.nChannels;
#endif

#if defined(CABLE_B) || !defined(CABLE_A)
        pStatus->CableB_Speaker.Active      = g_CableBLoopback.SpeakerActive;
        pStatus->CableB_Speaker.SampleRate   = g_CableBLoopback.SpeakerFormat.SampleRate;
        pStatus->CableB_Speaker.BitsPerSample= g_CableBLoopback.SpeakerFormat.BitsPerSample;
        pStatus->CableB_Speaker.Channels     = g_CableBLoopback.SpeakerFormat.nChannels;
        pStatus->CableB_Mic.Active          = g_CableBLoopback.MicActive;
        pStatus->CableB_Mic.SampleRate       = g_CableBLoopback.MicFormat.SampleRate;
        pStatus->CableB_Mic.BitsPerSample    = g_CableBLoopback.MicFormat.BitsPerSample;
        pStatus->CableB_Mic.Channels         = g_CableBLoopback.MicFormat.nChannels;
#endif

        bytesReturned = sizeof(AO_STREAM_STATUS);
        status = STATUS_SUCCESS;
        break;
    }

    default:
        // Not our IOCTL - pass to PortCls (only for PortCls devices, not control device)
        if (DeviceObject != g_ControlDevice && g_OriginalDeviceControl)
        {
            return g_OriginalDeviceControl(DeviceObject, Irp);
        }
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesReturned;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

//=============================================================================
// Standalone control device - separate from PortCls device stack
//=============================================================================

#pragma code_seg("PAGE")
static NTSTATUS
AoControlCreate(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    PAGED_CODE();
    if (DeviceObject == g_ControlDevice)
    {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }
    // Not our control device - pass to PortCls
    if (g_OriginalCreate)
        return g_OriginalCreate(DeviceObject, Irp);
    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_INVALID_DEVICE_REQUEST;
}

#pragma code_seg("PAGE")
static NTSTATUS
AoControlClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    PAGED_CODE();
    if (DeviceObject == g_ControlDevice)
    {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }
    if (g_OriginalClose)
        return g_OriginalClose(DeviceObject, Irp);
    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_INVALID_DEVICE_REQUEST;
}

#pragma code_seg("PAGE")
static NTSTATUS
AoControlDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    PAGED_CODE();
    // Route our control device IOCTLs to the existing handler
    if (DeviceObject == g_ControlDevice)
        return AoDeviceControlHandler(DeviceObject, Irp);
    // Not our device - pass to PortCls
    if (g_OriginalDeviceControl)
        return g_OriginalDeviceControl(DeviceObject, Irp);
    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_NOT_SUPPORTED;
}

#pragma code_seg("PAGE")
static NTSTATUS
AoCreateControlDevice(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    PAGED_CODE();

    UNICODE_STRING devName = RTL_CONSTANT_STRING(AO_CONTROL_DEVICE_NAME);
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(AO_CONTROL_SYMLINK_NAME);
    NTSTATUS status;

    status = IoCreateDevice(
        DriverObject,
        0,
        &devName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &g_ControlDevice);

    if (!NT_SUCCESS(status))
    {
        DPF(D_ERROR, ("[AoCreateControlDevice] IoCreateDevice failed 0x%x", status));
        return status;
    }

    status = IoCreateSymbolicLink(&symLink, &devName);
    if (!NT_SUCCESS(status))
    {
        DPF(D_ERROR, ("[AoCreateControlDevice] IoCreateSymbolicLink failed 0x%x", status));
        IoDeleteDevice(g_ControlDevice);
        g_ControlDevice = NULL;
        return status;
    }

    g_ControlDevice->Flags &= ~DO_DEVICE_INITIALIZING;
    g_ControlDevice->Flags |= DO_BUFFERED_IO;

    DPF(D_TERSE, ("[AoCreateControlDevice] Control device created: %wZ", &symLink));
    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
static VOID
AoDeleteControlDevice(VOID)
{
    PAGED_CODE();

    if (g_ControlDevice)
    {
        UNICODE_STRING symLink = RTL_CONSTANT_STRING(AO_CONTROL_SYMLINK_NAME);
        IoDeleteSymbolicLink(&symLink);
        IoDeleteDevice(g_ControlDevice);
        g_ControlDevice = NULL;
    }
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS
PnpHandler
(
    _In_ DEVICE_OBJECT *_DeviceObject, 
    _Inout_ IRP *_Irp
)
/*++

Routine Description:

  Handles PnP IRPs                                                           

Arguments:

  _DeviceObject - Functional Device object pointer.

  _Irp - The Irp being passed

Return Value:

  NT status code.

--*/
{
    NTSTATUS                ntStatus = STATUS_UNSUCCESSFUL;
    IO_STACK_LOCATION      *stack;
    PortClassDeviceContext *ext;

    // Documented https://msdn.microsoft.com/en-us/library/windows/hardware/ff544039(v=vs.85).aspx
    // This method will be called in IRQL PASSIVE_LEVEL
#pragma warning(suppress: 28118)
    PAGED_CODE(); 

    ASSERT(_DeviceObject);
    ASSERT(_Irp);

    //
    // Check for the REMOVE_DEVICE irp.  If we're being unloaded, 
    // uninstantiate our devices and release the adapter common
    // object.
    //
    stack = IoGetCurrentIrpStackLocation(_Irp);

    switch (stack->MinorFunction)
    {
    case IRP_MN_REMOVE_DEVICE:
    case IRP_MN_SURPRISE_REMOVAL:
        ext = static_cast<PortClassDeviceContext*>(_DeviceObject->DeviceExtension);

        if (ext->m_pCommon != NULL)
        {
            ext->m_pCommon->Cleanup();

            ext->m_pCommon->Release();
            ext->m_pCommon = NULL;
        }
        break;

    case IRP_MN_STOP_DEVICE:
        // Do not release adapter common here - device may restart via IRP_MN_START_DEVICE.
        break;

    default:
        break;
    }
    
    ntStatus = PcDispatchIrp(_DeviceObject, _Irp);

    return ntStatus;
}

#pragma code_seg()

