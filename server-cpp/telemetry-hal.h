#ifndef _TELEMETRY_HAL_H_
#define _TELEMETRY_HAL_H_

namespace telemetry {

// Hardware abstraction layer for the telemetry server.
class HalInterface {
public:
  virtual ~HalInterface() {}

  // Write a byte to the transmit buffer.
  virtual void transmit_byte(uint8_t data) = 0;
  // Returns the number of bytes available in the receive buffer.
  virtual size_t rx_available() = 0;
  // Returns the next byte in the receive stream. rx_available must return > 0.
  virtual uint8_t receive_byte() = 0;

  // TODO: more efficient block transmit operations?

  // Called on a telemetry error.
  virtual void do_error(const char* message) = 0;

  // Return the current time in milliseconds. May overflow at any time.
  virtual uint32_t get_time_ms() = 0;
};

}

#endif
