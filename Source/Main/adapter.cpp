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
#include "transport_engine.h"
#include "ioctl.h"

// Layout verification (must match loopback.cpp in Utilities.lib)
C_ASSERT(sizeof(LB_SRC_STATE) == 528);
C_ASSERT(sizeof(LOOPBACK_BUFFER) == 1248);
C_ASSERT(FIELD_OFFSET(LOOPBACK_BUFFER, InternalRate) == 0x4CC);
C_ASSERT(FIELD_OFFSET(LOOPBACK_BUFFER, MaxLatencyMs) == 0x4D0);
C_ASSERT(FIELD_OFFSET(LOOPBACK_BUFFER, InternalChannels) == 0x4D4);
C_ASSERT(FIELD_OFFSET(LOOPBACK_BUFFER, InternalBlockAlign) == 0x4D8);

// Phase 1: AO_V2_DIAG shape guard. Mirrors the ioctl.h C_ASSERT so a
// mismatch caused by editing only one of the two headers fails to build.
// Phase 5 (2026-04-14): bumped from 116 to 132 after render-side drive
// counter tail extension.
// Phase 1 Step 6 (2026-05-08): bumped from 132 to 172 after appending
// per-cable FRAME_PIPE ring diagnostics (Overflow / Underrun /
// UnderrunFlag / RingFill / WrapBound).
// Phase 3 Step 2 prep (2026-05-09): bumped from 172 to 220 after
// appending per-stream shadow helper counters (4 streams * 3 ULONGs)
// for live observability of canonical helper call sources. P7 layout.
C_ASSERT(sizeof(AO_V2_DIAG) == 220);

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

// Pre-upgrade quiesce state: when TRUE, new opens are rejected and
// control device is deleted once all existing handles drain.
static volatile LONG g_PrepareUnload = FALSE;
static volatile LONG g_ControlOpenCount = 0;     // open handle refcount
static volatile LONG g_SymlinkDeleted = FALSE;    // symlink already removed

//=============================================================================
// Dynamic 16ch format block -- single allocation holds all PortCls tables.
// Built at StartDevice for 16ch mode; static 16ch tables removed from binary
// because their mere presence in .data causes PortCls BugCheck 0xD1.
//=============================================================================
#define CABLE_8CH_FORMAT_COUNT   108
#define CABLE_16CH_EXTRA_COUNT    12   // 4 rates x (16bit + 24bit + 32float)
#define CABLE_16CH_TOTAL_COUNT  (CABLE_8CH_FORMAT_COUNT + CABLE_16CH_EXTRA_COUNT)
#define AO_DYN16_POOLTAG        '61oA'

typedef struct _AO_DYN16_BLOCK {
    AO_ENDPOINT_FORMAT_BINDING          Binding;
    PCFILTER_DESCRIPTOR                 FilterDesc;
    PCPIN_DESCRIPTOR                    Pins[2];
    KSDATARANGE_AUDIO                   DataRanges[2];
    PKSDATARANGE                        DataRangePointers[4];
    MODE_AND_DEFAULT_FORMAT             Mode;
    PIN_DEVICE_FORMATS_AND_MODES        PinFmtModes[2];
    KSDATAFORMAT_WAVEFORMATEXTENSIBLE   WaveFormats[CABLE_16CH_TOTAL_COUNT];
} AO_DYN16_BLOCK, *PAO_DYN16_BLOCK;

static PAO_DYN16_BLOCK g_pDyn16RenderA  = NULL;
static PAO_DYN16_BLOCK g_pDyn16CaptureA = NULL;
static PAO_DYN16_BLOCK g_pDyn16RenderB  = NULL;
static PAO_DYN16_BLOCK g_pDyn16CaptureB = NULL;

static VOID AoFree16chBlock(_Inout_ PAO_DYN16_BLOCK *ppBlock);
static NTSTATUS AoBuild16chBlock(_In_ BOOLEAN IsRender, _Out_ PAO_DYN16_BLOCK *ppBlock);

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
static VOID AoReadRegistryConfig(_Out_ PULONG pInternalRate, _Out_ PULONG pMaxLatencyMs, _Out_ PULONG pMaxChannelCount);
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
    // Free any remaining dynamic 16ch blocks (belt-and-suspenders).
    //
#if defined(CABLE_A)
    AoFree16chBlock(&g_pDyn16RenderA);
    AoFree16chBlock(&g_pDyn16CaptureA);
#elif defined(CABLE_B)
    AoFree16chBlock(&g_pDyn16RenderB);
    AoFree16chBlock(&g_pDyn16CaptureB);
#else
    AoFree16chBlock(&g_pDyn16RenderA);
    AoFree16chBlock(&g_pDyn16CaptureA);
    AoFree16chBlock(&g_pDyn16RenderB);
    AoFree16chBlock(&g_pDyn16CaptureB);
#endif

    //
    // Phase 6 Step 1: tear down the shared transport engine before freeing
    // the pipes. The engine's timer must be fully stopped and any in-flight
    // DPC drained (ExDeleteTimer Wait=TRUE) before any stream runtime that
    // the callback may have referenced is freed.
    //
    AoTransportEngineCleanup();

    //
    // Cleanup loopback buffers.
    //
#if defined(CABLE_A)
    LoopbackCleanup(&g_CableALoopback);
    FramePipeCleanup(&g_CableAPipe);
#elif defined(CABLE_B)
    LoopbackCleanup(&g_CableBLoopback);
    FramePipeCleanup(&g_CableBPipe);
