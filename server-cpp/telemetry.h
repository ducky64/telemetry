#ifndef _TELEMETRY_H_
#define _TELEMETRY_H_

#include <stddef.h>
#include <stdint.h>

namespace telemetry {

#ifndef TELEMETRY_DATA_LIMIT
#define TELEMETRY_DATA_LIMIT 16
#endif

// Maximum number of DataInterface objects a Telemetry object can hold.
// Used for array sizing.
const size_t MAX_DATA_PER_TELEMETRY = TELEMETRY_DATA_LIMIT;

// Maximum payload size for a received telemetry packet.
const size_t MAX_RECEIVE_PACKET_LENGTH = 255;

// Various wire protocol constants.
const uint8_t SOF1 = 0x05;  // start of frame byte 1
const uint8_t SOF2 = 0x39;  // start of frame byte 2
const uint8_t SOF_SEQ[] = {0x05, 0x39};

const size_t LENGTH_SIZE = 2;

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
const uint8_t RECORDID_NUMERIC_LIMITS = 0x42;
const uint8_t RECORDID_ARRAY_COUNT = 0x50;

const uint8_t NUMERIC_SUBTYPE_UINT = 0x01;
const uint8_t NUMERIC_SUBTYPE_SINT = 0x02;
const uint8_t NUMERIC_SUBTYPE_FLOAT = 0x03;

const uint32_t DECODER_TIMEOUT_MS = 100;

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

// Abstract base class for building a packet to be transmitted.
// Implementation is unconstrained - writes may either be buffered or passed
// directly to the hardware transmit buffers.
class TransmitPacketInterface {
public:
  virtual ~TransmitPacketInterface() {}

  // Writes a 8-bit unsigned integer to the packet stream.
  virtual void write_byte(uint8_t data) = 0;
    
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

class ReceivePacketBuffer {
public:
  ReceivePacketBuffer(HalInterface& hal);

  // Starts a new packet, resetting the packet length and read pointer.
  void new_packet();

  // Appends a new byte onto this packet, advancing the packet length
  void add_byte(uint8_t byte);

  // Reads a 8-bit unsigned integer from the packet stream, advancing buffer.
  uint8_t read_uint8();
  // Reads a 16-bit unsigned integer from the packet stream, advancing buffer.
  uint16_t read_uint16();
  // Reads a 32-bit unsigned integer from the packet stream, advancing buffer.
  uint32_t read_uint32();
  // Reads a float from the packet stream, advancing buffer.
  float read_float();

protected:
  HalInterface& hal;

  size_t packet_length;
  size_t read_loc;
  uint8_t data[MAX_RECEIVE_PACKET_LENGTH];
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

  // Sets my value from the received packet, interpreting the current packet
  // read position as my data type.
  virtual void set_from_packet(ReceivePacketBuffer& packet) = 0;

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
    received_packet(ReceivePacketBuffer(hal)),
    decoder_state(SOF),
    decoder_pos(0),
    packet_length(0),
	decoder_last_received(false),
	decoder_last_receive_ms(0),
    header_transmitted(false),
    packet_tx_sequence(0),
    packet_rx_sequence(0) {};

  // Associates a DataInterface with this object, returning the data ID.
  size_t add_data(Data& new_data);

  // Marks a data ID as updated, to be transmitted in the next packet.
  void mark_data_updated(size_t data_id);

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

  // Handles a received packet in received_packet.
  void process_received_packet();

  HalInterface& hal;

  // Array of associated DataInterface objects. The index+1 is the
  // DataInterface's data ID field.
  Data* data[MAX_DATA_PER_TELEMETRY];
  // Whether each data has been updated or not.
  bool data_updated[MAX_DATA_PER_TELEMETRY];
  // Count of associated DataInterface objects.
  size_t data_count;

  // Buffer holding the receive packet being assembled / parsed.
  ReceivePacketBuffer received_packet;

  enum DecoderState {
    SOF,    // reading start-of-frame sequence (or just non-telemetry data)
    LENGTH, // reading packet length
    DATA,   // reading telemetry packet data
    DATA_DESTUFF,     // reading a stuffed byte
    DATA_DESTUFF_END  // last stuffed byte in a packet
  } decoder_state;

  size_t decoder_pos;
  size_t packet_length;
  bool decoder_last_received;
  uint32_t decoder_last_receive_ms;

  bool header_transmitted;

  // Sequence number of the next packet to be transmitted.
  uint8_t packet_tx_sequence;
  uint8_t packet_rx_sequence; // TODO use this somewhere
};

// A telemetry packet with a length known before data is written to it.
// Data is written directly to the hardware transmit buffers without packet
// buffering. Assumes transmit buffers won't fill up.
class FixedLengthTransmitPacket : public TransmitPacketInterface {
public:
  FixedLengthTransmitPacket(HalInterface& hal, size_t length);

