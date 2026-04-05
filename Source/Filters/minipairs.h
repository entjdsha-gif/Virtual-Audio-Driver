/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    minipairs.h

Abstract:

    Local audio endpoint filter definitions.
--*/

#ifndef _VIRTUALAUDIODRIVER_MINIPAIRS_H_
#define _VIRTUALAUDIODRIVER_MINIPAIRS_H_

#include "speakertopo.h"
#include "speakertoptable.h"
#include "speakerwavtable.h"

#include "micarraytopo.h"
#include "micarray1toptable.h"
#include "micarraywavtable.h"
#include "cablewavtable.h"


NTSTATUS
CreateMiniportWaveRTVirtualAudioDriver
(
    _Out_       PUNKNOWN *,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN,
    _In_        POOL_FLAGS,
    _In_        PUNKNOWN,
    _In_opt_    PVOID,
    _In_        PENDPOINT_MINIPAIR
);

NTSTATUS
CreateMiniportTopologyVirtualAudioDriver
(
    _Out_       PUNKNOWN *,
    _In_        REFCLSID,
    _In_opt_    PUNKNOWN,
    _In_        POOL_FLAGS,
    _In_        PUNKNOWN,
    _In_opt_    PVOID,
    _In_        PENDPOINT_MINIPAIR
);

//
// Render miniports.
//

/*********************************************************************
* Topology/Wave bridge connection for speaker (internal)             *
*                                                                    *
*              +------+                +------+                      *
*              | Wave |                | Topo |                      *
*              |      |                |      |                      *
* System   --->|0    1|--------------->|0    1|---> Line Out         *
*              |      |                |      |                      *
*              +------+                +------+                      *
*********************************************************************/
static
PHYSICALCONNECTIONTABLE SpeakerTopologyPhysicalConnections[] =
{
    {
        KSPIN_TOPO_WAVEOUT_SOURCE,  // TopologyIn
        KSPIN_WAVE_RENDER3_SOURCE,   // WaveOut
        CONNECTIONTYPE_WAVE_OUTPUT
    }
};

static
ENDPOINT_MINIPAIR SpeakerMiniports =
{
    eSpeakerDevice,
    L"TopologySpeaker",                                     // make sure this or the template name matches with KSNAME_TopologySpeaker in the inf's [Strings] section
    NULL,                                                   // optional template name
    CreateMiniportTopologyVirtualAudioDriver,
    &SpeakerTopoMiniportFilterDescriptor,
    0, NULL,                                                // Interface properties
    L"WaveSpeaker",                                         // make sure this or the template name matches with KSNAME_WaveSpeaker in the inf's [Strings] section
    NULL,                                                   // optional template name
    CreateMiniportWaveRTVirtualAudioDriver,
    &SpeakerWaveMiniportFilterDescriptor,
    0,                                                      // Interface properties
    NULL,
    SPEAKER_DEVICE_MAX_CHANNELS,
    SpeakerPinDeviceFormatsAndModes,
    SIZEOF_ARRAY(SpeakerPinDeviceFormatsAndModes),
    SpeakerTopologyPhysicalConnections,
    SIZEOF_ARRAY(SpeakerTopologyPhysicalConnections),
    ENDPOINT_NO_FLAGS,
};

//
// Capture miniports.
//

/*********************************************************************
* Topology/Wave bridge connection for mic array  1 (front)           *
*                                                                    *
*              +------+    +------+                                  *
*              | Topo |    | Wave |                                  *
*              |      |    |      |                                  *
*  Mic in  --->|0    1|===>|0    1|---> Capture Host Pin             *
*              |      |    |      |                                  *
*              +------+    +------+                                  *
*********************************************************************/
static
PHYSICALCONNECTIONTABLE MicArray1TopologyPhysicalConnections[] =
{
    {
        KSPIN_TOPO_BRIDGE,          // TopologyOut
        KSPIN_WAVE_BRIDGE,          // WaveIn
        CONNECTIONTYPE_TOPOLOGY_OUTPUT
    }
};

