/*
 * telemetry-arduino-hal.cpp
 *
 *  Created on: Mar 2, 2015
 *      Author: Ducky
 *
 * Telemetry HAL for Serial on Arduino.
 */

#include "telemetry.h"

#ifdef TELEMETRY_HAL_ARDUINO

namespace telemetry {

void ArduinoHalInterface::transmit_byte(uint8_t data) {
  serial.write(data);
}

size_t ArduinoHalInterface::rx_available() {
  return serial.available();
}

uint8_t ArduinoHalInterface::receive_byte() {
  // TODO: handle -1 case
  return serial.read();
}

void ArduinoHalInterface::do_error(const char* msg) {
  // TODO: use side channel?
  serial.println(msg);
}

}

#endif
