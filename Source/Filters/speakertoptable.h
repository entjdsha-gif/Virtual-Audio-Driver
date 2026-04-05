/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    speakertoptable.h

Abstract:

    Declaration of topology tables.
--*/

#ifndef _VIRTUALAUDIODRIVER_SPEAKERTOPTABLE_H_
#define _VIRTUALAUDIODRIVER_SPEAKERTOPTABLE_H_

//=============================================================================
static
KSDATARANGE SpeakerTopoPinDataRangesBridge[] =
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

//=============================================================================
static
PKSDATARANGE SpeakerTopoPinDataRangePointersBridge[] =
{
  &SpeakerTopoPinDataRangesBridge[0]
};

//=============================================================================
static
PCPIN_DESCRIPTOR SpeakerTopoMiniportPins[] =
{
  // KSPIN_TOPO_WAVEOUT_SOURCE
  {
    0,
    0,
    0,                                                  // InstanceCount
    NULL,                                               // AutomationTable
    {                                                   // KsPinDescriptor
      0,                                                // InterfacesCount
      NULL,                                             // Interfaces
      0,                                                // MediumsCount
      NULL,                                             // Mediums
      SIZEOF_ARRAY(SpeakerTopoPinDataRangePointersBridge),// DataRangesCount
      SpeakerTopoPinDataRangePointersBridge,            // DataRanges
      KSPIN_DATAFLOW_IN,                                // DataFlow
      KSPIN_COMMUNICATION_NONE,                         // Communication
      &KSCATEGORY_AUDIO,                                // Category
      NULL,                                             // Name
      0                                                 // Reserved
    }
  },
  // KSPIN_TOPO_LINEOUT_DEST
  {
    0,
    0,
    0,                                                  // InstanceCount
    NULL,                                               // AutomationTable
    {                                                   // KsPinDescriptor
      0,                                                // InterfacesCount
      NULL,                                             // Interfaces
      0,                                                // MediumsCount
      NULL,                                             // Mediums
      SIZEOF_ARRAY(SpeakerTopoPinDataRangePointersBridge),// DataRangesCount
      SpeakerTopoPinDataRangePointersBridge,            // DataRanges
      KSPIN_DATAFLOW_OUT,                               // DataFlow
      KSPIN_COMMUNICATION_NONE,                         // Communication
      &KSNODETYPE_SPEAKER,                              // Category
      NULL,                                             // Name
      0                                                 // Reserved
    }
  }
};

//=============================================================================
static
KSJACK_DESCRIPTION SpeakerJackDescBridge =
{
    KSAUDIO_SPEAKER_STEREO,
    JACKDESC_RGB(0xB3,0xC9,0x8C),              // Color spec for green
    eConnTypeUnknown,
    eGeoLocFront,
    eGenLocPrimaryBox,
    ePortConnIntegratedDevice,
    TRUE
};

// Only return a KSJACK_DESCRIPTION for the physical bridge pin.
static 
PKSJACK_DESCRIPTION SpeakerJackDescriptions[] =
{
    NULL,
    &SpeakerJackDescBridge
};

//=============================================================================
static
PCPROPERTY_ITEM SpeakerPropertiesVolume[] =
{
    {
    &KSPROPSETID_Audio,
    KSPROPERTY_AUDIO_VOLUMELEVEL,
    KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
    PropertyHandler_SpeakerTopology
    }
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationSpeakerVolume, SpeakerPropertiesVolume);

//=============================================================================
static
PCPROPERTY_ITEM SpeakerPropertiesMute[] =
{
  {
    &KSPROPSETID_Audio,
    KSPROPERTY_AUDIO_MUTE,
    KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
    PropertyHandler_SpeakerTopology
  }
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationSpeakerMute, SpeakerPropertiesMute);

//=============================================================================
static
PCNODE_DESCRIPTOR SpeakerTopologyNodes[] =
{
    // KSNODE_TOPO_VOLUME
    {
      0,                              // Flags
      &AutomationSpeakerVolume,     // AutomationTable
      &KSNODETYPE_VOLUME,             // Type
      &KSAUDFNAME_MASTER_VOLUME         // Name
    },
    // KSNODE_TOPO_MUTE
    {
      0,                              // Flags
      &AutomationSpeakerMute,       // AutomationTable
      &KSNODETYPE_MUTE,               // Type
      &KSAUDFNAME_MASTER_MUTE            // Name
    }
};

C_ASSERT(KSNODE_TOPO_VOLUME == 0);
C_ASSERT(KSNODE_TOPO_MUTE == 1);

static
PCCONNECTION_DESCRIPTOR SpeakerTopoMiniportConnections[] =
{
    //  FromNode,                 FromPin,                    ToNode,                 ToPin
    {   PCFILTER_NODE,            KSPIN_TOPO_WAVEOUT_SOURCE,    KSNODE_TOPO_VOLUME,     1 },
    {   KSNODE_TOPO_VOLUME,       0,                          KSNODE_TOPO_MUTE,       1 },
    {   KSNODE_TOPO_MUTE,         0,                          PCFILTER_NODE,          KSPIN_TOPO_LINEOUT_DEST }
};

//=============================================================================
static
PCPROPERTY_ITEM PropertiesSpeakerTopoFilter[] =
{
    {
        &KSPROPSETID_Jack,
        KSPROPERTY_JACK_DESCRIPTION,
        KSPROPERTY_TYPE_GET |
        KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_SpeakerTopoFilter
    },
    {
        &KSPROPSETID_Jack,
        KSPROPERTY_JACK_DESCRIPTION2,
        KSPROPERTY_TYPE_GET |
        KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_SpeakerTopoFilter
    }
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationSpeakerTopoFilter, PropertiesSpeakerTopoFilter);