#else
    LoopbackCleanup(&g_CableALoopback);
    LoopbackCleanup(&g_CableBLoopback);
    FramePipeCleanup(&g_CableAPipe);
    FramePipeCleanup(&g_CableBPipe);
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
    // Open registry Parameters key for persistent settings.
    // Must be done here (system thread context) so Zw* calls bypass access checks.
    // Opened BEFORE LoopbackInit so channel count can be read first.
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
    // Read saved registry settings (rate, latency, channel count).
    //
    ULONG savedRate = LB_DEFAULT_INTERNAL_RATE;
    ULONG savedLatency = LB_DEFAULT_LATENCY_MS;
    ULONG savedChannels = LB_INTERNAL_CHANNELS;
    AoReadRegistryConfig(&savedRate, &savedLatency, &savedChannels);
    DPF(D_TERSE, ("[DriverEntry] Registry config: Rate=%u, Latency=%ums, Channels=%u",
                   savedRate, savedLatency, savedChannels));

    //
    // Initialize loopback buffers with configured channel count.
    //
#if defined(CABLE_A)
    ntStatus = LoopbackInit(&g_CableALoopback, savedChannels);
    IF_FAILED_ACTION_JUMP(
        ntStatus,
        DPF(D_ERROR, ("LoopbackInit CableA failed, 0x%x", ntStatus)),
        Done);
#elif defined(CABLE_B)
    ntStatus = LoopbackInit(&g_CableBLoopback, savedChannels);
    IF_FAILED_ACTION_JUMP(
        ntStatus,
        DPF(D_ERROR, ("LoopbackInit CableB failed, 0x%x", ntStatus)),
        Done);
#else
    ntStatus = LoopbackInit(&g_CableALoopback, savedChannels);
    IF_FAILED_ACTION_JUMP(
        ntStatus,
        DPF(D_ERROR, ("LoopbackInit CableA failed, 0x%x", ntStatus)),
        Done);

    ntStatus = LoopbackInit(&g_CableBLoopback, savedChannels);
    IF_FAILED_ACTION_JUMP(
        ntStatus,
        DPF(D_ERROR, ("LoopbackInit CableB failed, 0x%x", ntStatus)),
        Done);
#endif

    //
    // Apply saved rate and latency settings.
    //
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

    //
    // Initialize FRAME_PIPE (Phase 2: coexists with LOOPBACK_BUFFER).
    // TargetFillFrames = (latencyMs * rate) / 1000
    //
    {
        // Large capacity to survive Speaker STOP/RUN gaps (2-3 seconds typical).
        // TargetFill=96000 (~2s @ 48kHz), Capacity=192000 (~4s).
        // Memory: 192000 * 8ch * 4bytes = ~6MB per pipe. Acceptable for audio driver.
        ULONG targetFill = 96000;

#if defined(CABLE_A)
        ntStatus = FramePipeInit(&g_CableAPipe, savedRate, savedChannels, targetFill);
        IF_FAILED_ACTION_JUMP(
            ntStatus,
            DPF(D_ERROR, ("FramePipeInit CableA failed, 0x%x", ntStatus)),
            Done);
#elif defined(CABLE_B)
        ntStatus = FramePipeInit(&g_CableBPipe, savedRate, savedChannels, targetFill);
        IF_FAILED_ACTION_JUMP(
            ntStatus,
            DPF(D_ERROR, ("FramePipeInit CableB failed, 0x%x", ntStatus)),
            Done);
#else
        ntStatus = FramePipeInit(&g_CableAPipe, savedRate, savedChannels, targetFill);
        IF_FAILED_ACTION_JUMP(
            ntStatus,
            DPF(D_ERROR, ("FramePipeInit CableA failed, 0x%x", ntStatus)),
            Done);

        ntStatus = FramePipeInit(&g_CableBPipe, savedRate, savedChannels, targetFill);
        IF_FAILED_ACTION_JUMP(
            ntStatus,
            DPF(D_ERROR, ("FramePipeInit CableB failed, 0x%x", ntStatus)),
            Done);
#endif
    }

    //
    // Phase 6 Step 1: initialize the shared transport engine. The engine
    // owns the global 20 ms event timer. Step 1 wires the skeleton only —
    // the timer callback is a no-op and no data movement is switched onto
    // the engine yet. See docs/PHASE6_PLAN.md §8.
    //
    ntStatus = AoTransportEngineInit();
    IF_FAILED_ACTION_JUMP(
        ntStatus,
        DPF(D_ERROR, ("AoTransportEngineInit failed, 0x%x", ntStatus)),
        Done);

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

        // Phase 6: engine cleanup is idempotent and safe to call even if
        // AoTransportEngineInit never ran (guarded by Initialized flag).
        AoTransportEngineCleanup();

#if defined(CABLE_A)
        LoopbackCleanup(&g_CableALoopback);
        FramePipeCleanup(&g_CableAPipe);
#elif defined(CABLE_B)
        LoopbackCleanup(&g_CableBLoopback);
        FramePipeCleanup(&g_CableBPipe);
