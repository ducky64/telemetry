/**
 * HAL header for mbed platforms. DO NOT INCLUDE THIS FILE DIRECTLY.
 * Use the automatic platform detection in telemetry.h instead.
 */

#include "mbed.h"

#include "telemetry-hal.h"

#ifndef _TELEMETRY_MBED_HAL_
#define _TELEMETRY_MBED_HAL_
#define TELEMETRY_HAL
#define TELEMETRY_HAL_MBED

namespace telemetry {

class MbedHal : public HalInterface {
public:
  MbedHal(RawSerial& serial_in) :
    serial(serial_in) {
	  timer.start();
  }

  void transmit_byte(uint8_t data);
  size_t rx_available();
  uint8_t receive_byte();

  void do_error(const char* message);

  uint32_t get_time_ms();

protected:
  RawSerial& serial;
  Timer timer;
};

}

#endif
