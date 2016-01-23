/*
 * telemetry-data.cpp
 *
 *  Created on: Mar 2, 2015
 *      Author: Ducky
 *
 * Implementation for Telemetry Data classes.
 */
#include "telemetry.h"
#include <string.h>

namespace telemetry {
void packet_write_string(TransmitPacket& packet, const char* str) {
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

void Data::write_header_kvrs(TransmitPacket& packet) {
  packet.write_uint8(protocol::RECORDID_INTERNAL_NAME);
  packet_write_string(packet, internal_name);
  packet.write_uint8(protocol::RECORDID_DISPLAY_NAME);
  packet_write_string(packet, display_name);
  packet.write_uint8(protocol::RECORDID_UNITS);
  packet_write_string(packet, units);
}

}