#else
        LoopbackCleanup(&g_CableALoopback);
        LoopbackCleanup(&g_CableBLoopback);
        FramePipeCleanup(&g_CableAPipe);
        FramePipeCleanup(&g_CableBPipe);
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
    _In_ PENDPOINT_MINIPAIR _pAeMiniports,
    _In_opt_ PVOID          _pDeviceContext = NULL
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
        _pDeviceContext,
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
    _In_ PENDPOINT_MINIPAIR _pAeMiniports,
    _In_opt_ PVOID          _pDeviceContext = NULL
)
{
    NTSTATUS    ntStatus = STATUS_SUCCESS;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(_pDeviceObject);

    ntStatus = _pAdapterCommon->InstallEndpointFilters(
        _pIrp,
        _pAeMiniports,
        _pDeviceContext,
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
// Dynamic 16ch format block: free
//=============================================================================
#pragma code_seg("PAGE")
static
VOID
AoFree16chBlock(
    _Inout_ PAO_DYN16_BLOCK *ppBlock
)
{
    PAGED_CODE();
    if (ppBlock && *ppBlock)
    {
        ExFreePoolWithTag(*ppBlock, AO_DYN16_POOLTAG);
        *ppBlock = NULL;
    }
}

//=============================================================================
// Dynamic 16ch format block: runtime helper to fill one WAVEFORMATEXTENSIBLE
//=============================================================================
#pragma code_seg("PAGE")
static
VOID
AoFillWaveFormat(
    _Out_ PKSDATAFORMAT_WAVEFORMATEXTENSIBLE pFmt,
    _In_  ULONG   SampleRate,
    _In_  USHORT  BitsPerSample,
    _In_  USHORT  Channels,
    _In_  ULONG   ChannelMask,
    _In_  BOOLEAN IsFloat
)
{
    PAGED_CODE();

    RtlZeroMemory(pFmt, sizeof(*pFmt));

    pFmt->DataFormat.FormatSize  = sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE);
    pFmt->DataFormat.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
    pFmt->DataFormat.SubFormat   = IsFloat ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
                                           : KSDATAFORMAT_SUBTYPE_PCM;
    pFmt->DataFormat.Specifier   = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;

    USHORT blockAlign = (USHORT)(Channels * BitsPerSample / 8);

    pFmt->WaveFormatExt.Format.wFormatTag      = WAVE_FORMAT_EXTENSIBLE;
    pFmt->WaveFormatExt.Format.nChannels       = Channels;
    pFmt->WaveFormatExt.Format.nSamplesPerSec  = SampleRate;
    pFmt->WaveFormatExt.Format.nAvgBytesPerSec = SampleRate * blockAlign;
    pFmt->WaveFormatExt.Format.nBlockAlign     = blockAlign;
    pFmt->WaveFormatExt.Format.wBitsPerSample  = BitsPerSample;
    pFmt->WaveFormatExt.Format.cbSize          =
        sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

    pFmt->WaveFormatExt.Samples.wValidBitsPerSample = BitsPerSample;
    pFmt->WaveFormatExt.dwChannelMask               = ChannelMask;
    pFmt->WaveFormatExt.SubFormat = IsFloat ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
                                            : KSDATAFORMAT_SUBTYPE_PCM;
}

//=============================================================================
// Dynamic 16ch format block: build
//   Allocates a single AO_DYN16_BLOCK containing all PortCls tables needed
//   to advertise 16-channel formats. Called at StartDevice (PASSIVE_LEVEL).
//=============================================================================
#pragma code_seg("PAGE")
static
NTSTATUS
AoBuild16chBlock(
    _In_  BOOLEAN         IsRender,
    _Out_ PAO_DYN16_BLOCK *ppBlock
)
{
    PAGED_CODE();
    ASSERT(ppBlock);

    *ppBlock = NULL;

    PAO_DYN16_BLOCK pB = (PAO_DYN16_BLOCK)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, sizeof(AO_DYN16_BLOCK), AO_DYN16_POOLTAG);
    if (!pB)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(pB, sizeof(*pB));

    //
    // 1. WaveFormats: copy 108 base 8ch formats, then build 12 16ch entries.
    //
    C_ASSERT(SIZEOF_ARRAY(CableHostPinSupportedDeviceFormats) == CABLE_8CH_FORMAT_COUNT);
    RtlCopyMemory(pB->WaveFormats,
                   CableHostPinSupportedDeviceFormats,
                   sizeof(CableHostPinSupportedDeviceFormats));

    // 16ch direct-out: 4 rates x 2 PCM depths + 4 rates x 1 FLOAT = 12 entries
    static const ULONG s_16chRates[] = { 48000, 44100, 96000, 192000 };
    ULONG idx = CABLE_8CH_FORMAT_COUNT;
    for (ULONG r = 0; r < ARRAYSIZE(s_16chRates); r++)
    {
        AoFillWaveFormat(&pB->WaveFormats[idx++], s_16chRates[r], 16, 16,
                         KSAUDIO_SPEAKER_DIRECTOUT, FALSE);
        AoFillWaveFormat(&pB->WaveFormats[idx++], s_16chRates[r], 24, 16,
                         KSAUDIO_SPEAKER_DIRECTOUT, FALSE);
    }
    for (ULONG r = 0; r < ARRAYSIZE(s_16chRates); r++)
    {
        AoFillWaveFormat(&pB->WaveFormats[idx++], s_16chRates[r], 32, 16,
                         KSAUDIO_SPEAKER_DIRECTOUT, TRUE);
    }
    ASSERT(idx == CABLE_16CH_TOTAL_COUNT);

    //
    // 2. DataRanges: copy from static 8ch ranges, patch MaximumChannels = 16.
    //
    RtlCopyMemory(pB->DataRanges, CablePinDataRangesStream, sizeof(CablePinDataRangesStream));
    pB->DataRanges[0].MaximumChannels = 16;  // PCM
    pB->DataRanges[1].MaximumChannels = 16;  // FLOAT

    //
    // 3. DataRangePointers: PCM, attributes, FLOAT, attributes.
    //
    pB->DataRangePointers[0] = PKSDATARANGE(&pB->DataRanges[0]);
    pB->DataRangePointers[1] = PKSDATARANGE(&PinDataRangeAttributeList);
    pB->DataRangePointers[2] = PKSDATARANGE(&pB->DataRanges[1]);
    pB->DataRangePointers[3] = PKSDATARANGE(&PinDataRangeAttributeList);

    //
    // 4. Mode: RAW, default format = first wave format.
    //
    pB->Mode.Mode           = AUDIO_SIGNALPROCESSINGMODE_RAW;
    pB->Mode.DefaultFormat  = &pB->WaveFormats[0].DataFormat;

    //
    // 5. PinFmtModes: streaming pin + bridge pin (order depends on render/capture).
    //
    PIN_DEVICE_FORMATS_AND_MODES streamingFmt = {
        IsRender ? SystemRenderPin : SystemCapturePin,
        pB->WaveFormats,
        CABLE_16CH_TOTAL_COUNT,
        &pB->Mode,
        1
    };
    PIN_DEVICE_FORMATS_AND_MODES bridgeFmt = { BridgePin, NULL, 0, NULL, 0 };

    if (IsRender)
    {
        pB->PinFmtModes[0] = streamingFmt;   // Pin 0 = system render
        pB->PinFmtModes[1] = bridgeFmt;      // Pin 1 = bridge
    }
    else
    {
        pB->PinFmtModes[0] = bridgeFmt;      // Pin 0 = bridge
        pB->PinFmtModes[1] = streamingFmt;   // Pin 1 = system capture
    }

    //
    // 6. Pins: copy from static 8ch pins, patch streaming pin's data ranges.
    //
    if (IsRender)
    {
        RtlCopyMemory(pB->Pins, CableRenderWaveMiniportPins,
                       sizeof(CableRenderWaveMiniportPins));
        // Pin 0 = streaming: patch data range pointers
        pB->Pins[0].KsPinDescriptor.DataRangesCount = ARRAYSIZE(pB->DataRangePointers);
        pB->Pins[0].KsPinDescriptor.DataRanges      = pB->DataRangePointers;
    }
    else
    {
        RtlCopyMemory(pB->Pins, CableCaptureWaveMiniportPins,
                       sizeof(CableCaptureWaveMiniportPins));
        // Pin 1 = streaming: patch data range pointers
        pB->Pins[1].KsPinDescriptor.DataRangesCount = ARRAYSIZE(pB->DataRangePointers);
        pB->Pins[1].KsPinDescriptor.DataRanges      = pB->DataRangePointers;
    }

    //
    // 7. FilterDesc: reuse static automation table, nodes, connections.
    //
    pB->FilterDesc.Version         = 0;
    pB->FilterDesc.PinSize         = sizeof(PCPIN_DESCRIPTOR);
    pB->FilterDesc.PinCount        = 2;
    pB->FilterDesc.Pins            = pB->Pins;
    pB->FilterDesc.NodeSize        = sizeof(PCNODE_DESCRIPTOR);
    pB->FilterDesc.CategoryCount   = 0;
    pB->FilterDesc.Categories      = NULL;

    if (IsRender)
    {
        pB->FilterDesc.AutomationTable = &AutomationCableRenderWaveFilter;
        pB->FilterDesc.NodeCount       = 0;
        pB->FilterDesc.Nodes           = NULL;
        pB->FilterDesc.ConnectionCount = SIZEOF_ARRAY(CableRenderWaveMiniportConnections);
        pB->FilterDesc.Connections     = CableRenderWaveMiniportConnections;
    }
    else
    {
        pB->FilterDesc.AutomationTable = &AutomationCableCaptureWaveFilter;
        pB->FilterDesc.NodeCount       = SIZEOF_ARRAY(CableCaptureWaveMiniportNodes);
        pB->FilterDesc.Nodes           = CableCaptureWaveMiniportNodes;
        pB->FilterDesc.ConnectionCount = SIZEOF_ARRAY(CableCaptureWaveMiniportConnections);
        pB->FilterDesc.Connections     = CableCaptureWaveMiniportConnections;
    }

    //
    // 8. Binding: wire up for CMiniportWaveRT constructor consumption.
    //
    pB->Binding.DeviceMaxChannels           = 16;
    pB->Binding.WaveFilterDescriptor        = &pB->FilterDesc;
    pB->Binding.PinDeviceFormatsAndModes    = pB->PinFmtModes;
    pB->Binding.PinDeviceFormatsAndModesCount = 2;

    *ppBlock = pB;
    return STATUS_SUCCESS;
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
    // Re-read registry config on every StartDevice (covers device restart
    // after IOCTL_AO_SET_MAX_CHANNELS without a full DriverEntry cycle).
    // If MaxChannelCount changed, re-init the loopback with the new value.
    //
    {
        ULONG desiredRate = LB_DEFAULT_INTERNAL_RATE;
        ULONG desiredLatency = LB_DEFAULT_LATENCY_MS;
        ULONG desiredChannels = LB_INTERNAL_CHANNELS;
        AoReadRegistryConfig(&desiredRate, &desiredLatency, &desiredChannels);

#if defined(CABLE_A)
        if (desiredChannels != g_CableALoopback.InternalChannels)
        {
            DPF(D_TERSE, ("[StartDevice] Channel mode changed %u -> %u, re-init loopback A",
                           g_CableALoopback.InternalChannels, desiredChannels));
            LoopbackCleanup(&g_CableALoopback);
            ntStatus = LoopbackInit(&g_CableALoopback, desiredChannels);
            IF_FAILED_JUMP(ntStatus, Exit);
            LoopbackSetInternalRate(&g_CableALoopback, desiredRate);
            LoopbackResizeBuffer(&g_CableALoopback, desiredLatency);
        }
#elif defined(CABLE_B)
        if (desiredChannels != g_CableBLoopback.InternalChannels)
        {
            DPF(D_TERSE, ("[StartDevice] Channel mode changed %u -> %u, re-init loopback B",
                           g_CableBLoopback.InternalChannels, desiredChannels));
            LoopbackCleanup(&g_CableBLoopback);
            ntStatus = LoopbackInit(&g_CableBLoopback, desiredChannels);
            IF_FAILED_JUMP(ntStatus, Exit);
            LoopbackSetInternalRate(&g_CableBLoopback, desiredRate);
            LoopbackResizeBuffer(&g_CableBLoopback, desiredLatency);
        }
#else
        if (desiredChannels != g_CableALoopback.InternalChannels)
        {
            DPF(D_TERSE, ("[StartDevice] Channel mode changed %u -> %u, re-init loopback A",
                           g_CableALoopback.InternalChannels, desiredChannels));
            LoopbackCleanup(&g_CableALoopback);
            ntStatus = LoopbackInit(&g_CableALoopback, desiredChannels);
            IF_FAILED_JUMP(ntStatus, Exit);
            LoopbackSetInternalRate(&g_CableALoopback, desiredRate);
            LoopbackResizeBuffer(&g_CableALoopback, desiredLatency);
        }
        if (desiredChannels != g_CableBLoopback.InternalChannels)
        {
            DPF(D_TERSE, ("[StartDevice] Channel mode changed %u -> %u, re-init loopback B",
                           g_CableBLoopback.InternalChannels, desiredChannels));
            LoopbackCleanup(&g_CableBLoopback);
            ntStatus = LoopbackInit(&g_CableBLoopback, desiredChannels);
            IF_FAILED_JUMP(ntStatus, Exit);
            LoopbackSetInternalRate(&g_CableBLoopback, desiredRate);
            LoopbackResizeBuffer(&g_CableBLoopback, desiredLatency);
        }
#endif
    }

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
            ULONG ch = g_CableALoopback.InternalChannels;

            // Free any stale dynamic blocks from a previous StartDevice cycle
            AoFree16chBlock(&g_pDyn16RenderA);
            AoFree16chBlock(&g_pDyn16CaptureA);

            PAO_ENDPOINT_FORMAT_BINDING pRenderBinding  = &CableRenderBinding8ch;
            PAO_ENDPOINT_FORMAT_BINDING pCaptureBinding = &CableCaptureBinding8ch;

            if (ch == 16)
            {
                ntStatus = AoBuild16chBlock(TRUE, &g_pDyn16RenderA);
                IF_FAILED_JUMP(ntStatus, Exit);

                ntStatus = AoBuild16chBlock(FALSE, &g_pDyn16CaptureA);
                if (!NT_SUCCESS(ntStatus))
                {
                    AoFree16chBlock(&g_pDyn16RenderA);
                    goto Exit;
                }

                pRenderBinding  = &g_pDyn16RenderA->Binding;
                pCaptureBinding = &g_pDyn16CaptureA->Binding;
            }

            DPF(D_TERSE, ("[StartDevice] Hardware ID: AOCableA, Channels=%u, Binding=%uch",
                           ch, (ULONG)pRenderBinding->DeviceMaxChannels));

            ntStatus = InstallEndpointRenderFilters(DeviceObject, Irp, pAdapterCommon, &CableASpeakerMiniports, pRenderBinding);
            IF_FAILED_JUMP(ntStatus, Exit);
            ntStatus = InstallEndpointCaptureFilters(DeviceObject, Irp, pAdapterCommon, &CableAMicMiniports, pCaptureBinding);
            IF_FAILED_JUMP(ntStatus, Exit);
        }
        else if (wcsstr(hardwareId, L"AOCableB") != NULL)
        {
            ULONG ch = g_CableBLoopback.InternalChannels;

            // Free any stale dynamic blocks from a previous StartDevice cycle
            AoFree16chBlock(&g_pDyn16RenderB);
            AoFree16chBlock(&g_pDyn16CaptureB);

            PAO_ENDPOINT_FORMAT_BINDING pRenderBinding  = &CableRenderBinding8ch;
            PAO_ENDPOINT_FORMAT_BINDING pCaptureBinding = &CableCaptureBinding8ch;

            if (ch == 16)
            {
                ntStatus = AoBuild16chBlock(TRUE, &g_pDyn16RenderB);
                IF_FAILED_JUMP(ntStatus, Exit);

                ntStatus = AoBuild16chBlock(FALSE, &g_pDyn16CaptureB);
                if (!NT_SUCCESS(ntStatus))
                {
                    AoFree16chBlock(&g_pDyn16RenderB);
                    goto Exit;
                }

                pRenderBinding  = &g_pDyn16RenderB->Binding;
                pCaptureBinding = &g_pDyn16CaptureB->Binding;
            }

            DPF(D_TERSE, ("[StartDevice] Hardware ID: AOCableB, Channels=%u, Binding=%uch",
                           ch, (ULONG)pRenderBinding->DeviceMaxChannels));

            ntStatus = InstallEndpointRenderFilters(DeviceObject, Irp, pAdapterCommon, &CableBSpeakerMiniports, pRenderBinding);
            IF_FAILED_JUMP(ntStatus, Exit);
            ntStatus = InstallEndpointCaptureFilters(DeviceObject, Irp, pAdapterCommon, &CableBMicMiniports, pCaptureBinding);
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
    _Out_ PULONG pMaxLatencyMs,
    _Out_ PULONG pMaxChannelCount
)
{
    PAGED_CODE();

    *pInternalRate = LB_DEFAULT_INTERNAL_RATE;
    *pMaxLatencyMs = LB_DEFAULT_LATENCY_MS;
    *pMaxChannelCount = LB_INTERNAL_CHANNELS;

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

    // Read MaxChannelCount
    {
        UNICODE_STRING valName;
        RtlInitUnicodeString(&valName, AO_REG_MAX_CHANNELS);

        UCHAR buf[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONG)];
        ULONG resultLen = 0;
        NTSTATUS st = ZwQueryValueKey(hKey, &valName, KeyValuePartialInformation, buf, sizeof(buf), &resultLen);
        if (NT_SUCCESS(st))
        {
            PKEY_VALUE_PARTIAL_INFORMATION info = (PKEY_VALUE_PARTIAL_INFORMATION)buf;
            if (info->Type == REG_DWORD && info->DataLength == sizeof(ULONG))
            {
                ULONG val = *(PULONG)info->Data;
                if (val == 8 || val == 16)
                    *pMaxChannelCount = val;
            }
        }
    }

    // Do NOT close hKey -- it is the shared g_hParametersKey handle.
}

