/*++

Module Name:

    cablewavtable.h

Abstract:

    Declaration of wave miniport tables for Cable A/B endpoints.
    Single format: 16 kHz, 16-bit, mono PCM (Phone Link HFP).
    Render and capture share the same format so loopback is raw byte copy.

--*/

#ifndef _VIRTUALAUDIODRIVER_CABLEWAVTABLE_H_
#define _VIRTUALAUDIODRIVER_CABLEWAVTABLE_H_

// Cable endpoints: 16 kHz, 16-bit, mono
#define CABLE_DEVICE_MAX_CHANNELS           1
#define CABLE_HOST_MAX_CHANNELS             1
#define CABLE_HOST_BITS_PER_SAMPLE          16
#define CABLE_HOST_SAMPLE_RATE              16000
#define CABLE_MAX_INPUT_STREAMS             1

//=============================================================================
// Single supported device format: 16 kHz / 16-bit / mono PCM
//=============================================================================
static
KSDATAFORMAT_WAVEFORMATEXTENSIBLE CableHostPinSupportedDeviceFormats[] =
{
    {
        {
            sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE),
            0,
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        {
            {
                WAVE_FORMAT_EXTENSIBLE,
                1,                                      // nChannels = mono
                16000,                                  // nSamplesPerSec
                16000 * 1 * 16 / 8,                     // nAvgBytesPerSec = 32000
                1 * 16 / 8,                             // nBlockAlign = 2
                16,                                     // wBitsPerSample
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
            },
            16,                                         // wValidBitsPerSample
            KSAUDIO_SPEAKER_MONO,                               // dwChannelMask
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
        }
    }
};

//=============================================================================
// Render modes (Cable Speaker - DEFAULT mode)
//=============================================================================
static
MODE_AND_DEFAULT_FORMAT CableRenderPinSupportedDeviceModes[] =
{
    {
        STATIC_AUDIO_SIGNALPROCESSINGMODE_DEFAULT,
        &CableHostPinSupportedDeviceFormats[0].DataFormat
    }
};

//=============================================================================
// Capture modes (Cable Mic - RAW mode)
//=============================================================================
static
MODE_AND_DEFAULT_FORMAT CableCapturePinSupportedDeviceModes[] =
{
    {
        STATIC_AUDIO_SIGNALPROCESSINGMODE_RAW,
        &CableHostPinSupportedDeviceFormats[0].DataFormat
    }
};

//=============================================================================
// Render pin layout: Pin 0 = SystemRenderPin, Pin 1 = BridgePin
//=============================================================================
static
PIN_DEVICE_FORMATS_AND_MODES CableRenderPinDeviceFormatsAndModes[] =
{
    {
        SystemRenderPin,
        CableHostPinSupportedDeviceFormats,
        SIZEOF_ARRAY(CableHostPinSupportedDeviceFormats),
        CableRenderPinSupportedDeviceModes,
        SIZEOF_ARRAY(CableRenderPinSupportedDeviceModes)
    },
    {
        BridgePin,
        NULL,
        0,
        NULL,
        0
    }
};

//=============================================================================
// Capture pin layout: Pin 0 = BridgePin, Pin 1 = SystemCapturePin
//=============================================================================
static
PIN_DEVICE_FORMATS_AND_MODES CableCapturePinDeviceFormatsAndModes[] =
{
    {
        BridgePin,
        NULL,
        0,
        NULL,
        0
    },
    {
        SystemCapturePin,
        CableHostPinSupportedDeviceFormats,
        SIZEOF_ARRAY(CableHostPinSupportedDeviceFormats),
        CableCapturePinSupportedDeviceModes,
        SIZEOF_ARRAY(CableCapturePinSupportedDeviceModes)
    }
};

