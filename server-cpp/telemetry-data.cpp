/*
 * telemetry-data.cpp
 *
 *  Created on: Mar 2, 2015
 *      Author: Ducky
 *
 * Implementation for Telemetry Data classes.
 */
#include <telemetry.h>
#include <string.h>

namespace telemetry {
void packet_write_string(TransmitPacketInterface& packet, const char* str) {
  // TODO: move into HAL for higher performance?
  while (*str != '\0') {
    packet.write_uint8(*str);
    str++;
  }
  packet.write_uint8('\0');
}

size_t Data::get_header_kvrs_length() {
  return 1 + strlen(internal_name) + 1
      + 1 + strlen(display_name) + 1
      + 1 + strlen(units) + 1;
}

void Data::write_header_kvrs(TransmitPacketInterface& packet) {
  packet.write_uint8(RECORDID_INTERNAL_NAME);
  packet_write_string(packet, internal_name);
  packet.write_uint8(RECORDID_DISPLAY_NAME);
  packet_write_string(packet, display_name);
  packet.write_uint8(RECORDID_UNITS);
  packet_write_string(packet, units);
}

template<>
uint8_t NumericData<uint8_t>::get_subtype() {
  return NUMERIC_SUBTYPE_UINT;
}
template<>
void NumericData<uint8_t>::write_payload(TransmitPacketInterface& packet) {
  packet.write_uint8(*this);
}

template<>
uint8_t NumericData<uint16_t>::get_subtype() {
  return NUMERIC_SUBTYPE_UINT;
}
template<>
void NumericData<uint16_t>::write_payload(TransmitPacketInterface& packet) {
  packet.write_uint16(*this);
}

template<>
uint8_t NumericData<uint32_t>::get_subtype() {
  return NUMERIC_SUBTYPE_UINT;
}
template<>
void NumericData<uint32_t>::write_payload(TransmitPacketInterface& packet) {
  packet.write_uint32(*this);
}

// TODO: move into HAL
template<>
uint8_t NumericData<float>::get_subtype() {
  return NUMERIC_SUBTYPE_FLOAT;
}
template<>
void NumericData<float>::write_payload(TransmitPacketInterface& packet) {
  packet.write_float(*this);
}

}
