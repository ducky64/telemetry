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
  if (serial != NULL) {
    serial->putc(data);
  }
}

size_t MbedHal::rx_available() {
  if (serial != NULL) {
    return serial->rxBufferGetCount();
  } else {
	return 0;
  }

}

uint8_t MbedHal::receive_byte() {
  if (serial != NULL) {
    return serial->getc();
  } else {
	return 0;
  }
}

void MbedHal::do_error(const char* msg) {
  if (serial != NULL) {
    serial->puts(msg);
    serial->puts("\r\n");
  }
}

uint32_t MbedHal::get_time_ms() {
  return timer.read_ms();
}

}

#endif
