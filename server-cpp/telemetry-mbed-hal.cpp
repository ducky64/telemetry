/*
 * telemetry-mbedo-hal.cpp
 *
 *  Created on: Mar 4, 2015
 *      Author: Ducky
 *
 * Telemetry HAL for Serial on mBed.
 */

#include "telemetry.h"

#ifdef TELEMETRY_HAL_MBED

namespace telemetry {

void MbedHal::transmit_byte(uint8_t data) {
  // TODO: optimize with DMA
  serial.putc(data);
}

size_t MbedHal::rx_available() {
  return serial.readable();
}

uint8_t MbedHal::receive_byte() {
  return serial.getc();
}

void MbedHal::do_error(const char* msg) {
  serial.puts(msg);
  serial.puts("\r\n");
}

uint32_t MbedHal::get_time_ms() {
  return timer.read_ms();
}

}

#endif
