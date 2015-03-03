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
  while (*str != '\0') {
    packet.write_uint8(*str);
    str++;
  }
}

size_t Data::get_header_kvrs_length() {
  return 1 + strlen(internal_name)
      + 1 + strlen(display_name)
      + 1 + strlen(units);
}

void Data::write_header_kvrs(TransmitPacketInterface& packet) {
  packet.write_uint8(RECORDID_INTERNAL_NAME);
  packet_write_string(packet, internal_name);
  packet.write_uint8(RECORDID_DISPLAY_NAME);
  packet_write_string(packet, display_name);
  packet.write_uint8(RECORDID_UNITS);
  packet_write_string(packet, units);
}

template<typename T>
size_t IntData<T>::get_header_kvrs_length() {
  return Data::get_header_kvrs_length()
      + 1 + 1;  // data length
}

template<typename T>
void IntData<T>::write_header_kvrs(TransmitPacketInterface& packet) {
  Data::write_header_kvrs(packet);
  packet.write_uint8(RECORDID_INT_LENGTH);
  packet.write_uint8(get_payload_length());
}

template<>
size_t IntData<uint8_t>::get_payload_length() {
  return 1;
}
template<>
void IntData<uint8_t>::write_payload(TransmitPacketInterface& packet) {
  packet.write_uint8(*this);
}

template<>
size_t IntData<uint16_t>::get_payload_length() {
  return 2;
}
template<>
void IntData<uint16_t>::write_payload(TransmitPacketInterface& packet) {
  packet.write_uint16(*this);
}

template<>
size_t IntData<uint32_t>::get_payload_length() {
  return 4;
}
template<>
void IntData<uint32_t>::write_payload(TransmitPacketInterface& packet) {
  packet.write_uint32(*this);
}

IntData<uint8_t> instantiate_uint8("", "", "");
IntData<uint16_t> instantiate_uint16("", "", "");
IntData<uint32_t> instantiate_uint32("", "", "");

}