  virtual void write_byte(uint8_t data);

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

template <typename T> 
class Numeric : public Data {
public:
  Numeric(Telemetry& telemetry_container, 
      const char* internal_name, const char* display_name,
      const char* units, T init_value):
      Data(internal_name, display_name, units),
      telemetry_container(telemetry_container),
      value(init_value), min_val(init_value), max_val(init_value),
      frozen(false) {
    data_id = telemetry_container.add_data(*this);
  }

  T operator = (T b) {
    if (!frozen) {
      value = b;
      telemetry_container.mark_data_updated(data_id);
    }
    return value;
  }

  operator T() {
    return value;
  }
  
  Numeric<T>& set_limits(T min, T max) {
    min_val = min;
    max_val = max;
    return *this;
  }
  
  virtual uint8_t get_data_type() { return DATATYPE_NUMERIC; }

  virtual size_t get_header_kvrs_length() {
    return Data::get_header_kvrs_length()
        + 1 + 1   // subtype
        + 1 + 1   // data length
        + 1 + sizeof(value) + sizeof(value);  // limits
  }

  virtual void write_header_kvrs(TransmitPacketInterface& packet) {
    Data::write_header_kvrs(packet);
    packet.write_uint8(RECORDID_NUMERIC_SUBTYPE);
    packet.write_uint8(get_subtype());
    packet.write_uint8(RECORDID_NUMERIC_LENGTH);
    packet.write_uint8(sizeof(value));
    packet.write_uint8(RECORDID_NUMERIC_LIMITS);
    serialize_data(min_val, packet);
    serialize_data(max_val, packet);
  }

  uint8_t get_subtype();

  virtual size_t get_payload_length() { return sizeof(value); }
  virtual void write_payload(TransmitPacketInterface& packet) { serialize_data(value, packet); }
  virtual void set_from_packet(ReceivePacketBuffer& packet) {
    value = deserialize_data(packet);
    telemetry_container.mark_data_updated(data_id); }
  
  void serialize_data(T data, TransmitPacketInterface& packet);
  T deserialize_data(ReceivePacketBuffer& packet);

protected:
  Telemetry& telemetry_container;
  size_t data_id;
  T value;
  T min_val, max_val;
  bool frozen;
};

template <typename T, uint32_t array_count>
class NumericArrayAccessor;

// TODO: fix this partial specialization inheritance nightmare
template <typename T, uint32_t array_count> 
class NumericArrayBase : public Data {
  friend class NumericArrayAccessor<T, array_count>;
public:
  NumericArrayBase(Telemetry& telemetry_container, 
      const char* internal_name, const char* display_name,
      const char* units, T elem_init_value):
      Data(internal_name, display_name, units),
      telemetry_container(telemetry_container),
      min_val(elem_init_value), max_val(elem_init_value),
      frozen(false) {
    for (size_t i=0; i<array_count; i++) {
      value[i] = elem_init_value;
    }
    data_id = telemetry_container.add_data(*this);
  }

  NumericArrayAccessor<T, array_count> operator[] (const int index) {
    // TODO: add bounds checking here
    // TODO: add "frozen" check
    return NumericArrayAccessor<T, array_count>(*this, index);
  }

  NumericArrayBase<T, array_count>& set_limits(T min, T max) {
    min_val = min;
    max_val = max;
    return *this;
  }
  
  virtual uint8_t get_data_type() { return DATATYPE_NUMERIC_ARRAY; }

  virtual size_t get_header_kvrs_length() {
    return Data::get_header_kvrs_length()
        + 1 + 1   // subtype
        + 1 + 1   // data length
        + 1 + 4   // array length
        + 1 + sizeof(value[0]) + sizeof(value[0]);  // limits
  }

  virtual void write_header_kvrs(TransmitPacketInterface& packet) {
    Data::write_header_kvrs(packet);
    packet.write_uint8(RECORDID_NUMERIC_SUBTYPE);
    packet.write_uint8(get_subtype());
    packet.write_uint8(RECORDID_NUMERIC_LENGTH);
    packet.write_uint8(sizeof(value[0]));
    packet.write_uint8(RECORDID_ARRAY_COUNT);
    packet.write_uint32(array_count);
    packet.write_uint8(RECORDID_NUMERIC_LIMITS);
    serialize_data(min_val, packet);
    serialize_data(max_val, packet);
  }

  virtual uint8_t get_subtype() = 0;

