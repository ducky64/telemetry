#ifdef ARDUINO

#include "telemetry.h"
#include <Stream.h>

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

#endif // ifdef ARDUINO