//=============================================================================
// Data range: 16 kHz / 16-bit / mono only
//=============================================================================
static
KSDATARANGE_AUDIO CablePinDataRangesStream[] =
{
    {
        {
            sizeof(KSDATARANGE_AUDIO),
            KSDATARANGE_ATTRIBUTES,
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        CABLE_HOST_MAX_CHANNELS,            // MaximumChannels = 1
        CABLE_HOST_BITS_PER_SAMPLE,         // MinimumBitsPerSample = 16
        CABLE_HOST_BITS_PER_SAMPLE,         // MaximumBitsPerSample = 16
        CABLE_HOST_SAMPLE_RATE,             // MinimumSampleFrequency = 16000
        CABLE_HOST_SAMPLE_RATE              // MaximumSampleFrequency = 16000
    }
};

static
PKSDATARANGE CablePinDataRangePointersStream[] =
{
    PKSDATARANGE(&CablePinDataRangesStream[0]),
    PKSDATARANGE(&PinDataRangeAttributeList),
};

//=============================================================================
// Bridge pin data range (analog)
//=============================================================================
static
KSDATARANGE CablePinDataRangesBridge[] =
{
    {
        sizeof(KSDATARANGE),
        0,
        0,
        0,
        STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
        STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
    }
};

static
PKSDATARANGE CablePinDataRangePointersBridge[] =
{
    &CablePinDataRangesBridge[0]
};

//=============================================================================
// Cable Render Wave Pins (same layout as Speaker: pin0=sink, pin1=source)
//=============================================================================
static
PCPIN_DESCRIPTOR CableRenderWaveMiniportPins[] =
{
    // Pin 0: KSPIN_WAVE_RENDER3_SINK_SYSTEM (host writes here)
    {
        CABLE_MAX_INPUT_STREAMS,
        CABLE_MAX_INPUT_STREAMS,
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(CablePinDataRangePointersStream),
            CablePinDataRangePointersStream,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
    // Pin 1: KSPIN_WAVE_RENDER3_SOURCE (bridge to topology)
    {
        0,
        0,
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(CablePinDataRangePointersBridge),
            CablePinDataRangePointersBridge,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_NONE,
            &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    }
};

//=============================================================================
// Cable Render Wave Connections (pass-through, no DSP nodes)
//=============================================================================
static
PCCONNECTION_DESCRIPTOR CableRenderWaveMiniportConnections[] =
{
    { PCFILTER_NODE, KSPIN_WAVE_RENDER3_SINK_SYSTEM, PCFILTER_NODE, KSPIN_WAVE_RENDER3_SOURCE }
};

//=============================================================================
// Cable Render Wave Filter properties
//=============================================================================
static
PCPROPERTY_ITEM PropertiesCableRenderWaveFilter[] =
{
    {
        &KSPROPSETID_Pin,
        KSPROPERTY_PIN_PROPOSEDATAFORMAT,
        KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_WaveFilter
    },
    {
        &KSPROPSETID_Pin,
        KSPROPERTY_PIN_PROPOSEDATAFORMAT2,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_WaveFilter
    }
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationCableRenderWaveFilter, PropertiesCableRenderWaveFilter);

//=============================================================================
// Cable Render Wave Filter Descriptor
//=============================================================================
static
PCFILTER_DESCRIPTOR CableRenderWaveMiniportFilterDescriptor =
{
    0,                                                      // Version
    &AutomationCableRenderWaveFilter,                       // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),                               // PinSize
    SIZEOF_ARRAY(CableRenderWaveMiniportPins),              // PinCount
    CableRenderWaveMiniportPins,                            // Pins
    sizeof(PCNODE_DESCRIPTOR),                              // NodeSize
    0,                                                      // NodeCount
    NULL,                                                   // Nodes
    SIZEOF_ARRAY(CableRenderWaveMiniportConnections),       // ConnectionCount
    CableRenderWaveMiniportConnections,                     // Connections
    0,                                                      // CategoryCount
    NULL                                                    // Categories
};

//=============================================================================
// Cable Capture Wave Pins (same layout as MicArray: pin0=bridge, pin1=host)
//=============================================================================
static
PCPIN_DESCRIPTOR CableCaptureWaveMiniportPins[] =
{
    // Pin 0: KSPIN_WAVE_BRIDGE (from topology)
    {
        0,
        0,
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(CablePinDataRangePointersBridge),
            CablePinDataRangePointersBridge,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_NONE,
            &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
    // Pin 1: KSPIN_WAVEIN_HOST (host reads from here)
    {
        CABLE_MAX_INPUT_STREAMS,
        CABLE_MAX_INPUT_STREAMS,
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(CablePinDataRangePointersStream),
            CablePinDataRangePointersStream,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            &KSAUDFNAME_RECORDING_CONTROL,
            0
        }
    }
};

//=============================================================================
// Cable Capture Wave Nodes (ADC)
//=============================================================================
static
PCNODE_DESCRIPTOR CableCaptureWaveMiniportNodes[] =
{
    // KSNODE_WAVE_ADC
    {
        0,
        NULL,
        &KSNODETYPE_ADC,
        NULL
    }
};

//=============================================================================
// Cable Capture Wave Connections (bridge -> ADC -> host)
//=============================================================================
static
PCCONNECTION_DESCRIPTOR CableCaptureWaveMiniportConnections[] =
{
    { PCFILTER_NODE,    KSPIN_WAVE_BRIDGE,  KSNODE_WAVE_ADC, 1 },
    { KSNODE_WAVE_ADC,  0,                  PCFILTER_NODE,   KSPIN_WAVEIN_HOST },
};

//=============================================================================
// Cable Capture Wave Filter properties
//=============================================================================
static
VIRTUALAUDIODRIVER_PROPERTY_ITEM PropertiesCableCaptureWaveFilter[] =
{
    {
        {
            &KSPROPSETID_General,
            KSPROPERTY_GENERAL_COMPONENTID,
            KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
            PropertyHandler_WaveFilter,
        },
        0, 0, NULL, NULL, NULL, NULL, 0
    },
    {
        {
            &KSPROPSETID_Pin,
            KSPROPERTY_PIN_PROPOSEDATAFORMAT,
            KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
            PropertyHandler_WaveFilter,
        },
        0, 0, NULL, NULL, NULL, NULL, 0
    },
    {
        {
            &KSPROPSETID_Pin,
            KSPROPERTY_PIN_PROPOSEDATAFORMAT2,
            KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
            PropertyHandler_WaveFilter,
        },
        0, 0, NULL, NULL, NULL, NULL, 0
    },
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationCableCaptureWaveFilter, PropertiesCableCaptureWaveFilter);

//=============================================================================
// Cable Capture Wave Filter Descriptor
//=============================================================================
static
PCFILTER_DESCRIPTOR CableCaptureWaveMiniportFilterDescriptor =
{
    0,                                                      // Version
    &AutomationCableCaptureWaveFilter,                      // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),                               // PinSize
    SIZEOF_ARRAY(CableCaptureWaveMiniportPins),             // PinCount
    CableCaptureWaveMiniportPins,                           // Pins
    sizeof(PCNODE_DESCRIPTOR),                              // NodeSize
    SIZEOF_ARRAY(CableCaptureWaveMiniportNodes),            // NodeCount
    CableCaptureWaveMiniportNodes,                          // Nodes
    SIZEOF_ARRAY(CableCaptureWaveMiniportConnections),      // ConnectionCount
    CableCaptureWaveMiniportConnections,                    // Connections
    0,                                                      // CategoryCount
    NULL                                                    // Categories
};

#endif // _VIRTUALAUDIODRIVER_CABLEWAVTABLE_H_
