// Make this less hacky and detect properly
#if defined(__ARMCC_VERSION) || defined(TOOLCHAIN_GCC_ARM)

#include "telemetry.h"
#include "mbed.h"
#include "MODSERIAL.h"

namespace telemetry {

class MbedHal : public HalInterface {
public:
  MbedHal(MODSERIAL& serial) :
    serial(serial) {
	  timer.start();
  }

  virtual void transmit_byte(uint8_t data);
  virtual size_t rx_available();
  virtual uint8_t receive_byte();

  virtual void do_error(const char* message);

  virtual uint32_t get_time_ms();

protected:
  MODSERIAL& serial;
  Timer timer;
};

}

#endif // ifdef MBED
