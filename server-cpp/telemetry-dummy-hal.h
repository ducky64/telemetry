/**
 * Dummy HAL header, does nothing.
 * Use the automatic platform detection in telemetry.h instead.
 */

#include "mbed.h"

#include "telemetry-hal.h"

#ifndef _TELEMETRY_DUMMY_HAL_
#define _TELEMETRY_DUMMY_HAL_

namespace telemetry {

class DummyHal : public HalInterface {
public:
  DummyHal() {
  }

  void transmit_byte(uint8_t data) {

  };
  size_t rx_available() {
    return 0;
  }
  uint8_t receive_byte() {
    return 0;
  }

  void do_error(const char* message) {
  }

  uint32_t get_time_ms() {
    return 0;
  }
};

}

#endif
