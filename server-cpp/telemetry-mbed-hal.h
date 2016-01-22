/**
 * HAL header for mbed platforms. DO NOT INCLUDE THIS FILE DIRECTLY.
 * Use the automatic platform detection in telemetry.h instead.
 */

#include "mbed.h"
#include "MODSERIAL.h"

#include "telemetry-hal.h"

#ifndef _TELEMETRY_MBED_HAL_
#define _TELEMETRY_MBED_HAL_
#define TELEMETRY_HAL
#define TELEMETRY_HAL_MBED

namespace telemetry {

class MbedHal : public HalInterface {
public:
  MbedHal(MODSERIAL& serial_in) :
    serial(&serial_in) {
	  timer.start();
  }
  MbedHal() :
      serial(NULL) {
  	  timer.start();
  }

  void set_serial(MODSERIAL& serial_new) {
	serial = &serial_new;
  }

  virtual void transmit_byte(uint8_t data);
  virtual size_t rx_available();
  virtual uint8_t receive_byte();

  virtual void do_error(const char* message);

  virtual uint32_t get_time_ms();

protected:
  MODSERIAL* serial;
  Timer timer;
};

}

#endif
