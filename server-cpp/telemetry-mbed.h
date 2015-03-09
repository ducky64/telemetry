// Make this less hacky and detect properly
#ifdef __ARMCC_VERSION

#include "telemetry.h"
#include "mbed.h"

namespace telemetry {

class MbedHal : public HalInterface {
public:
  MbedHal(Serial& serial) :
    serial(serial) {}

  virtual void transmit_byte(uint8_t data);
  virtual size_t rx_available();
  virtual uint8_t receive_byte();

  virtual void do_error(const char* message);

protected:
  Serial& serial;
};

}

#endif // ifdef MBED
