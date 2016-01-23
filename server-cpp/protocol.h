/**
 * Telemetry protocol defines.
 */

#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

namespace telemetry {
// Various wire protocol constants.
const uint8_t SOF1 = 0x05;  // start of frame byte 1
const uint8_t SOF2 = 0x39;  // start of frame byte 2
const uint8_t SOF_SEQ[] = {0x05, 0x39};

const size_t LENGTH_SIZE = 2;

// TODO: make these length independent

const uint8_t OPCODE_HEADER = 0x81;
const uint8_t OPCODE_DATA = 0x01;

const uint8_t DATAID_TERMINATOR = 0x00;

const uint8_t DATATYPE_NUMERIC = 0x01;
const uint8_t DATATYPE_NUMERIC_ARRAY = 0x02;

const uint8_t RECORDID_TERMINATOR = 0x00;
const uint8_t RECORDID_INTERNAL_NAME = 0x01;
const uint8_t RECORDID_DISPLAY_NAME = 0x02;
const uint8_t RECORDID_UNITS = 0x03;

const uint8_t RECORDID_OVERRIDE_CTL = 0x08;
const uint8_t RECORDID_OVERRIDE_DATA = 0x08;

const uint8_t RECORDID_NUMERIC_SUBTYPE = 0x40;
const uint8_t RECORDID_NUMERIC_LENGTH = 0x41;
const uint8_t RECORDID_NUMERIC_LIMITS = 0x42;
const uint8_t RECORDID_ARRAY_COUNT = 0x50;

const uint8_t NUMERIC_SUBTYPE_UINT = 0x01;
const uint8_t NUMERIC_SUBTYPE_SINT = 0x02;
const uint8_t NUMERIC_SUBTYPE_FLOAT = 0x03;

const uint32_t DECODER_TIMEOUT_MS = 100;

namespace protocol {

/**
 * Returns the subtype field value for a numeric recordid.
 */
template<typename T> uint8_t numeric_subtype();
}

}

#endif
