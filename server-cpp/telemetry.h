#ifndef _TELEMETRY_H_
#define _TELEMETRY_H_

#include <stddef.h>
#include <stdint.h>

// Maximum number of Data objects a Telemetry object can hold.
// Default given here, but can be redefined with a compiler define.
#ifndef TELEMETRY_DATA_LIMIT
#define TELEMETRY_DATA_LIMIT 16
#endif

#ifndef TELEMETRY_SERIAL_RX_BUFFER_SIZE
#define TELEMETRY_SERIAL_RX_BUFFER_SIZE 256
#endif

namespace telemetry {
// Maximum number of Data objects a Telemetry object can hold.
// Used for array sizing.
const size_t MAX_DATA_PER_TELEMETRY = TELEMETRY_DATA_LIMIT;

// Maximum payload size for a received telemetry packet.
const size_t MAX_RECEIVE_PACKET_LENGTH = 255;

// Time after which a partially received packet is discarded.
const uint32_t DECODER_TIMEOUT_MS = 100;

// Buffer size for received non-telemetry data.
const size_t SERIAL_RX_BUFFER_SIZE = TELEMETRY_SERIAL_RX_BUFFER_SIZE;
}

#ifdef ARDUINO
  #ifdef TELEMETRY_HAL
    #error "Multiple telemetry HALs defined"
  #endif
  #include "telemetry-arduino-hal.h"
#endif

#if defined(__MBED__)
  #ifdef TELEMETRY_HAL
    #error "Multiple telemetry HALs defined"
  #endif
  #include "telemetry-mbed-hal.h"
#endif

#ifndef TELEMETRY_HAL
  #error "No telemetry HAL defined"
#endif

#include "protocol.h"
#include "packet.h"
#include "queue.h"

namespace telemetry {
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
  virtual void write_header_kvrs(TransmitPacket& packet);

  // Returns the length of the payload, in bytes. Should be "fast".
  virtual size_t get_payload_length() = 0;
  // Writes the payload to the transmit packet. Should be "fast".
  virtual void write_payload(TransmitPacket& packet) = 0;

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
  // Returns whether or not read_receive will return valid data.
  bool receive_available();
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

  Queue<uint8_t, SERIAL_RX_BUFFER_SIZE> rx_buffer;

  bool header_transmitted;

  // Sequence number of the next packet to be transmitted.
  uint8_t packet_tx_sequence;
  uint8_t packet_rx_sequence; // TODO use this somewhere
};

template <typename T>
class Numeric : public Data {
public:
  Numeric(Telemetry& telemetry_container,
      const char* internal_name, const char* display_name,
      const char* units, T init_value):
      Data(internal_name, display_name, units),
      telemetry_container(telemetry_container),
      value(init_value), min_val(init_value), max_val(init_value) {
    data_id = telemetry_container.add_data(*this);
  }

  T operator = (T b) {
    value = b;
    telemetry_container.mark_data_updated(data_id);
    return b;
  }

  operator T() {
    return value;
  }

  Numeric<T>& set_limits(T min, T max) {
    min_val = min;
    max_val = max;
    return *this;
  }

  uint8_t get_data_type() { return protocol::DATATYPE_NUMERIC; }

  size_t get_header_kvrs_length() {
    return Data::get_header_kvrs_length()
        + 1 + 1   // subtype
        + 1 + 1   // data length
        + 1 + sizeof(value) + sizeof(value);  // limits
  }

  void write_header_kvrs(TransmitPacket& packet) {
    Data::write_header_kvrs(packet);
    packet.write_uint8(protocol::RECORDID_NUMERIC_SUBTYPE);
    packet.write_uint8(protocol::numeric_subtype<T>());
    packet.write_uint8(protocol::RECORDID_NUMERIC_LENGTH);
    packet.write_uint8(sizeof(value));
    packet.write_uint8(protocol::RECORDID_NUMERIC_LIMITS);
    serialize_data(min_val, packet);
    serialize_data(max_val, packet);
  }

