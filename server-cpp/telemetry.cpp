/*
 * telemetry.cpp
 *
 *  Created on: Mar 2, 2015
 *      Author: Ducky
 *
 * Implementation for the base Telemetry class.
 */

#include "telemetry.h"

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

  packet.write_uint8(protocol::OPCODE_HEADER);
  packet.write_uint8(packet_tx_sequence);
  for (int data_idx = 0; data_idx < data_count; data_idx++) {
    packet.write_uint8(data_idx+1);
    packet.write_uint8(data[data_idx]->get_data_type());
    data[data_idx]->write_header_kvrs(packet);
    packet.write_uint8(protocol::RECORDID_TERMINATOR);
  }
  packet.write_uint8(protocol::DATAID_TERMINATOR);

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

  packet.write_uint8(protocol::OPCODE_DATA);
  packet.write_uint8(packet_tx_sequence);
  for (int data_idx = 0; data_idx < data_count; data_idx++) {
    if (data_updated_local[data_idx]) {
      packet.write_uint8(data_idx+1);
      data[data_idx]->write_payload(packet);
    }
  }
  packet.write_uint8(protocol::DATAID_TERMINATOR);

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
      if (rx_byte == protocol::SOF_SEQ[decoder_pos]) {
        decoder_pos++;
        if (decoder_pos >= protocol::SOF_LENGTH) {
          decoder_pos = 0;
          packet_length = 0;
          decoder_state = LENGTH;
        }
      } else {
        if (decoder_pos > 0) {
          // Pass through any partial SOF sequence.
          for (uint8_t i=0; i<decoder_pos; i++) {
            rx_buffer.enqueue(protocol::SOF_SEQ[i]);
          }
        }
        decoder_pos = 0;
        rx_buffer.enqueue(rx_byte);
      }
    } else if (decoder_state == LENGTH) {
      packet_length = (packet_length << 8) | rx_byte;
      decoder_pos++;
      if (decoder_pos >= protocol::LENGTH_SIZE) {
        decoder_pos = 0;
        decoder_state = DATA;
      }
    } else if (decoder_state == DATA) {
      received_packet.add_byte(rx_byte);
      decoder_pos++;
      if (decoder_pos >= packet_length) {
        process_received_packet();

        decoder_pos = 0;
        if (rx_byte == protocol::SOF_SEQ[0]) {
          decoder_state = DATA_DESTUFF_END;
        } else {
          decoder_state = SOF;
        }
      } else {
        if (rx_byte == protocol::SOF_SEQ[0]) {
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
  if (opcode == protocol::OPCODE_DATA) {
    uint8_t data_id = received_packet.read_uint8();
    while (data_id != protocol::DATAID_TERMINATOR) {
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

bool Telemetry::receive_available() {
  return !rx_buffer.empty();
}

uint8_t Telemetry::read_receive() {
  uint8_t rx_data = '#';
  if (rx_buffer.dequeue(&rx_data)) {
    return rx_data;
  } else {
    return 255;
  }
}

}