//=============================================================================
// AoSnapshotFramePipeDiag — atomic snapshot of FRAME_PIPE ring diagnostics
// under pipe->Lock. Phase 1 Step 6: deliberately scopes one new FRAME_PIPE
// field access in adapter.cpp to provide IOCTL_AO_GET_STREAM_STATUS with a
// lock-correct, mutually consistent five-tuple (Overflow / Underrun /
// UnderrunFlag / RingFill / WrapBound). Step 4 audit (§ 5) noted that
// adapter.cpp did not access FRAME_PIPE fields directly; this is the one
// permitted exception, contained inside this helper.
//
// Nonpaged code segment so the spinlock-bracketed section never resides in
// a pageable page when executing at DISPATCH_LEVEL. Caller may itself be
// pageable (PASSIVE_LEVEL) — control returns to caller after the spinlock
// is released, so caller stays at PASSIVE throughout.
//=============================================================================
#pragma code_seg()
static VOID
AoSnapshotFramePipeDiag(
    _In_  PFRAME_PIPE  pipe,
    _Out_ ULONG*       overflowCount,
    _Out_ ULONG*       underrunCount,
    _Out_ UCHAR*       underrunFlag,
    _Out_ ULONG*       ringFillFrames,
    _Out_ ULONG*       wrapBoundFrames)
{
    *overflowCount   = 0;
    *underrunCount   = 0;
    *underrunFlag    = 0;
    *ringFillFrames  = 0;
    *wrapBoundFrames = 0;

    if (!pipe || !pipe->Data || pipe->WrapBound <= 0)
        return;

    KIRQL oldIrql;
    KeAcquireSpinLock(&pipe->Lock, &oldIrql);

    LONG fill = pipe->WritePos - pipe->ReadPos;
    if (fill < 0)
        fill += pipe->WrapBound;

    *overflowCount   = (ULONG)pipe->OverflowCounter;
    *underrunCount   = (ULONG)pipe->UnderrunCounter;
    *underrunFlag    = pipe->UnderrunFlag;
    *ringFillFrames  = (ULONG)fill;
    *wrapBoundFrames = (ULONG)pipe->WrapBound;

    KeReleaseSpinLock(&pipe->Lock, oldIrql);
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
#if defined(CABLE_A)
        pConfig->InternalChannels = g_CableALoopback.InternalChannels;
#elif defined(CABLE_B)
        pConfig->InternalChannels = g_CableBLoopback.InternalChannels;
#else
        pConfig->InternalChannels = g_CableALoopback.InternalChannels;
#endif
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

    case IOCTL_AO_SET_MAX_CHANNELS:
    {
        // Writes MaxChannelCount to registry only. Takes effect after device restart.
        if (irpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(ULONG))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        ULONG newChannels = *(ULONG*)Irp->AssociatedIrp.SystemBuffer;
        if (newChannels != 8 && newChannels != 16)
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        AoWriteRegistryValue(AO_REG_MAX_CHANNELS, newChannels);
        status = STATUS_SUCCESS;
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

        // V2 diagnostic extension. AO_V2_DIAG has grown across phases
        // (P1=116 -> P5=132 -> P6=172 -> P7=220 bytes). To stay
        // backwards compatible with consumers that pass an older-tier
        // sized buffer, the handler does a partial write: it picks
        // the largest tier that fits in the caller's buffer, zeros
        // exactly that many bytes, fills only the fields inside that
        // tier, sets StructSize = chosen size, and returns
        // v2Offset + chosen size. A V1-only buffer (sizeof(AO_STREAM_STATUS))
        // gets bytesReturned = sizeof(AO_STREAM_STATUS).
        //
        // Tier sizes are derived from the documented layout in ioctl.h:
        //   P1 = StructSize (1) + 4 blocks * 7 ULONGs (28)         = 116
        //   P5 = P1 + 4 render-drive ULONGs                         = 132
        //   P6 = P5 + 2 cables * 5 ring-diag ULONG-equivs           = 172
        //   P7 = P6 + 4 streams * 3 shadow-counter ULONGs           = 220
        // sizeof(AO_V2_DIAG) is the current (P7) size and is the C_ASSERT
        // anchor up at the top of this TU.
        ULONG v2Offset       = sizeof(AO_STREAM_STATUS);
        ULONG outBufLen      = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
        const ULONG kP1Size  = 116;
        const ULONG kP5Size  = 132;
        const ULONG kP6Size  = 172;
        const ULONG kP7Size  = sizeof(AO_V2_DIAG);  // 220, guarded by C_ASSERT above

        ULONG diagWriteSize = 0;
        if      (outBufLen >= v2Offset + kP7Size) diagWriteSize = kP7Size;
        else if (outBufLen >= v2Offset + kP6Size) diagWriteSize = kP6Size;
        else if (outBufLen >= v2Offset + kP5Size) diagWriteSize = kP5Size;
        else if (outBufLen >= v2Offset + kP1Size) diagWriteSize = kP1Size;

        if (diagWriteSize > 0)
        {
            AO_V2_DIAG* pDiag = (AO_V2_DIAG*)((BYTE*)pStatus + v2Offset);
            RtlZeroMemory(pDiag, diagWriteSize);
            pDiag->StructSize = diagWriteSize;

            // P1 tier: legacy per-direction pump counter blocks. The
            // canonical V1 shape (DESIGN § 2.1) no longer carries these
            // source members, so they are zero-filled. RtlZeroMemory
            // above already cleared the whole region; the explicit
            // assignments document each removed-source field for
            // reviewers and are guaranteed to fit because P1 fields
            // sit entirely inside [0, kP1Size). Phase 6 cleanup retires
            // these schema fields entirely.
            pDiag->A_R_GatedSkipCount            = 0;
            pDiag->A_R_OverJumpCount             = 0;
            pDiag->A_R_FramesProcessedLow        = 0;
            pDiag->A_R_FramesProcessedHigh       = 0;
            pDiag->A_R_PumpInvocationCount       = 0;
            pDiag->A_R_PumpShadowDivergenceCount = 0;
            pDiag->A_R_PumpFeatureFlags          = 0;

            pDiag->A_C_GatedSkipCount            = 0;
            pDiag->A_C_OverJumpCount             = 0;
            pDiag->A_C_FramesProcessedLow        = 0;
            pDiag->A_C_FramesProcessedHigh       = 0;
            pDiag->A_C_PumpInvocationCount       = 0;
            pDiag->A_C_PumpShadowDivergenceCount = 0;
            pDiag->A_C_PumpFeatureFlags          = 0;

            pDiag->B_R_GatedSkipCount            = 0;
            pDiag->B_R_OverJumpCount             = 0;
            pDiag->B_R_FramesProcessedLow        = 0;
            pDiag->B_R_FramesProcessedHigh       = 0;
            pDiag->B_R_PumpInvocationCount       = 0;
            pDiag->B_R_PumpShadowDivergenceCount = 0;
            pDiag->B_R_PumpFeatureFlags          = 0;

            pDiag->B_C_GatedSkipCount            = 0;
            pDiag->B_C_OverJumpCount             = 0;
            pDiag->B_C_FramesProcessedLow        = 0;
            pDiag->B_C_FramesProcessedHigh       = 0;
            pDiag->B_C_PumpInvocationCount       = 0;
            pDiag->B_C_PumpShadowDivergenceCount = 0;
            pDiag->B_C_PumpFeatureFlags          = 0;

            if (diagWriteSize >= kP5Size)
            {
                // P5 tier: render-side drive counters. Source members
                // removed from FRAME_PIPE in Phase 1 Step 1; zero-filled
                // here for ABI continuity.
                pDiag->A_R_PumpDriveCount            = 0;
                pDiag->A_R_LegacyDriveCount          = 0;
                pDiag->B_R_PumpDriveCount            = 0;
                pDiag->B_R_LegacyDriveCount          = 0;
            }

            if (diagWriteSize >= kP6Size)
            {
                // P6 tier: per-cable canonical FRAME_PIPE ring diag.
                // AoSnapshotFramePipeDiag takes pipe->Lock for a single-
                // shot consistent five-tuple per cable. Reserved pad
                // bytes already zeroed above. UCHAR locals stay inside
                // the build guard so single-cable builds do not warn
                // on the unused variable for the absent cable.
#if defined(CABLE_A) || !defined(CABLE_B)
                {
                    UCHAR ufA = 0;
                    AoSnapshotFramePipeDiag(&g_CableAPipe,
                                            &pDiag->A_OverflowCount,
                                            &pDiag->A_UnderrunCount,
                                            &ufA,
                                            &pDiag->A_RingFillFrames,
                                            &pDiag->A_WrapBoundFrames);
                    pDiag->A_UnderrunFlag = ufA;
                }
#endif
#if defined(CABLE_B) || !defined(CABLE_A)
                {
                    UCHAR ufB = 0;
                    AoSnapshotFramePipeDiag(&g_CableBPipe,
                                            &pDiag->B_OverflowCount,
                                            &pDiag->B_UnderrunCount,
                                            &ufB,
                                            &pDiag->B_RingFillFrames,
                                            &pDiag->B_WrapBoundFrames);
                    pDiag->B_UnderrunFlag = ufB;
                }
#endif
            }

            if (diagWriteSize >= kP7Size)
            {
                // P7 tier: per-stream shadow helper counters.
                // AoTransportSnapshotShadowCounters takes the engine list
                // lock for ActiveStreams stability and reads each
                // AO_STREAM_RT's DbgShadow*Hits via no-op
                // InterlockedCompareExchange. Streams not currently
                // registered contribute zero (snapshot is zero-initialized
                // on entry). Counters aggregate per endpoint when more
                // than one active stream maps to the same (cable,
                // direction) slot -- see AoTransportSnapshotShadowCounters
                // body for rationale.
                AO_SHADOW_COUNTERS_SNAPSHOT shadow;
                AoTransportSnapshotShadowCounters(&shadow);

                pDiag->A_R_ShadowAdvanceHits = shadow.A_Render.ShadowAdvanceHits;
                pDiag->A_R_ShadowQueryHits   = shadow.A_Render.ShadowQueryHits;
                pDiag->A_R_ShadowTimerHits   = shadow.A_Render.ShadowTimerHits;

                pDiag->A_C_ShadowAdvanceHits = shadow.A_Capture.ShadowAdvanceHits;
                pDiag->A_C_ShadowQueryHits   = shadow.A_Capture.ShadowQueryHits;
                pDiag->A_C_ShadowTimerHits   = shadow.A_Capture.ShadowTimerHits;

                pDiag->B_R_ShadowAdvanceHits = shadow.B_Render.ShadowAdvanceHits;
                pDiag->B_R_ShadowQueryHits   = shadow.B_Render.ShadowQueryHits;
                pDiag->B_R_ShadowTimerHits   = shadow.B_Render.ShadowTimerHits;

                pDiag->B_C_ShadowAdvanceHits = shadow.B_Capture.ShadowAdvanceHits;
                pDiag->B_C_ShadowQueryHits   = shadow.B_Capture.ShadowQueryHits;
                pDiag->B_C_ShadowTimerHits   = shadow.B_Capture.ShadowTimerHits;
            }

            bytesReturned = v2Offset + diagWriteSize;
        }
        else
        {
            bytesReturned = sizeof(AO_STREAM_STATUS);
        }

        status = STATUS_SUCCESS;
        break;
    }

    case IOCTL_AO_PREPARE_UNLOAD:
    {
        //
        // Pre-upgrade quiesce sequence:
        //   1. Delete symbolic link immediately (prevents new user-mode opens)
        //   2. Set g_PrepareUnload flag (rejects any CREATE that races past symlink removal)
        //   3. If refcount is already 0, delete the device object now
        //   4. Otherwise, AoControlClose will delete it when the last handle closes
        //
        if (!InterlockedExchange(&g_SymlinkDeleted, TRUE))
        {
            UNICODE_STRING symLink = RTL_CONSTANT_STRING(AO_CONTROL_SYMLINK_NAME);
            IoDeleteSymbolicLink(&symLink);
            DPF(D_TERSE, ("[IOCTL_AO_PREPARE_UNLOAD] Symbolic link deleted"));
        }

        InterlockedExchange(&g_PrepareUnload, TRUE);

        LONG current = InterlockedCompareExchange(&g_ControlOpenCount, 0, 0);
        DPF(D_TERSE, ("[IOCTL_AO_PREPARE_UNLOAD] Quiesce armed, open handles: %ld", current));

        if (current <= 1)
        {
            // Only the caller's own handle remains (the one sending this IOCTL).
            // The device will be deleted when this handle closes via AoControlClose.
            DPF(D_TERSE, ("[IOCTL_AO_PREPARE_UNLOAD] Only caller handle remains - will delete on close"));
        }

        status = STATUS_SUCCESS;
        break;
    }

    case IOCTL_AO_SET_PUMP_FEATURE_FLAGS:
    {
        // Phase 5 (2026-04-14): runtime rollback knob for render
        // ownership. Write-only, mask-constrained to
        // AO_PUMP_FLAG_DISABLE_LEGACY_RENDER inside
        // AoPumpApplyRenderFlagMask(). Effect is visible at the next
        // PumpToCurrentPositionFromQuery() call (1-2 position queries).
        if (irpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof(AO_PUMP_FLAGS_REQ))
        {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        AO_PUMP_FLAGS_REQ* pReq = (AO_PUMP_FLAGS_REQ*)Irp->AssociatedIrp.SystemBuffer;
        ULONG setMask   = pReq->SetMask;
        ULONG clearMask = pReq->ClearMask;
#if defined(CABLE_A)
        status = AoPumpApplyRenderFlagMask(0, setMask, clearMask);
#elif defined(CABLE_B)
        status = AoPumpApplyRenderFlagMask(1, setMask, clearMask);
#else
        status = AoPumpApplyRenderFlagMask(0, setMask, clearMask);
        if (NT_SUCCESS(status))
            status = AoPumpApplyRenderFlagMask(1, setMask, clearMask);
#endif
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
        // Reject new opens during pre-upgrade quiesce
        if (InterlockedCompareExchange(&g_PrepareUnload, FALSE, FALSE))
        {
            Irp->IoStatus.Status = STATUS_DEVICE_NOT_READY;
            Irp->IoStatus.Information = 0;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_DEVICE_NOT_READY;
        }
        InterlockedIncrement(&g_ControlOpenCount);
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
        LONG remaining = InterlockedDecrement(&g_ControlOpenCount);
        DPF(D_TERSE, ("[AoControlClose] open count -> %ld", remaining));

        // If quiesce is active and all handles drained, delete control device
        if (remaining <= 0 && InterlockedCompareExchange(&g_PrepareUnload, FALSE, FALSE))
        {
            DPF(D_TERSE, ("[AoControlClose] All handles closed during quiesce - deleting control device"));
            // Symlink already deleted in PREPARE_UNLOAD; just delete the device
            if (g_ControlDevice)
            {
                IoDeleteDevice(g_ControlDevice);
                g_ControlDevice = NULL;
            }
        }

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
        // Delete symlink only if not already removed by PREPARE_UNLOAD
        if (!InterlockedExchange(&g_SymlinkDeleted, TRUE))
        {
            UNICODE_STRING symLink = RTL_CONSTANT_STRING(AO_CONTROL_SYMLINK_NAME);
            IoDeleteSymbolicLink(&symLink);
        }
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

        // Free dynamic 16ch blocks after subdevices are unregistered
        AoFree16chBlock(&g_pDyn16RenderA);
        AoFree16chBlock(&g_pDyn16CaptureA);
        AoFree16chBlock(&g_pDyn16RenderB);
        AoFree16chBlock(&g_pDyn16CaptureB);
        break;

    case IRP_MN_STOP_DEVICE:
        // Flush subdevice cache so restart loads fresh bindings
        // (e.g., after MaxChannelCount registry change + device restart).
        // Do NOT release adapter common - device may restart via IRP_MN_START_DEVICE.
        ext = static_cast<PortClassDeviceContext*>(_DeviceObject->DeviceExtension);
        if (ext->m_pCommon != NULL)
        {
            ext->m_pCommon->Cleanup();
        }

        // Free dynamic 16ch blocks after subdevices are unregistered
        AoFree16chBlock(&g_pDyn16RenderA);
        AoFree16chBlock(&g_pDyn16CaptureA);
        AoFree16chBlock(&g_pDyn16RenderB);
        AoFree16chBlock(&g_pDyn16CaptureB);
        break;

    default:
        break;
    }
    
    ntStatus = PcDispatchIrp(_DeviceObject, _Irp);

    return ntStatus;
}

#pragma code_seg()

