#ifndef _TELEMETRY_H_
#define _TELEMETRY_H_

#include <stddef.h>
#include <stdint.h>

namespace telemetry {

// Maximum number of DataInterface objects a Telemetry object can hold.
// Used for array sizing.
const size_t MAX_DATA_PER_TELEMETRY = 16;

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

  // Returns the current system time in us. Rollover is handled gracefully.
  virtual uint32_t get_time_us() = 0;

  // Called on a telemetry error.
  virtual void error(const char* message) = 0;
};

// Abstract base class for building a packet to be transmitted.
// Implementation is unconstrained - writes may either be buffered or passed
// directly to the hardware transmit buffers.
class TransmitPacketInterface {
public:
  virtual ~TransmitPacketInterface() {}

  // Writes a 8-bit unsigned integer to the packet stream.
  virtual void write_uint8(uint8_t data) = 0;
  // Writes a 16-bit unsigned integer to the packet stream.
  virtual void write_uint16(uint16_t data) = 0;
  // Writes a 32-bit unsigned integer to the packet stream.
  virtual void write_uint32(uint32_t data) = 0;
  // Writes a float to the packet stream.
  virtual void write_float(float data) = 0;
  // Writes a double to the packet stream.
  virtual void write_double(double data) = 0;

  // Finish the packet and writes data to the transmit stream (if not already
  // done). No more data may be written afterwards.
  virtual void finish_packet() = 0;
};

// Abstract base class for telemetry data objects.
class DataInterface {
public:
  virtual ~DataInterface() {}

  // Returns the length of the header, in bytes.
  virtual size_t get_header_length() = 0;
  // Writes the header to the transmit packet.
  virtual void write_header(TransmitPacketInterface& packet) = 0;

  // Returns the length of the payload, in bytes. Should be "fast".
  virtual size_t get_payload_length() = 0;
  // Writes the payload to the transmit packet. Should be "fast".
  virtual void write_payload(TransmitPacketInterface& packet) = 0;
};

// Telemetry Server object.
class Telemetry {
public:
  Telemetry(HalInterface& hal) :
    hal(hal),
    data_count(0),
    header_transmitted(false) {};

  // Associates a DataInterface with this object.
  void add_data(DataInterface& data);

  // Transmits header data. Must be called after all add_data calls are done
  // and before and IO is done.
  void transmit_header();

  // Does IO, including transmitting telemetry packets. Should be called on
  // a regular basis. Since this does IO, this may block depending on the HAL
  // semantics.
  void do_io();

  // TODO: better docs defining in-band receive.
  // Returns the number of bytes available in the receive stream.
  size_t receive_available();
  // Returns the next byte in the receive stream.
  uint8_t read_receive();

protected:
  // Transmits any updated data.
  void transmit_data();

  // Handles received data, splitting regular UART data from in-band packet
  // data and processing received telemetry packets.
  void process_received_data();

  HalInterface& hal;

  // Array of associated DataInterface objects.
  DataInterface* data[MAX_DATA_PER_TELEMETRY];
  // Count of associated DataInterface objects.
  size_t data_count;

  bool header_transmitted;
};

// A telemetry packet with a length known before data is written to it.
// Data is written directly to the hardware transmit buffers without packet
// buffering. Assumes transmit buffers won't fill up.
class FixedLengthTransmitPacket : public TransmitPacketInterface {
public:
  FixedLengthTransmitPacket(size_t length);

  void write_uint8(uint8_t data);
  void write_uint16(uint16_t data);
  void write_uint32(uint32_t data);
  void write_float(float data);
  void write_double(double data);

  void finish_packet() = 0;
};

// Telemetry data for integer types (uint8_t, uint16_t, ...).
template <typename T> class IntData : public DataInterface {
public:
  size_t get_header_length();
  void write_header(TransmitPacketInterface& packet);

  size_t get_payload_length();
  void write_payload(TransmitPacketInterface& packet);

protected:
};

// Telemetry data for float types (float, double).
template <typename T> class FloatData : public DataInterface {
public:
  size_t get_header_length();
  void write_header(TransmitPacketInterface& packet);

  size_t get_payload_length();
  void write_payload(TransmitPacketInterface& packet);

protected:
};

}

#endif