//=============================================================================
static
PCFILTER_DESCRIPTOR SpeakerTopoMiniportFilterDescriptor =
{
  0,                                            // Version
  &AutomationSpeakerTopoFilter,                 // AutomationTable
  sizeof(PCPIN_DESCRIPTOR),                     // PinSize
  SIZEOF_ARRAY(SpeakerTopoMiniportPins),        // PinCount
  SpeakerTopoMiniportPins,                      // Pins
  sizeof(PCNODE_DESCRIPTOR),                    // NodeSize
  SIZEOF_ARRAY(SpeakerTopologyNodes),           // NodeCount
  SpeakerTopologyNodes,                         // Nodes
  SIZEOF_ARRAY(SpeakerTopoMiniportConnections), // ConnectionCount
  SpeakerTopoMiniportConnections,               // Connections
  0,                                            // CategoryCount
  NULL                                          // Categories
};

//=============================================================================
//
// Cable A Speaker - unique topology (separate WASAPI render endpoint)
//
//=============================================================================

// {B2C3D4E5-3333-4000-8000-AABBCCDDEEF3}
DEFINE_GUID(CABLEA_SPEAKER_CUSTOM_NAME,
    0xb2c3d4e5, 0x3333, 0x4000, 0x80, 0x00, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xf3);

static
PCPIN_DESCRIPTOR CableASpeakerTopoMiniportPins[] =
{
  // KSPIN_TOPO_WAVEOUT_SOURCE
  {
    0, 0, 0, NULL,
    {
      0, NULL, 0, NULL,
      SIZEOF_ARRAY(SpeakerTopoPinDataRangePointersBridge),
      SpeakerTopoPinDataRangePointersBridge,
      KSPIN_DATAFLOW_IN,
      KSPIN_COMMUNICATION_NONE,
      &KSCATEGORY_AUDIO,
      NULL,                               // Name (NULL = use INF FriendlyName)
      0
    }
  },
  // KSPIN_TOPO_LINEOUT_DEST
  {
    0, 0, 0, NULL,
    {
      0, NULL, 0, NULL,
      SIZEOF_ARRAY(SpeakerTopoPinDataRangePointersBridge),
      SpeakerTopoPinDataRangePointersBridge,
      KSPIN_DATAFLOW_OUT,
      KSPIN_COMMUNICATION_NONE,
      &KSNODETYPE_SPEAKER,
      NULL,
      0
    }
  }
};

static
PCFILTER_DESCRIPTOR CableASpeakerTopoMiniportFilterDescriptor =
{
  0,
  &AutomationSpeakerTopoFilter,                // reuse automation
  sizeof(PCPIN_DESCRIPTOR),
  SIZEOF_ARRAY(CableASpeakerTopoMiniportPins),
  CableASpeakerTopoMiniportPins,                // unique pins
  sizeof(PCNODE_DESCRIPTOR),
  SIZEOF_ARRAY(SpeakerTopologyNodes),
  SpeakerTopologyNodes,                         // reuse nodes
  SIZEOF_ARRAY(SpeakerTopoMiniportConnections),
  SpeakerTopoMiniportConnections,               // reuse connections
  0,
  NULL
};

//=============================================================================
//
// Cable B Speaker - unique topology
//
//=============================================================================

// {B2C3D4E5-4444-4000-8000-AABBCCDDEEF4}
DEFINE_GUID(CABLEB_SPEAKER_CUSTOM_NAME,
    0xb2c3d4e5, 0x4444, 0x4000, 0x80, 0x00, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xf4);

static
PCPIN_DESCRIPTOR CableBSpeakerTopoMiniportPins[] =
{
  // KSPIN_TOPO_WAVEOUT_SOURCE
  {
    0, 0, 0, NULL,
    {
      0, NULL, 0, NULL,
      SIZEOF_ARRAY(SpeakerTopoPinDataRangePointersBridge),
      SpeakerTopoPinDataRangePointersBridge,
      KSPIN_DATAFLOW_IN,
      KSPIN_COMMUNICATION_NONE,
      &KSCATEGORY_AUDIO,
      NULL,                               // Name (NULL = use INF FriendlyName)
      0
    }
  },
  // KSPIN_TOPO_LINEOUT_DEST
  {
    0, 0, 0, NULL,
    {
      0, NULL, 0, NULL,
      SIZEOF_ARRAY(SpeakerTopoPinDataRangePointersBridge),
      SpeakerTopoPinDataRangePointersBridge,
      KSPIN_DATAFLOW_OUT,
      KSPIN_COMMUNICATION_NONE,
      &KSNODETYPE_SPEAKER,
      NULL,
      0
    }
  }
};

static
PCFILTER_DESCRIPTOR CableBSpeakerTopoMiniportFilterDescriptor =
{
  0,
  &AutomationSpeakerTopoFilter,
  sizeof(PCPIN_DESCRIPTOR),
  SIZEOF_ARRAY(CableBSpeakerTopoMiniportPins),
  CableBSpeakerTopoMiniportPins,                // unique pins
  sizeof(PCNODE_DESCRIPTOR),
  SIZEOF_ARRAY(SpeakerTopologyNodes),
  SpeakerTopologyNodes,
  SIZEOF_ARRAY(SpeakerTopoMiniportConnections),
  SpeakerTopoMiniportConnections,
  0,
  NULL
};

#endif // _VIRTUALAUDIODRIVER_SPEAKERTOPTABLE_H_