static
ENDPOINT_MINIPAIR MicArray1Miniports =
{
    eMicArrayDevice1,
    L"TopologyMicArray1",                   // make sure this or the template name matches with KSNAME_TopologyMicArray1 in the inf's [Strings] section
    NULL,                                   // optional template name
    CreateMicArrayMiniportTopology,
    &MicArray1TopoMiniportFilterDescriptor,
    0, NULL,                                // Interface properties
    L"WaveMicArray1",                       // make sure this or the tempalte name matches with KSNAME_WaveMicArray1 in the inf's [Strings] section
    NULL,                                   // optional template name
    CreateMiniportWaveRTVirtualAudioDriver,
    &MicArrayWaveMiniportFilterDescriptor,
    0,                                      // Interface properties
    NULL,
    MICARRAY_DEVICE_MAX_CHANNELS,
    MicArrayPinDeviceFormatsAndModes,
    SIZEOF_ARRAY(MicArrayPinDeviceFormatsAndModes),
    MicArray1TopologyPhysicalConnections,
    SIZEOF_ARRAY(MicArray1TopologyPhysicalConnections),
    ENDPOINT_NO_FLAGS,
};


//=============================================================================
//
// Cable A/B virtual audio cable endpoints
//
// Cable A: Phone Link outputs to "speaker" -> server captures from "mic" (caller voice)
// Cable B: Server outputs to "speaker" -> Phone Link captures from "mic" (AI response)
//
// Each Cable consists of a render (Speaker) + capture (Mic) pair.
// FilterDescriptors reuse existing Speaker/MicArray topology structures.
// (Same volume/mute nodes and pin layout, no need to create separate ones)
//
//=============================================================================

//-----------------------------------------------------------------------------
// Cable A Speaker ()  Phone Link 
//-----------------------------------------------------------------------------

/*********************************************************************
* Cable A Speaker:  PhysicalConnections                       *
* Wave (SOURCE) Topology (WAVEOUT_SOURCE)    *
*                                                                    *
*              +------+                +------+                      *
*              | Wave |                | Topo |                      *
*              |      |                |      |                      *
* System   --->|0    1|--------------->|0    1|---> Cable A Out      *
*              |      |                |      |                      *
*              +------+                +------+                      *
*********************************************************************/
static
PHYSICALCONNECTIONTABLE CableASpeakerTopologyPhysicalConnections[] =
{
    {
        KSPIN_TOPO_WAVEOUT_SOURCE,  // Topology  Wave 
        KSPIN_WAVE_RENDER3_SOURCE,  // Wave  Topology 
        CONNECTIONTYPE_WAVE_OUTPUT  // Wave -> Topology 
    }
};

static
ENDPOINT_MINIPAIR CableASpeakerMiniports =
{
    // DeviceType: common.h eDeviceType enum Cable A .
    // .
    eCableASpeaker,

    // TopoName: INF [Strings] KSNAME_TopologyCableASpeaker .
    // PnP .
    L"TopologyCableASpeaker",

    // TemplateTopoName: ( FilterDescriptor ).
    NULL,

    // TopoCreateCallback: .
    // Speaker  / .
    CreateMiniportTopologyVirtualAudioDriver,

    // TopoDescriptor: Cable A Speaker dedicated (unique pin Name GUID).
    &CableASpeakerTopoMiniportFilterDescriptor,

    0, NULL,

    // WaveName: INF [Strings] KSNAME_WaveCableASpeaker .
    // PnP .
    L"WaveCableASpeaker",

    // TemplateWaveName: ( FilterDescriptor ).
    NULL,

    // WaveCreateCallback: WaveRT .
    // Speaker WaveRT  DMA .
    CreateMiniportWaveRTVirtualAudioDriver,

    // WaveDescriptor: Cable dedicated render (16kHz/16bit/mono).
    &CableRenderWaveMiniportFilterDescriptor,

    0,
    NULL,

    // DeviceMaxChannels: mono for HFP.
    CABLE_DEVICE_MAX_CHANNELS,

    // PinDeviceFormatsAndModes: Cable render (16kHz/16bit/mono only).
    CableRenderPinDeviceFormatsAndModes,
    SIZEOF_ARRAY(CableRenderPinDeviceFormatsAndModes),

    CableASpeakerTopologyPhysicalConnections,
    SIZEOF_ARRAY(CableASpeakerTopologyPhysicalConnections),

    ENDPOINT_NO_FLAGS,
};

//-----------------------------------------------------------------------------
// Cable A Mic
//-----------------------------------------------------------------------------

