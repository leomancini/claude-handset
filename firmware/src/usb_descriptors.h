/*
 * USB descriptor layout for the Claude Handset composite device.
 */

#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

// pid.codes test PID (0x1209:0x0001 is reserved for lab/test use). Replace
// with an allocated PID before distributing hardware.
#define HANDSET_USB_VID   0x1209
#define HANDSET_USB_PID   0x0001
#define HANDSET_USB_BCD   0x0100

// UAC2 entity IDs (arbitrary but unique within the audio function)
#define UAC2_ENTITY_CLOCK                 0x04
// Speaker path: USB -> feature unit -> speaker
#define UAC2_ENTITY_SPK_INPUT_TERMINAL    0x01
#define UAC2_ENTITY_SPK_FEATURE_UNIT      0x02
#define UAC2_ENTITY_SPK_OUTPUT_TERMINAL   0x03
// Microphone path: mic -> feature unit -> USB
#define UAC2_ENTITY_MIC_INPUT_TERMINAL    0x11
#define UAC2_ENTITY_MIC_FEATURE_UNIT      0x12
#define UAC2_ENTITY_MIC_OUTPUT_TERMINAL   0x13

enum {
  ITF_NUM_AUDIO_CONTROL = 0,
  ITF_NUM_AUDIO_STREAMING_SPK,
  ITF_NUM_AUDIO_STREAMING_MIC,
  ITF_NUM_HID,
  ITF_NUM_RESET,
  ITF_NUM_TOTAL
};

// Endpoint addresses
#define EPNUM_AUDIO_OUT   0x01   // speaker data, host -> device
#define EPNUM_AUDIO_FB    0x81   // speaker rate feedback, device -> host
#define EPNUM_AUDIO_IN    0x82   // microphone data, device -> host
#define EPNUM_HID_IN      0x83   // keyboard reports

// String descriptor indices
enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
  STRID_SPEAKER,
  STRID_MICROPHONE,
  STRID_HID,
  STRID_RESET,
};

// Total length of the audio function's descriptors (IAD through the last
// streaming alternate). Must match HANDSET_AUDIO_DESCRIPTOR below.
#define HANDSET_AUDIO_DESC_LEN (TUD_AUDIO_DESC_IAD_LEN\
    + TUD_AUDIO_DESC_STD_AC_LEN\
    + TUD_AUDIO_DESC_CS_AC_LEN\
    + TUD_AUDIO_DESC_CLK_SRC_LEN\
    + TUD_AUDIO_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL_LEN\
    + TUD_AUDIO_DESC_OUTPUT_TERM_LEN\
    + TUD_AUDIO_DESC_INPUT_TERM_LEN\
    + TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL_LEN\
    + TUD_AUDIO_DESC_OUTPUT_TERM_LEN\
    /* Speaker streaming interface, alt 0 (idle) */\
    + TUD_AUDIO_DESC_STD_AS_INT_LEN\
    /* Speaker streaming interface, alt 1 (48 kHz 16-bit stereo) */\
    + TUD_AUDIO_DESC_STD_AS_INT_LEN\
    + TUD_AUDIO_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN\
    + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN\
    + TUD_AUDIO_DESC_STD_AS_ISO_FB_EP_LEN\
    /* Microphone streaming interface, alt 0 (idle) */\
    + TUD_AUDIO_DESC_STD_AS_INT_LEN\
    /* Microphone streaming interface, alt 1 (48 kHz 16-bit mono) */\
    + TUD_AUDIO_DESC_STD_AS_INT_LEN\
    + TUD_AUDIO_DESC_CS_AS_INT_LEN\
    + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN\
    + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN\
    + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN)

#define HANDSET_AC_TOTAL_LEN (TUD_AUDIO_DESC_CLK_SRC_LEN\
    + TUD_AUDIO_DESC_INPUT_TERM_LEN + TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL_LEN + TUD_AUDIO_DESC_OUTPUT_TERM_LEN\
    + TUD_AUDIO_DESC_INPUT_TERM_LEN + TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL_LEN + TUD_AUDIO_DESC_OUTPUT_TERM_LEN)

#define HANDSET_FU_CTRL_MUTE_VOL ((AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS) | (AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS))

