/**
 * HAL header for Arduino platforms. DO NOT INCLUDE THIS FILE DIRECTLY.
 * Use the automatic platform detection in telemetry.h instead.
 */

#include <Stream.h>

#include "telemetry-hal.h"

#ifndef _TELEMETRY_ARDUINO_HAL_
#define _TELEMETRY_ARDUINO_HAL_
#define TELEMETRY_HAL
#define TELEMETRY_HAL_ARDUINO

namespace telemetry {

class ArduinoHalInterface : public HalInterface {
public:
  ArduinoHalInterface(Stream& serial) :
    serial(serial) {}

  virtual void transmit_byte(uint8_t data);
  virtual size_t rx_available();
  virtual uint8_t receive_byte();

  virtual void do_error(const char* message);

protected:
  Stream& serial;
};

}

#endif
