/*++

Module Name:

    cablewavtable.h

Abstract:

    Declaration of wave miniport tables for Cable A/B endpoints.
    Multi-format: 8k~192kHz, 16/24-bit, mono/stereo PCM (48 combinations).
    Default 48 kHz/16-bit/stereo matches Windows audio engine.
    Internal loopback uses format conversion engine for mismatched formats.

--*/

#ifndef _VIRTUALAUDIODRIVER_CABLEWAVTABLE_H_
#define _VIRTUALAUDIODRIVER_CABLEWAVTABLE_H_

// Cable endpoints: mono/stereo, multi-rate, multi-depth
#define CABLE_DEVICE_MAX_CHANNELS           2
#define CABLE_DEVICE_MIN_CHANNELS           1
#define CABLE_HOST_MAX_CHANNELS             2
#define CABLE_HOST_MIN_BITS_PER_SAMPLE      16
#define CABLE_HOST_MAX_BITS_PER_SAMPLE      24
#define CABLE_HOST_MIN_SAMPLE_RATE          8000
#define CABLE_HOST_MAX_SAMPLE_RATE          192000
#define CABLE_MAX_INPUT_STREAMS             1

//=============================================================================
// Supported device formats: 12 rates x 2 depths x 2 channels = 48 combos
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

// Helper macro: stereo format entry
#define CABLE_FMT_STEREO(rate, bits) \
    { \
        CABLE_KSDATAFORMAT_AUDIO_HEADER, \
        { \
            { \
                WAVE_FORMAT_EXTENSIBLE, \
                2, \
                (rate), \
                (rate) * 2 * (bits) / 8, \
                2 * (bits) / 8, \
                (WORD)(bits), \
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX) \
            }, \
            (WORD)(bits), \
            KSAUDIO_SPEAKER_STEREO, \
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM) \
        } \
    }

// Helper macro: mono format entry
#define CABLE_FMT_MONO(rate, bits) \
    { \
        CABLE_KSDATAFORMAT_AUDIO_HEADER, \
        { \
            { \
                WAVE_FORMAT_EXTENSIBLE, \
                1, \
                (rate), \
                (rate) * 1 * (bits) / 8, \
                1 * (bits) / 8, \
                (WORD)(bits), \
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX) \
            }, \
            (WORD)(bits), \
            KSAUDIO_SPEAKER_MONO, \
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM) \
        } \
    }

static
KSDATAFORMAT_WAVEFORMATEXTENSIBLE CableHostPinSupportedDeviceFormats[] =
{
    // === STEREO FORMATS (24 entries) ===
    //  [0] 48000/16/stereo (DEFAULT)
    CABLE_FMT_STEREO(48000, 16),
    //  [1] 48000/24/stereo
    CABLE_FMT_STEREO(48000, 24),
    //  [2] 44100/16/stereo
    CABLE_FMT_STEREO(44100, 16),
    //  [3] 44100/24/stereo
    CABLE_FMT_STEREO(44100, 24),
    //  [4] 96000/16/stereo
    CABLE_FMT_STEREO(96000, 16),
    //  [5] 96000/24/stereo
    CABLE_FMT_STEREO(96000, 24),
    //  [6] 88200/16/stereo
    CABLE_FMT_STEREO(88200, 16),
    //  [7] 88200/24/stereo
    CABLE_FMT_STEREO(88200, 24),
    //  [8] 192000/16/stereo
    CABLE_FMT_STEREO(192000, 16),
    //  [9] 192000/24/stereo
    CABLE_FMT_STEREO(192000, 24),
    // [10] 176400/16/stereo
    CABLE_FMT_STEREO(176400, 16),
    // [11] 176400/24/stereo
    CABLE_FMT_STEREO(176400, 24),
    // [12] 32000/16/stereo
    CABLE_FMT_STEREO(32000, 16),
    // [13] 32000/24/stereo
    CABLE_FMT_STEREO(32000, 24),
    // [14] 24000/16/stereo
    CABLE_FMT_STEREO(24000, 16),
    // [15] 24000/24/stereo
    CABLE_FMT_STEREO(24000, 24),
    // [16] 22050/16/stereo
    CABLE_FMT_STEREO(22050, 16),
    // [17] 22050/24/stereo
    CABLE_FMT_STEREO(22050, 24),
    // [18] 16000/16/stereo
    CABLE_FMT_STEREO(16000, 16),
    // [19] 16000/24/stereo
    CABLE_FMT_STEREO(16000, 24),
    // [20] 11025/16/stereo
    CABLE_FMT_STEREO(11025, 16),
    // [21] 11025/24/stereo
    CABLE_FMT_STEREO(11025, 24),
    // [22] 8000/16/stereo
    CABLE_FMT_STEREO(8000, 16),
    // [23] 8000/24/stereo
    CABLE_FMT_STEREO(8000, 24),

    // === MONO FORMATS (24 entries) ===
    // [24] 48000/16/mono
    CABLE_FMT_MONO(48000, 16),
    // [25] 48000/24/mono
    CABLE_FMT_MONO(48000, 24),
    // [26] 44100/16/mono
    CABLE_FMT_MONO(44100, 16),
    // [27] 44100/24/mono
    CABLE_FMT_MONO(44100, 24),
    // [28] 96000/16/mono
    CABLE_FMT_MONO(96000, 16),
    // [29] 96000/24/mono
    CABLE_FMT_MONO(96000, 24),
    // [30] 88200/16/mono
    CABLE_FMT_MONO(88200, 16),
    // [31] 88200/24/mono
    CABLE_FMT_MONO(88200, 24),
    // [32] 192000/16/mono
    CABLE_FMT_MONO(192000, 16),
    // [33] 192000/24/mono
    CABLE_FMT_MONO(192000, 24),
    // [34] 176400/16/mono
    CABLE_FMT_MONO(176400, 16),
    // [35] 176400/24/mono
    CABLE_FMT_MONO(176400, 24),
    // [36] 32000/16/mono
    CABLE_FMT_MONO(32000, 16),
    // [37] 32000/24/mono
    CABLE_FMT_MONO(32000, 24),
    // [38] 24000/16/mono
    CABLE_FMT_MONO(24000, 16),
    // [39] 24000/24/mono
    CABLE_FMT_MONO(24000, 24),
    // [40] 22050/16/mono
    CABLE_FMT_MONO(22050, 16),
    // [41] 22050/24/mono
    CABLE_FMT_MONO(22050, 24),
    // [42] 16000/16/mono
    CABLE_FMT_MONO(16000, 16),
    // [43] 16000/24/mono
    CABLE_FMT_MONO(16000, 24),
    // [44] 11025/16/mono
    CABLE_FMT_MONO(11025, 16),
    // [45] 11025/24/mono
    CABLE_FMT_MONO(11025, 24),
    // [46] 8000/16/mono
    CABLE_FMT_MONO(8000, 16),
    // [47] 8000/24/mono
    CABLE_FMT_MONO(8000, 24),
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
// Data range: full range (mono~stereo, 16~24bit, 8k~192kHz)
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
        CABLE_HOST_MIN_BITS_PER_SAMPLE,     // MinimumBitsPerSample = 16
        CABLE_HOST_MAX_BITS_PER_SAMPLE,     // MaximumBitsPerSample = 24
        CABLE_HOST_MIN_SAMPLE_RATE,         // MinimumSampleFrequency = 8000
        CABLE_HOST_MAX_SAMPLE_RATE          // MaximumSampleFrequency = 192000
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
