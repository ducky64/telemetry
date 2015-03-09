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
uint8_t Numeric<uint8_t>::get_subtype() {
  return NUMERIC_SUBTYPE_UINT;
}
template<>
void Numeric<uint8_t>::serialize_data(uint8_t value, TransmitPacketInterface& packet) {
  packet.write_uint8(value);
}
template<>
uint8_t Numeric<uint8_t>::deserialize_data(ReceivePacketBuffer& packet) {
  return packet.read_uint8();
}

template<>
uint8_t Numeric<uint16_t>::get_subtype() {
  return NUMERIC_SUBTYPE_UINT;
}
template<>
void Numeric<uint16_t>::serialize_data(uint16_t value, TransmitPacketInterface& packet) {
  packet.write_uint16(value);
}
template<>
uint16_t Numeric<uint16_t>::deserialize_data(ReceivePacketBuffer& packet) {
  return packet.read_uint16();
}

template<>
uint8_t Numeric<uint32_t>::get_subtype() {
  return NUMERIC_SUBTYPE_UINT;
}
template<>
void Numeric<uint32_t>::serialize_data(uint32_t value, TransmitPacketInterface& packet) {
  packet.write_uint32(value);
}
template<>
uint32_t Numeric<uint32_t>::deserialize_data(ReceivePacketBuffer& packet) {
  return packet.read_uint32();
}

// TODO: move into HAL
template<>
uint8_t Numeric<float>::get_subtype() {
  return NUMERIC_SUBTYPE_FLOAT;
}
template<>
void Numeric<float>::serialize_data(float value, TransmitPacketInterface& packet) {
  packet.write_float(value);
}
template<>
float Numeric<float>::deserialize_data(ReceivePacketBuffer& packet) {
  return packet.read_float();
}

}