/*********************************************************************
* Cable A Mic:  PhysicalConnections                           *
* Topology (BRIDGE) Wave (BRIDGE)            *
*                                                                    *
*              +------+    +------+                                  *
*              | Topo |    | Wave |                                  *
*              |      |    |      |                                  *
* Cable A  --->|0    1|===>|0    1|---> Capture Host Pin             *
*              |      |    |      |                                  *
*              +------+    +------+                                  *
*********************************************************************/
static
PHYSICALCONNECTIONTABLE CableAMicTopologyPhysicalConnections[] =
{
    {
        KSPIN_TOPO_BRIDGE,              // Topology  Wave 
        KSPIN_WAVE_BRIDGE,              // Wave  Topology 
        CONNECTIONTYPE_TOPOLOGY_OUTPUT  // Topology -> Wave 
    }
};

static
ENDPOINT_MINIPAIR CableAMicMiniports =
{
    // DeviceType: common.h eDeviceType enum Cable A .
    eCableAMic,

    // TopoName: INF [Strings] KSNAME_TopologyCableAMic .
    L"TopologyCableAMic",

    // TemplateTopoName: .
    NULL,

    // TopoCreateCallback: .
    // MicArray  ,
    // SNR, .
    CreateMicArrayMiniportTopology,

    // TopoDescriptor: Cable A Mic dedicated (unique pin Name GUID for separate WASAPI endpoint).
    &CableAMicTopoMiniportFilterDescriptor,

    // TopoInterfacePropertyCount, TopoInterfaceProperties: .
    0, NULL,

    // WaveName: INF [Strings] KSNAME_WaveCableAMic .
    L"WaveCableAMic",

    // TemplateWaveName: .
    NULL,

    // WaveCreateCallback: WaveRT .
    // WaveRT  DMA .
    CreateMiniportWaveRTVirtualAudioDriver,

    // WaveDescriptor: Cable dedicated capture (16kHz/16bit/mono).
    &CableCaptureWaveMiniportFilterDescriptor,

    0,
    NULL,

    // DeviceMaxChannels: mono for HFP.
    CABLE_DEVICE_MAX_CHANNELS,

    // PinDeviceFormatsAndModes: Cable capture (16kHz/16bit/mono only).
    CableCapturePinDeviceFormatsAndModes,
    SIZEOF_ARRAY(CableCapturePinDeviceFormatsAndModes),

    CableAMicTopologyPhysicalConnections,
    SIZEOF_ARRAY(CableAMicTopologyPhysicalConnections),

    ENDPOINT_NO_FLAGS,
};

//-----------------------------------------------------------------------------
// Cable B Speaker
//-----------------------------------------------------------------------------

/*********************************************************************
* Cable B Speaker:  PhysicalConnections                       *
* Cable A Speaker                                  *
*                                                                    *
*              +------+                +------+                      *
*              | Wave |                | Topo |                      *
*              |      |                |      |                      *
* System   --->|0    1|--------------->|0    1|---> Cable B Out      *
*              |      |                |      |                      *
*              +------+                +------+                      *
*********************************************************************/
static
PHYSICALCONNECTIONTABLE CableBSpeakerTopologyPhysicalConnections[] =
{
    {
        KSPIN_TOPO_WAVEOUT_SOURCE,  // Topology  Wave 
        KSPIN_WAVE_RENDER3_SOURCE,  // Wave  Topology 
        CONNECTIONTYPE_WAVE_OUTPUT  // Wave -> Topology 
    }
};

static
ENDPOINT_MINIPAIR CableBSpeakerMiniports =
{
    // DeviceType: Cable B .
    eCableBSpeaker,

    // TopoName: INF KSNAME_TopologyCableBSpeaker .
    L"TopologyCableBSpeaker",

    // TemplateTopoName: .
    NULL,

    // TopoCreateCallback: Speaker .
    CreateMiniportTopologyVirtualAudioDriver,

    // TopoDescriptor: Cable B Speaker dedicated (unique pin Name GUID).
    &CableBSpeakerTopoMiniportFilterDescriptor,

    0, NULL,

    // WaveName: INF KSNAME_WaveCableBSpeaker .
    L"WaveCableBSpeaker",

    // TemplateWaveName: .
    NULL,

    // WaveCreateCallback: Speaker WaveRT .
    CreateMiniportWaveRTVirtualAudioDriver,

    // WaveDescriptor: Cable dedicated render (16kHz/16bit/mono).
    &CableRenderWaveMiniportFilterDescriptor,

    0,
    NULL,

    // DeviceMaxChannels: mono for HFP.
    CABLE_DEVICE_MAX_CHANNELS,

    // PinDeviceFormatsAndModes: Cable render (16kHz/16bit/mono only).
    CableRenderPinDeviceFormatsAndModes,
    SIZEOF_ARRAY(CableRenderPinDeviceFormatsAndModes),

    CableBSpeakerTopologyPhysicalConnections,
    SIZEOF_ARRAY(CableBSpeakerTopologyPhysicalConnections),

    ENDPOINT_NO_FLAGS,
};

