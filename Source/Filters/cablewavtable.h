/*++

Module Name:

    cablewavtable.h

Abstract:

    Declaration of wave miniport tables for Cable A/B endpoints.
    Multi-format: 44.1/48/96 kHz, 16/24-bit, stereo PCM.
    Default 48 kHz/16-bit matches Windows audio engine, eliminating resampling.
    Render and capture share the same format table so loopback is raw byte copy.
    NOTE: Speaker and Mic must use the same format for correct loopback.

--*/

#ifndef _VIRTUALAUDIODRIVER_CABLEWAVTABLE_H_
#define _VIRTUALAUDIODRIVER_CABLEWAVTABLE_H_

// Cable endpoints: stereo, multi-rate, multi-depth
#define CABLE_DEVICE_MAX_CHANNELS           2
#define CABLE_HOST_MAX_CHANNELS             2
#define CABLE_HOST_MIN_BITS_PER_SAMPLE      16
#define CABLE_HOST_MAX_BITS_PER_SAMPLE      24
#define CABLE_HOST_MIN_SAMPLE_RATE          44100
#define CABLE_HOST_MAX_SAMPLE_RATE          96000
#define CABLE_MAX_INPUT_STREAMS             1

//=============================================================================
// Supported device formats: 44.1/48/96 kHz x 16/24-bit x stereo PCM
// Index [0] = default (48 kHz / 16-bit / stereo)
//=============================================================================

// Helper: KSDATAFORMAT header for audio
#define CABLE_KSDATAFORMAT_AUDIO_HEADER \
    { \
        sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE), \
        0, 0, 0, \
        STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO), \
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM), \
        STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX) \
    }

static
KSDATAFORMAT_WAVEFORMATEXTENSIBLE CableHostPinSupportedDeviceFormats[] =
{
    // [0] 48000 Hz / 16-bit / stereo  (DEFAULT)
    {
        CABLE_KSDATAFORMAT_AUDIO_HEADER,
        {
            {
                WAVE_FORMAT_EXTENSIBLE,
                2,                              // nChannels
                48000,                          // nSamplesPerSec
                48000 * 2 * 16 / 8,             // nAvgBytesPerSec = 192000
                2 * 16 / 8,                     // nBlockAlign = 4
                16,                             // wBitsPerSample
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
            },
            16,                                 // wValidBitsPerSample
            KSAUDIO_SPEAKER_STEREO,
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
        }
    },
    // [1] 48000 Hz / 24-bit / stereo
    {
        CABLE_KSDATAFORMAT_AUDIO_HEADER,
        {
            {
                WAVE_FORMAT_EXTENSIBLE,
                2,
                48000,
                48000 * 2 * 24 / 8,             // nAvgBytesPerSec = 288000
                2 * 24 / 8,                     // nBlockAlign = 6
                24,
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
            },
            24,
            KSAUDIO_SPEAKER_STEREO,
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
        }
    },
    // [2] 44100 Hz / 16-bit / stereo
    {
        CABLE_KSDATAFORMAT_AUDIO_HEADER,
        {
            {
                WAVE_FORMAT_EXTENSIBLE,
                2,
                44100,
                44100 * 2 * 16 / 8,             // nAvgBytesPerSec = 176400
                2 * 16 / 8,                     // nBlockAlign = 4
                16,
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
            },
            16,
            KSAUDIO_SPEAKER_STEREO,
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
        }
    },
    // [3] 44100 Hz / 24-bit / stereo
    {
        CABLE_KSDATAFORMAT_AUDIO_HEADER,
        {
            {
                WAVE_FORMAT_EXTENSIBLE,
                2,
                44100,
                44100 * 2 * 24 / 8,             // nAvgBytesPerSec = 264600
                2 * 24 / 8,                     // nBlockAlign = 6
                24,
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
            },
            24,
            KSAUDIO_SPEAKER_STEREO,
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
        }
    },
    // [4] 96000 Hz / 16-bit / stereo
    {
        CABLE_KSDATAFORMAT_AUDIO_HEADER,
        {
            {
                WAVE_FORMAT_EXTENSIBLE,
                2,
                96000,
                96000 * 2 * 16 / 8,             // nAvgBytesPerSec = 384000
                2 * 16 / 8,                     // nBlockAlign = 4
                16,
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
            },
            16,
            KSAUDIO_SPEAKER_STEREO,
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
        }
    },
    // [5] 96000 Hz / 24-bit / stereo
    {
        CABLE_KSDATAFORMAT_AUDIO_HEADER,
        {
            {
                WAVE_FORMAT_EXTENSIBLE,
                2,
                96000,
                96000 * 2 * 24 / 8,             // nAvgBytesPerSec = 576000
                2 * 24 / 8,                     // nBlockAlign = 6
                24,
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
            },
            24,
            KSAUDIO_SPEAKER_STEREO,
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)
        }
    },
};

//=============================================================================
// Render modes (Cable Speaker - RAW mode, APO bypass)
//=============================================================================
static
MODE_AND_DEFAULT_FORMAT CableRenderPinSupportedDeviceModes[] =
{
    {
        STATIC_AUDIO_SIGNALPROCESSINGMODE_RAW,
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
// Data range: 48 kHz / 16-bit / stereo
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
        CABLE_HOST_MAX_CHANNELS,            // MaximumChannels = 2
        CABLE_HOST_BITS_PER_SAMPLE,         // MinimumBitsPerSample = 16
        CABLE_HOST_BITS_PER_SAMPLE,         // MaximumBitsPerSample = 16
        CABLE_HOST_SAMPLE_RATE,             // MinimumSampleFrequency = 48000
        CABLE_HOST_SAMPLE_RATE              // MaximumSampleFrequency = 48000
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
