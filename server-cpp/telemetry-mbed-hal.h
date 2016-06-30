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

class MbedHalBase : public HalInterface {
public:
  MbedHalBase() {
    timer.start();
  }

  uint32_t get_time_ms();

protected:
  Timer timer;
};

class MbedHal : public MbedHalBase {
public:
  MbedHal(Serial& serial_in) :
    MbedHalBase(), serial(serial_in) {
  }

  void transmit_byte(uint8_t data);
  size_t rx_available();
  uint8_t receive_byte();

  void do_error(const char* message);

protected:
  Serial& serial;
};

class MbedRawSerialHal : public MbedHalBase {
public:
  MbedRawSerialHal(RawSerial& serial_in) :
    MbedHalBase(), serial(serial_in) {
  }

  void transmit_byte(uint8_t data);
  size_t rx_available();
  uint8_t receive_byte();

  void do_error(const char* message);

protected:
  RawSerial& serial;
};

}

#endif
