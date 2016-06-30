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

template<typename S>
class MbedHalBase : public HalInterface {
public:
  MbedHalBase(S& serial_in) : serial(serial_in) {
    timer.start();
  }

  uint32_t get_time_ms() {
    return timer.read_ms();
  }

  void transmit_byte(uint8_t data) {
    // TODO: optimize with DMA
    serial.putc(data);
  }

  size_t rx_available() {
    return serial.readable();
  }

  uint8_t receive_byte() {
    return serial.getc();
  }

  void do_error(const char* msg) {
    serial.puts(msg);
    serial.puts("\r\n");
  }

protected:
  S& serial;
  Timer timer;
};

class MbedHal : public MbedHalBase<Serial> {
};

class MbedRawSerialHal : public MbedHalBase<RawSerial> {
};

}

#endif