#define HANDSET_AUDIO_DESCRIPTOR(_stridx_spk, _stridx_mic) \
    /* Interface Association Descriptor: the three audio interfaces form one function */\
    TUD_AUDIO_DESC_IAD(/*_firstitf*/ ITF_NUM_AUDIO_CONTROL, /*_nitfs*/ 3, /*_stridx*/ 0x00),\
    /* Standard AC Interface Descriptor (4.7.1), no interrupt endpoint */\
    TUD_AUDIO_DESC_STD_AC(/*_itfnum*/ ITF_NUM_AUDIO_CONTROL, /*_nEPs*/ 0x00, /*_stridx*/ STRID_PRODUCT),\
    /* Class-Specific AC Interface Header Descriptor (4.7.2) */\
    TUD_AUDIO_DESC_CS_AC(/*_bcdADC*/ 0x0200, /*_category*/ AUDIO_FUNC_HEADSET, /*_totallen*/ HANDSET_AC_TOTAL_LEN, /*_ctrl*/ AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS),\
    /* Clock Source Descriptor (4.7.2.1): internal programmable clock, frequency RW, validity R */\
    TUD_AUDIO_DESC_CLK_SRC(/*_clkid*/ UAC2_ENTITY_CLOCK, /*_attr*/ AUDIO_CLOCK_SOURCE_ATT_INT_PRO_CLK, /*_ctrl*/ (AUDIO_CTRL_RW << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS) | (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_VAL_POS), /*_assocTerm*/ 0x00, /*_stridx*/ 0x00),\
    /* --- Speaker path --- */\
    TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ HANDSET_SPK_CHANNELS, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_FRONT_LEFT | AUDIO_CHANNEL_CONFIG_FRONT_RIGHT, /*_idxchannelnames*/ 0x00, /*_ctrl*/ 0, /*_stridx*/ 0x00),\
    TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL(/*_unitid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_srcid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_ctrlch0master*/ HANDSET_FU_CTRL_MUTE_VOL, /*_ctrlch1*/ 0, /*_ctrlch2*/ 0, /*_stridx*/ 0x00),\
    TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_SPK_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_OUT_GENERIC_SPEAKER, /*_assocTerm*/ 0x00, /*_srcid*/ UAC2_ENTITY_SPK_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
    /* --- Microphone path --- */\
    TUD_AUDIO_DESC_INPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ 0x00, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_nchannelslogical*/ HANDSET_MIC_CHANNELS, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_idxchannelnames*/ 0x00, /*_ctrl*/ 0, /*_stridx*/ 0x00),\
    TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL(/*_unitid*/ UAC2_ENTITY_MIC_FEATURE_UNIT, /*_srcid*/ UAC2_ENTITY_MIC_INPUT_TERMINAL, /*_ctrlch0master*/ HANDSET_FU_CTRL_MUTE_VOL, /*_ctrlch1*/ 0, /*_stridx*/ 0x00),\
    TUD_AUDIO_DESC_OUTPUT_TERM(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, /*_srcid*/ UAC2_ENTITY_MIC_FEATURE_UNIT, /*_clkid*/ UAC2_ENTITY_CLOCK, /*_ctrl*/ 0x0000, /*_stridx*/ 0x00),\
    /* --- Speaker streaming interface --- */\
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ ITF_NUM_AUDIO_STREAMING_SPK, /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx_spk),\
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ ITF_NUM_AUDIO_STREAMING_SPK, /*_altset*/ 0x01, /*_nEPs*/ 0x02, /*_stridx*/ _stridx_spk),\
    TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_SPK_INPUT_TERMINAL, /*_ctrl*/ AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I, /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ HANDSET_SPK_CHANNELS, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_FRONT_LEFT | AUDIO_CHANNEL_CONFIG_FRONT_RIGHT, /*_stridx*/ 0x00),\
    TUD_AUDIO_DESC_TYPE_I_FORMAT(HANDSET_BYTES_PER_SAMPLE, HANDSET_BITS_PER_SAMPLE),\
    TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ EPNUM_AUDIO_OUT, /*_attr*/ (uint8_t)((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX, /*_interval*/ 0x01),\
    TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 0x0001),\
    TUD_AUDIO_DESC_STD_AS_ISO_FB_EP(/*_ep*/ EPNUM_AUDIO_FB, /*_epsize*/ 4, /*_interval*/ 1),\
    /* --- Microphone streaming interface --- */\
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ ITF_NUM_AUDIO_STREAMING_MIC, /*_altset*/ 0x00, /*_nEPs*/ 0x00, /*_stridx*/ _stridx_mic),\
    TUD_AUDIO_DESC_STD_AS_INT(/*_itfnum*/ ITF_NUM_AUDIO_STREAMING_MIC, /*_altset*/ 0x01, /*_nEPs*/ 0x01, /*_stridx*/ _stridx_mic),\
    TUD_AUDIO_DESC_CS_AS_INT(/*_termid*/ UAC2_ENTITY_MIC_OUTPUT_TERMINAL, /*_ctrl*/ AUDIO_CTRL_NONE, /*_formattype*/ AUDIO_FORMAT_TYPE_I, /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, /*_nchannelsphysical*/ HANDSET_MIC_CHANNELS, /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, /*_stridx*/ 0x00),\
    TUD_AUDIO_DESC_TYPE_I_FORMAT(HANDSET_BYTES_PER_SAMPLE, HANDSET_BITS_PER_SAMPLE),\
    TUD_AUDIO_DESC_STD_AS_ISO_EP(/*_ep*/ EPNUM_AUDIO_IN, /*_attr*/ (uint8_t)((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_DATA), /*_maxEPsize*/ CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX, /*_interval*/ 0x01),\
    TUD_AUDIO_DESC_CS_AS_ISO_EP(/*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, /*_ctrl*/ AUDIO_CTRL_NONE, /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, /*_lockdelay*/ 0x0000)

// Raspberry Pi reset interface (vendor class, subclass 0, protocol 1). A bare
// interface with no endpoints; picotool talks to it with control requests.
#define HANDSET_RESET_DESC_LEN 9
#define HANDSET_RESET_DESCRIPTOR(_itfnum, _stridx) \
    9, TUSB_DESC_INTERFACE, _itfnum, 0, 0, TUSB_CLASS_VENDOR_SPECIFIC, 0x00, 0x01, _stridx

#endif /* USB_DESCRIPTORS_H_ */