  virtual size_t get_payload_length() { return sizeof(value); }
  virtual void write_payload(TransmitPacketInterface& packet) {
    for (size_t i=0; i<array_count; i++) { serialize_data(this->value[i], packet); } }
  virtual void set_from_packet(ReceivePacketBuffer& packet) {
    for (size_t i=0; i<array_count; i++) { value[i] = deserialize_data(packet); }
    telemetry_container.mark_data_updated(data_id); }

  virtual void serialize_data(T data, TransmitPacketInterface& packet) = 0;
  virtual T deserialize_data(ReceivePacketBuffer& packet) = 0;
  
protected:
  Telemetry& telemetry_container;
  size_t data_id;
  T value[array_count];
  T min_val, max_val;
  bool frozen;
};

template <typename T, uint32_t array_count>
class NumericArrayAccessor {
public:
  NumericArrayAccessor(NumericArrayBase<T, array_count>& container, size_t index) :
    container(container), index(index) { }

  T operator = (T b) {
    if (!container.frozen) {
      container.value[index] = b;
      container.telemetry_container.mark_data_updated(container.data_id);
    }
    return container.value[index];
  }

  operator T() {
    return container.value[index];
  }

protected:
  NumericArrayBase<T, array_count>& container;
  size_t index;
};

template <typename T, uint32_t array_count> 
class NumericArray : public NumericArrayBase<T, array_count> {
  NumericArray(Telemetry& telemetry_container, 
      const char* internal_name, const char* display_name,
      const char* units, T elem_init_value);
  virtual uint8_t get_subtype();
  virtual void write_payload(TransmitPacketInterface& packet);
  virtual T deserialize_data(ReceivePacketBuffer& packet);
};

template <uint32_t array_count> 
class NumericArray<uint8_t, array_count> : public NumericArrayBase<uint8_t, array_count> {
public:
  NumericArray(Telemetry& telemetry_container, 
      const char* internal_name, const char* display_name,
      const char* units, uint8_t elem_init_value):
      NumericArrayBase<uint8_t, array_count>(
          telemetry_container, internal_name, display_name,
          units, elem_init_value) {};
  virtual uint8_t get_subtype() {return NUMERIC_SUBTYPE_UINT; }
  virtual void serialize_data(uint8_t data, TransmitPacketInterface& packet) {
    packet.write_uint8(data); }
  virtual uint8_t deserialize_data(ReceivePacketBuffer& packet) {
    return packet.read_uint8(); }
};

template <uint32_t array_count> 
class NumericArray<uint16_t, array_count> : public NumericArrayBase<uint16_t, array_count> {
public:
  NumericArray(Telemetry& telemetry_container, 
      const char* internal_name, const char* display_name,
      const char* units, uint16_t elem_init_value):
      NumericArrayBase<uint16_t, array_count>(
          telemetry_container, internal_name, display_name,
          units, elem_init_value) {};
  virtual uint8_t get_subtype() {return NUMERIC_SUBTYPE_UINT; }
  virtual void serialize_data(uint16_t data, TransmitPacketInterface& packet) {
    packet.write_uint16(data); }
  virtual uint16_t deserialize_data(ReceivePacketBuffer& packet) {
    return packet.read_uint16(); }
};

template <uint32_t array_count> 
class NumericArray<uint32_t, array_count> : public NumericArrayBase<uint32_t, array_count> {
public:
  NumericArray(Telemetry& telemetry_container, 
      const char* internal_name, const char* display_name,
      const char* units, uint32_t elem_init_value):
      NumericArrayBase<uint32_t, array_count>(
          telemetry_container, internal_name, display_name,
          units, elem_init_value) {};
  virtual uint8_t get_subtype() {return NUMERIC_SUBTYPE_UINT; }
  virtual void serialize_data(uint32_t data, TransmitPacketInterface& packet) {
    packet.write_uint32(data); }
  virtual uint32_t deserialize_data(ReceivePacketBuffer& packet) {
    return packet.read_uint32(); }
};

template <uint32_t array_count> 
class NumericArray<float, array_count> : public NumericArrayBase<float, array_count> {
public:
  NumericArray(Telemetry& telemetry_container, 
      const char* internal_name, const char* display_name,
      const char* units, float elem_init_value):
      NumericArrayBase<float, array_count>(
          telemetry_container, internal_name, display_name,
          units, elem_init_value) {};
  virtual uint8_t get_subtype() {return NUMERIC_SUBTYPE_FLOAT; }
  virtual void serialize_data(float data, TransmitPacketInterface& packet) {
    packet.write_float(data); }
  virtual float deserialize_data(ReceivePacketBuffer& packet) {
    return packet.read_float(); }
};

}

#endif
