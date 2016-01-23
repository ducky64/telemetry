/**
 * Transmit and receive packet interfaces
 */

#include "telemetry.h"

namespace telemetry {

namespace internal {
  template<> void pkt_write<uint8_t>(TransmitPacket& interface, uint8_t data) {
    interface.write_uint8(data);
  }
  template<> void pkt_write<uint16_t>(TransmitPacket& interface, uint16_t data) {
    interface.write_uint16(data);
  }
  template<> void pkt_write<uint32_t>(TransmitPacket& interface, uint32_t data) {
    interface.write_uint32(data);
  }
  template<> void pkt_write<float>(TransmitPacket& interface, float data) {
    interface.write_float(data);
  }

  template<> uint8_t buf_read<uint8_t>(ReceivePacketBuffer& buffer) {
    return buffer.read_float();
  }
  template<> uint16_t buf_read<uint16_t>(ReceivePacketBuffer& buffer) {
    return buffer.read_uint16();
  }
  template<> uint32_t buf_read<uint32_t>(ReceivePacketBuffer& buffer) {
    return buffer.read_uint32();
  }
  template<> float buf_read<float>(ReceivePacketBuffer& buffer) {
    return buffer.read_float();
  }
}

FixedLengthTransmitPacket::FixedLengthTransmitPacket(HalInterface& hal,
    size_t length) :
        hal(hal),
        length(length),
        count(0) {
  for (int i=0; i<protocol::SOF_LENGTH; i++) {
    hal.transmit_byte(protocol::SOF_SEQ[i]);
  }

  hal.transmit_byte((length >> 8) & 0xff);
  hal.transmit_byte((length >> 0) & 0xff);

  valid = true;
}

void FixedLengthTransmitPacket::write_byte(uint8_t data) {
  if (!valid) {
    hal.do_error("Writing to invalid packet");
    return;
  } else if (count + 1 > length) {
    hal.do_error("Writing over packet length");
    return;
  }
  hal.transmit_byte(data);
#if SOF_LENGTH > 2
#error "Byte stuffing algorithm does not work for SOF_LENGTH > 2"
#endif
  if (data == protocol::SOF_SEQ[0]) {
    hal.transmit_byte(protocol::SOF_SEQ0_STUFF);
  }
  count++;
}

void FixedLengthTransmitPacket::write_uint8(uint8_t data) {
  write_byte(data);
}

void FixedLengthTransmitPacket::write_uint16(uint16_t data) {
  write_byte((data >> 8) & 0xff);
  write_byte((data >> 0) & 0xff);
}

void FixedLengthTransmitPacket::write_uint32(uint32_t data) {
  write_byte((data >> 24) & 0xff);
  write_byte((data >> 16) & 0xff);
  write_byte((data >> 8) & 0xff);
  write_byte((data >> 0) & 0xff);
}

void FixedLengthTransmitPacket::write_float(float data) {
  // TODO: THIS IS ENDIANNESS DEPENDENT, ABSTRACT INTO HAL?
  uint8_t *float_array = (uint8_t*) &data;
  write_byte(float_array[3]);
  write_byte(float_array[2]);
  write_byte(float_array[1]);
  write_byte(float_array[0]);
}

void FixedLengthTransmitPacket::finish() {
  if (!valid) {
    hal.do_error("Finish invalid packet");
    return;
  } else if (count != length) {
    hal.do_error("TX packet under length");
    return;
  }

  // TODO: add CRC check here
}

ReceivePacketBuffer::ReceivePacketBuffer(HalInterface& hal) :
    hal(hal) {
  new_packet();
}

void ReceivePacketBuffer::new_packet() {
  packet_length = 0;
  read_loc = 0;
}

void ReceivePacketBuffer::add_byte(uint8_t byte) {
  if (packet_length >= MAX_RECEIVE_PACKET_LENGTH) {
    hal.do_error("RX packet over length");
    return;
  }

  data[packet_length] = byte;
  packet_length++;
}

uint8_t ReceivePacketBuffer::read_uint8() {
  if (read_loc + 1 > packet_length) {
    hal.do_error("Read uint8 over length");
    return 0;
  }
  read_loc += 1;
  return data[read_loc - 1];
}

uint16_t ReceivePacketBuffer::read_uint16() {
  if (read_loc + 2 > packet_length) {
    hal.do_error("Read uint16 over length");
    return 0;
  }
  read_loc += 2;
  return ((uint16_t)data[read_loc - 2] << 8)
       | ((uint16_t)data[read_loc - 1] << 0);
}

uint32_t ReceivePacketBuffer::read_uint32() {
  if (read_loc + 4 > packet_length) {
    hal.do_error("Read uint32 over length");
    return 0;
  }
  read_loc += 4;
  return ((uint32_t)data[read_loc - 4] << 24)
       | ((uint32_t)data[read_loc - 3] << 16)
       | ((uint32_t)data[read_loc - 2] << 8)
       | ((uint32_t)data[read_loc - 1] << 0);
}

float ReceivePacketBuffer::read_float() {
  if (read_loc + 4 > packet_length) {
    hal.do_error("Read float over length");
    return 0;
  }
  read_loc += 4;
  float out = 0;
  uint8_t* out_array = (uint8_t*)&out;
  out_array[0] = data[read_loc - 1];
  out_array[1] = data[read_loc - 2];
  out_array[2] = data[read_loc - 3];
  out_array[3] = data[read_loc - 4];
  return out;
}

}
