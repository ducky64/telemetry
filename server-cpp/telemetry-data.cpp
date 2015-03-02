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

}
