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

void Telemetry::add_data(Data& new_data) {
  if (data_count >= MAX_DATA_PER_TELEMETRY) {
    do_error("MAX_DATA_PER_TELEMETRY limit reached.");
    return;
  }
  if (header_transmitted) {
    do_error("Cannot add new data after header transmitted.");
    return;
  }
  data[data_count] = &new_data;
  data_count++;
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

  size_t packet_legnth = 2; // opcode + sequence
  for (int data_idx = 0; data_idx < data_count; data_idx++) {
    packet_legnth += 1; // data ID
    packet_legnth += data[data_idx]->get_payload_length();
  }
  packet_legnth++;  // terminator "record"

  FixedLengthTransmitPacket packet(hal, packet_legnth);

  packet.write_uint8(OPCODE_HEADER);
  packet.write_uint8(packet_tx_sequence);
  for (int data_idx = 0; data_idx < data_count; data_idx++) {
    packet.write_uint8(data_idx+1);
    data[data_idx]->write_payload(packet);
  }
  packet.write_uint8(DATAID_TERMINATOR);

  packet.finish();

  packet_tx_sequence++;
}

void Telemetry::process_received_data() {
  // TODO: implement me
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
  if (length > MAX_PACKET_LENGTH) {
    hal.do_error("Packet exceeds maximum length");
    valid = false;
    return;
  }

  hal.transmit_byte(SOF1);
  hal.transmit_byte(SOF2);

  hal.transmit_byte((length >> 8) & 0xff);
  hal.transmit_byte((length >> 0) & 0xff);

  valid = true;
}

void FixedLengthTransmitPacket::write_uint8(uint8_t data) {
  if (!valid) {
    hal.do_error("Writing to invalid packet");
    return;
  } else if (count + 1 > length) {
    hal.do_error("Writing over packet length");
    return;
  }
  hal.transmit_byte(data);
  count++;
}

void FixedLengthTransmitPacket::write_uint16(uint16_t data) {
  if (!valid) {
    hal.do_error("Writing to invalid packet");
    return;
  } else if (count + 2 > length) {
    hal.do_error("Writing over packet length");
    return;
  }
  hal.transmit_byte((data >> 8) & 0xff);
  hal.transmit_byte((data >> 0) & 0xff);
  count += 2;
}

void FixedLengthTransmitPacket::write_uint32(uint32_t data) {
  if (!valid) {
    hal.do_error("Writing to invalid packet");
    return;
  } else if (count + 4 > length) {
    hal.do_error("Writing over packet length");
    return;
  }
  hal.transmit_byte((data >> 24) & 0xff);
  hal.transmit_byte((data >> 16) & 0xff);
  hal.transmit_byte((data >> 8) & 0xff);
  hal.transmit_byte((data >> 0) & 0xff);
  count += 4;
}

void FixedLengthTransmitPacket::write_float(float data) {
  if (!valid) {
    hal.do_error("Writing to invalid packet");
    return;
  } else if (count + 4 > length) {
    hal.do_error("Writing over packet length");
    return;
  }
  // TODO: THIS IS ENDIANNESS DEPENDENT, ABSTRACT INTO HAL?
  uint8_t *float_array = (uint8_t*) &data;
  hal.transmit_byte(float_array[0]);
  hal.transmit_byte(float_array[1]);
  hal.transmit_byte(float_array[2]);
  hal.transmit_byte(float_array[3]);
  count += 4;
}

void FixedLengthTransmitPacket::finish() {
  if (!valid) {
    hal.do_error("Finishing invalid packet");
  } else if (count != length) {
    hal.do_error("Packet under length");
  }

  // TODO: add CRC check here
}

}