  size_t get_payload_length() { return sizeof(value); }
  void write_payload(TransmitPacket& packet) { serialize_data(value, packet); }
  void set_from_packet(ReceivePacketBuffer& packet) {
    value = deserialize_data(packet);
    telemetry_container.mark_data_updated(data_id); }

  void serialize_data(T value, TransmitPacket& packet) {
    packet.write<T>(value);
  }
  T deserialize_data(ReceivePacketBuffer& packet) {
    return packet.read<T>();
  }


protected:
  Telemetry& telemetry_container;
  size_t data_id;
  T value;
  T min_val, max_val;
};

template <typename T, uint32_t array_count>
class NumericArrayAccessor;

template <typename T, uint32_t array_count>
class NumericArray : public Data {
  friend class NumericArrayAccessor<T, array_count>;
public:
  NumericArray(Telemetry& telemetry_container,
      const char* internal_name, const char* display_name,
      const char* units, T elem_init_value):
      Data(internal_name, display_name, units),
      telemetry_container(telemetry_container),
      min_val(elem_init_value), max_val(elem_init_value) {
    for (size_t i=0; i<array_count; i++) {
      value[i] = elem_init_value;
    }
    data_id = telemetry_container.add_data(*this);
  }

  NumericArrayAccessor<T, array_count> operator[] (const int index) {
    // TODO: add bounds checking here?
    return NumericArrayAccessor<T, array_count>(*this, index);
  }

  NumericArray<T, array_count>& set_limits(T min, T max) {
    min_val = min;
    max_val = max;
    return *this;
  }

  uint8_t get_data_type() { return protocol::DATATYPE_NUMERIC_ARRAY; }

  size_t get_header_kvrs_length() {
    return Data::get_header_kvrs_length()
        + 1 + 1   // subtype
        + 1 + 1   // data length
        + 1 + 4   // array length
        + 1 + sizeof(value[0]) + sizeof(value[0]);  // limits
  }

  void write_header_kvrs(TransmitPacket& packet) {
    Data::write_header_kvrs(packet);
    packet.write_uint8(protocol::RECORDID_NUMERIC_SUBTYPE);
    packet.write_uint8(protocol::numeric_subtype<T>());
    packet.write_uint8(protocol::RECORDID_NUMERIC_LENGTH);
    packet.write_uint8(sizeof(value[0]));
    packet.write_uint8(protocol::RECORDID_ARRAY_COUNT);
    packet.write_uint32(array_count);
    packet.write_uint8(protocol::RECORDID_NUMERIC_LIMITS);
    serialize_data(min_val, packet);
    serialize_data(max_val, packet);
  }

  size_t get_payload_length() { return sizeof(value); }
  void write_payload(TransmitPacket& packet) {
    for (size_t i=0; i<array_count; i++) { serialize_data(this->value[i], packet); } }
  void set_from_packet(ReceivePacketBuffer& packet) {
    for (size_t i=0; i<array_count; i++) { value[i] = deserialize_data(packet); }
    telemetry_container.mark_data_updated(data_id); }

  void serialize_data(T data, TransmitPacket& packet) {
    packet.write<T>(data); }
  T deserialize_data(ReceivePacketBuffer& packet) {
    return packet.read<T>(); }

protected:
  Telemetry& telemetry_container;
  size_t data_id;
  T value[array_count];
  T min_val, max_val;
};

template <typename T, uint32_t array_count>
class NumericArrayAccessor {
public:
  NumericArrayAccessor(NumericArray<T, array_count>& container, size_t index) :
    container(container), index(index) { }

  T operator = (T b) {
    container.value[index] = b;
    container.telemetry_container.mark_data_updated(container.data_id);
    return b;
  }

  operator T() {
    return container.value[index];
  }

protected:
  NumericArray<T, array_count>& container;
  size_t index;
};

}

#endif
