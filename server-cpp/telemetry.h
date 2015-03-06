#ifndef _TELEMETRY_H_
#define _TELEMETRY_H_

#include <stddef.h>
#include <stdint.h>

namespace telemetry {

// Maximum number of DataInterface objects a Telemetry object can hold.
// Used for array sizing.
const size_t MAX_DATA_PER_TELEMETRY = 16;

// Maximum payload size for telemetry packet.
const size_t MAX_PACKET_LENGTH = 65535;

// Various wire protocol constants.
const uint8_t SOF1 = 0x05;  // start of frame byte 1
const uint8_t SOF2 = 0x39;  // start of frame byte 2
// TODO: make these length independent

const uint8_t OPCODE_HEADER = 0x81;
const uint8_t OPCODE_DATA = 0x01;

const uint8_t DATAID_TERMINATOR = 0x00;

const uint8_t DATATYPE_NUMERIC = 0x01;
const uint8_t DATATYPE_NUMERIC_ARRAY = 0x02;

const uint8_t RECORDID_TERMINATOR = 0x00;
const uint8_t RECORDID_INTERNAL_NAME = 0x01;
const uint8_t RECORDID_DISPLAY_NAME = 0x02;
const uint8_t RECORDID_UNITS = 0x03;

const uint8_t RECORDID_OVERRIDE_CTL = 0x08;
const uint8_t RECORDID_OVERRIDE_DATA = 0x08;

const uint8_t RECORDID_NUMERIC_SUBTYPE = 0x40;
const uint8_t RECORDID_NUMERIC_LENGTH = 0x41;

const uint8_t NUMERIC_SUBTYPE_UINT = 0x01;
const uint8_t NUMERIC_SUBTYPE_SINT = 0x02;
const uint8_t NUMERIC_SUBTYPE_FLOAT = 0x03;

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

  // Finish the packet and writes data to the transmit stream (if not already
  // done). No more data may be written afterwards.
  virtual void finish() = 0;
};

// Abstract base class for telemetry data objects.
class Data {
public:
  Data(const char* internal_name, const char* display_name,
      const char* units):
      internal_name(internal_name),
      display_name(display_name),
      units(units) {};

  virtual ~Data() {}

  // Returns the data type code.
  virtual uint8_t get_data_type() = 0;

  // Returns the length of the header KVRs, in bytes. Does not include the
  // terminator header.
  virtual size_t get_header_kvrs_length();
  // Writes the header KVRs to the transmit packet. Does not write the
  // terminiator header.
  virtual void write_header_kvrs(TransmitPacketInterface& packet);

  // Returns the length of the payload, in bytes. Should be "fast".
  virtual size_t get_payload_length() = 0;
  // Writes the payload to the transmit packet. Should be "fast".
  virtual void write_payload(TransmitPacketInterface& packet) = 0;

protected:
  const char* internal_name;
  const char* display_name;
  const char* units;
};

// Telemetry Server object.
class Telemetry {
public:
  Telemetry(HalInterface& hal) :
    hal(hal),
    data_count(0),
    header_transmitted(false),
    packet_tx_sequence(0),
    packet_rx_sequence(0) {};

  // Associates a DataInterface with this object.
  void add_data(Data& new_data);

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

  // Calls the HAL's error function if some condition is false.
  void do_error(const char* message) {
    hal.do_error(message);
  }

protected:
  // Transmits any updated data.
  void transmit_data();

  // Handles received data, splitting regular UART data from in-band packet
  // data and processing received telemetry packets.
  void process_received_data();

  HalInterface& hal;

  // Array of associated DataInterface objects. The index+1 is the
  // DataInterface's data ID field.
  Data* data[MAX_DATA_PER_TELEMETRY];
  // Count of associated DataInterface objects.
  size_t data_count;

  bool header_transmitted;

  // Sequence number of the next packet to be transmitted.
  uint8_t packet_tx_sequence;
  uint8_t packet_rx_sequence;
};

// A telemetry packet with a length known before data is written to it.
// Data is written directly to the hardware transmit buffers without packet
// buffering. Assumes transmit buffers won't fill up.
class FixedLengthTransmitPacket : public TransmitPacketInterface {
public:
  FixedLengthTransmitPacket(HalInterface& hal, size_t length);

  virtual void write_uint8(uint8_t data);
  virtual void write_uint16(uint16_t data);
  virtual void write_uint32(uint32_t data);
  virtual void write_float(float data);

  virtual void finish();

protected:
  HalInterface& hal;

  // Predetermined length, in bytes, of this packet's payload, for sanity check.
  size_t length;

  // Current length, in bytes, of this packet's payload.
  size_t count;

  // Is the packet valid?
  bool valid;
};

template <typename T> class NumericData : public Data {
public:
  NumericData(Telemetry& telemetry_container, 
      const char* internal_name, const char* display_name,
      const char* units, T init_value):
      Data(internal_name, display_name, units),
      value(init_value) {
    telemetry_container.add_data(*this);
  }

  T operator = (T b) {
    this->value = b;
    return b;
    // TODO: mark as updated here
  }

  operator T() {
    if (overload_enabled) {
      return overloaded_value;
    } else {
      return value;
    }
  }

  virtual uint8_t get_data_type() { return DATATYPE_NUMERIC; }

  virtual size_t get_header_kvrs_length() {
    return Data::get_header_kvrs_length()
        + 1 + 1   // subtype
        + 1 + 1;  // data length
  }

  virtual void write_header_kvrs(TransmitPacketInterface& packet) {
    Data::write_header_kvrs(packet);
    packet.write_uint8(RECORDID_NUMERIC_SUBTYPE);
    packet.write_uint8(get_subtype());
    packet.write_uint8(RECORDID_NUMERIC_LENGTH);
    packet.write_uint8(sizeof(value));
  }

  uint8_t get_subtype();

  virtual size_t get_payload_length() {
    return sizeof(value);
  }

  virtual void write_payload(TransmitPacketInterface& packet);

protected:
  T overloaded_value;
  bool overload_enabled;

  T value;
};

}

#endif