//-----------------------------------------------------------------------------
// Cable B Mic
//-----------------------------------------------------------------------------

/*********************************************************************
* Cable B Mic:  PhysicalConnections                           *
* Cable A Mic                                      *
*                                                                    *
*              +------+    +------+                                  *
*              | Topo |    | Wave |                                  *
*              |      |    |      |                                  *
* Cable B  --->|0    1|===>|0    1|---> Capture Host Pin             *
*              |      |    |      |                                  *
*              +------+    +------+                                  *
*********************************************************************/
static
PHYSICALCONNECTIONTABLE CableBMicTopologyPhysicalConnections[] =
{
    {
        KSPIN_TOPO_BRIDGE,              // Topology  Wave 
        KSPIN_WAVE_BRIDGE,              // Wave  Topology 
        CONNECTIONTYPE_TOPOLOGY_OUTPUT  // Topology -> Wave 
    }
};

static
ENDPOINT_MINIPAIR CableBMicMiniports =
{
    // DeviceType: Cable B .
    eCableBMic,

    // TopoName: INF KSNAME_TopologyCableBMic .
    L"TopologyCableBMic",

    // TemplateTopoName: .
    NULL,

    // TopoCreateCallback: MicArray  .
    CreateMicArrayMiniportTopology,

    // TopoDescriptor: Cable B Mic dedicated (unique pin Name GUID for separate WASAPI endpoint).
    &CableBMicTopoMiniportFilterDescriptor,

    // TopoInterfacePropertyCount, TopoInterfaceProperties: .
    0, NULL,

    // WaveName: INF KSNAME_WaveCableBMic .
    L"WaveCableBMic",

    // TemplateWaveName: .
    NULL,

    // WaveCreateCallback: WaveRT  .
    CreateMiniportWaveRTVirtualAudioDriver,

    // WaveDescriptor: Cable dedicated capture (16kHz/16bit/mono).
    &CableCaptureWaveMiniportFilterDescriptor,

    0,
    NULL,

    // DeviceMaxChannels: mono for HFP.
    CABLE_DEVICE_MAX_CHANNELS,

    // PinDeviceFormatsAndModes: Cable capture (16kHz/16bit/mono only).
    CableCapturePinDeviceFormatsAndModes,
    SIZEOF_ARRAY(CableCapturePinDeviceFormatsAndModes),

    CableBMicTopologyPhysicalConnections,
    SIZEOF_ARRAY(CableBMicTopologyPhysicalConnections),

    ENDPOINT_NO_FLAGS,
};


//=============================================================================
//
// Render miniport pairs. NOTE: the split of render and capture is arbitrary and
// unnessary, this array could contain capture endpoints.
//
static
PENDPOINT_MINIPAIR  g_RenderEndpoints[] =
{
    &SpeakerMiniports,
    &CableASpeakerMiniports,       // Cable A  Phone Link 
    &CableBSpeakerMiniports,       // Cable B  AI 
};

#define g_cRenderEndpoints  (SIZEOF_ARRAY(g_RenderEndpoints))

//=============================================================================
//
// Capture miniport pairs. NOTE: the split of render and capture is arbitrary and
// unnessary, this array could contain render endpoints.
//
static
PENDPOINT_MINIPAIR  g_CaptureEndpoints[] =
{
    &MicArray1Miniports,
    &CableAMicMiniports,           // Cable A  
    &CableBMicMiniports,           // Cable B  Phone Link AI 
};

#define g_cCaptureEndpoints (SIZEOF_ARRAY(g_CaptureEndpoints))

//=============================================================================
//
// Total miniports = # endpoints * 2 (topology + wave).
//
#define g_MaxMiniports  ((g_cRenderEndpoints + g_cCaptureEndpoints) * 2)

#endif // _VIRTUALAUDIODRIVER_MINIPAIRS_H_
