/*
 * telemetry.cpp
 *
 *  Created on: Mar 2, 2015
 *      Author: Ducky
 *
 * Implementation for the base Telemetry class.
 */

#include <telemetry.h>

namespace telemetry {

size_t Telemetry::add_data(Data& new_data) {
  if (data_count >= MAX_DATA_PER_TELEMETRY) {
    do_error("MAX_DATA_PER_TELEMETRY limit reached.");
    return 0;
  }
  if (header_transmitted) {
    do_error("Cannot add new data after header transmitted.");
    return 0;
  }
  data[data_count] = &new_data;
  data_updated[data_count] = true;
  data_count++;
  return data_count - 1;
}

void Telemetry::mark_data_updated(size_t data_id) {
  data_updated[data_id] = true;
}

void Telemetry::transmit_header() {
  if (header_transmitted) {
    do_error("Cannot retransmit header.");
    return;
  }

  size_t packet_legnth = 2; // opcode + sequence
  for (int data_idx = 0; data_idx < data_count; data_idx++) {
    packet_legnth += 2; // data ID, data type
    packet_legnth += data[data_idx]->get_header_kvrs_length();
    packet_legnth += 1; // terminator record id
  }
  packet_legnth++;  // terminator "record"

  FixedLengthTransmitPacket packet(hal, packet_legnth);

  packet.write_uint8(OPCODE_HEADER);
  packet.write_uint8(packet_tx_sequence);
  for (int data_idx = 0; data_idx < data_count; data_idx++) {
    packet.write_uint8(data_idx+1);
    packet.write_uint8(data[data_idx]->get_data_type());
    data[data_idx]->write_header_kvrs(packet);
    packet.write_uint8(RECORDID_TERMINATOR);
  }
  packet.write_uint8(DATAID_TERMINATOR);

  packet.finish();

  packet_tx_sequence++;
  header_transmitted = true;
}

void Telemetry::do_io() {
  transmit_data();
  process_received_data();
}

void Telemetry::transmit_data() {
  if (!header_transmitted) {
    do_error("Must transmit header before transmitting data.");
    return;
  }

  // Keep a local copy to make it more thread-safe
  bool data_updated_local[MAX_DATA_PER_TELEMETRY];

  size_t packet_legnth = 2; // opcode + sequence
  for (int data_idx = 0; data_idx < data_count; data_idx++) {
    data_updated_local[data_idx] = data_updated[data_idx];
    data_updated[data_idx] = 0;
    if (data_updated_local[data_idx]) {
      packet_legnth += 1; // data ID
      packet_legnth += data[data_idx]->get_payload_length();
    }
  }
  packet_legnth++;  // terminator "record"

  FixedLengthTransmitPacket packet(hal, packet_legnth);

  packet.write_uint8(OPCODE_DATA);
  packet.write_uint8(packet_tx_sequence);
  for (int data_idx = 0; data_idx < data_count; data_idx++) {
    if (data_updated_local[data_idx]) {
      packet.write_uint8(data_idx+1);
      data[data_idx]->write_payload(packet);
    }
  }
  packet.write_uint8(DATAID_TERMINATOR);

  packet.finish();

  packet_tx_sequence++;
}

void Telemetry::process_received_data() {
  uint32_t current_time = hal.get_time_ms();

  if (decoder_last_receive_ms <= current_time) {
	if (!decoder_last_received && decoder_state != SOF && decoder_pos != 0
		&& (decoder_last_receive_ms - current_time > DECODER_TIMEOUT_MS)) {
	  decoder_pos = 0;
	  packet_length = 0;
	  decoder_state = SOF;
	  hal.do_error("RX timeout");
	}
  } else {
	// timer overflowed, do nothing
  }
  decoder_last_receive_ms = current_time;

  decoder_last_received = false;
  while (hal.rx_available()) {
	decoder_last_received = true;

    uint8_t rx_byte = hal.receive_byte();

    if (decoder_state == SOF) {
      if (rx_byte == SOF_SEQ[decoder_pos]) {
        decoder_pos++;
        if (decoder_pos >= (sizeof(SOF_SEQ) / sizeof(SOF_SEQ[0]))) {
          decoder_pos = 0;
          packet_length = 0;
          decoder_state = LENGTH;
        }
      } else {
        decoder_pos = 0;
        // TODO: pass rest of data through
      }
    } else if (decoder_state == LENGTH) {
      packet_length = (packet_length << 8) | rx_byte;
      decoder_pos++;
      if (decoder_pos >= LENGTH_SIZE) {
        decoder_pos = 0;
        decoder_state = DATA;
      }
    } else if (decoder_state == DATA) {
      received_packet.add_byte(rx_byte);
      decoder_pos++;
      if (decoder_pos >= packet_length) {
        process_received_packet();

        decoder_pos = 0;
        if (rx_byte == SOF_SEQ[0]) {
          decoder_state = DATA_DESTUFF_END;
        } else {
          decoder_state = SOF;
        }
      } else {
        if (rx_byte == SOF_SEQ[0]) {
          decoder_state = DATA_DESTUFF;
        }
      }
    } else if (decoder_state == DATA_DESTUFF) {
      decoder_state = DATA;
    } else if (decoder_state == DATA_DESTUFF_END) {
      decoder_state = SOF;
    }
  }
}

void Telemetry::process_received_packet() {
  uint8_t opcode = received_packet.read_uint8();
  if (opcode == OPCODE_DATA) {
    uint8_t data_id = received_packet.read_uint8();
    while (data_id != DATAID_TERMINATOR) {
      if (data_id < data_count + 1) {
        data[data_id - 1]->set_from_packet(received_packet);
      } else {
        hal.do_error("Unknown data ID");
      }
      data_id = received_packet.read_uint8();
    }
  } else {
    hal.do_error("Unknown opcode");
  }
}

size_t Telemetry::receive_available() {
  // TODO: implement me
  return 0;
}

uint8_t read_receive() {
  // TODO: implement me
  return 0;
}

FixedLengthTransmitPacket::FixedLengthTransmitPacket(HalInterface& hal,
    size_t length) :
        hal(hal),
        length(length),
        count(0) {
  hal.transmit_byte(SOF1);
  hal.transmit_byte(SOF2);

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
  if (data == SOF1) {
    hal.transmit_byte(0x00);  // TODO: proper abstraction and magic numbers
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
