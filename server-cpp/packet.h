#ifndef _PACKET_H_
#define _PACKET_H_

namespace telemetry {
class TransmitPacket;
class ReceivePacketBuffer;

namespace internal {
  template<typename T> void pkt_write(TransmitPacket& interface, T data);
  template<typename T> T buf_read(ReceivePacketBuffer& buffer);
}

// Abstract base class for building a packet to be transmitted.
// Implementation is unconstrained - writes may either be buffered or passed
// directly to the hardware transmit buffers.
class TransmitPacket {
public:
  virtual ~TransmitPacket() {}

  // Writes a 8-bit unsigned integer to the packet stream.
  virtual void write_uint8(uint8_t data) = 0;
  // Writes a 16-bit unsigned integer to the packet stream.
  virtual void write_uint16(uint16_t data) = 0;
  // Writes a 32-bit unsigned integer to the packet stream.
  virtual void write_uint32(uint32_t data) = 0;
  // Writes a float to the packet stream.
  virtual void write_float(float data) = 0;

  // Generic templated write operations.
  template<typename T> void write(T data) {
    internal::pkt_write<T>(*this, data);
  }

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

  // Generic templated write operations.
  template<typename T> T read() {
	return internal::buf_read<T>(*this);
  }

protected:
  HalInterface& hal;

  size_t packet_length;
  size_t read_loc;
  uint8_t data[MAX_RECEIVE_PACKET_LENGTH];
};

// A telemetry packet with a length known before data is written to it.
// Data is written directly to the hardware transmit buffers without packet
// buffering. Assumes transmit buffers won't fill up.
class FixedLengthTransmitPacket : public TransmitPacket {
public:
  FixedLengthTransmitPacket(HalInterface& hal, size_t length);

  void write_byte(uint8_t data);

  void write_uint8(uint8_t data);
  void write_uint16(uint16_t data);
  void write_uint32(uint32_t data);
  void write_float(float data);

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

}

#endif
