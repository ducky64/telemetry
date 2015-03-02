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

void Telemetry::add_data(DataInterface& new_data) {
  if (data_count >= MAX_DATA_PER_TELEMETRY) {
    error("MAX_DATA_PER_TELEMETRY limit reached.");
    return;
  }
  if (header_transmitted) {
    error("Cannot add new data after header transmitted.");
    return;
  }
  data[data_count] = &new_data;
  data_count++;
}

void Telemetry::transmit_header() {
  if (header_transmitted) {
    error("Cannot retransmit header.");
    return;
  }

  size_t packet_legnth = 2; // opcode + sequence
  for (int data_idx = 0; data_idx < data_count; data_idx++) {
    packet_legnth += 2; // data ID, data type
    packet_legnth += data[data_idx]->get_header_kvrs_length();
    // TODO: assert length is never zero?
  }
  packet_legnth++;  // terminator "record"

  FixedLengthTransmitPacket packet(packet_legnth);

  packet.write_uint8(OPCODE_HEADER);
  packet.write_uint8(packet_tx_sequence);
  for (int data_idx = 0; data_idx < data_count; data_idx++) {
    packet.write_uint8(data_idx+1);
    packet.write_uint8(data[data_idx]->get_data_type());
    data[data_idx]->write_header_kvrs(packet);
  }
  packet.write_uint8(DATAID_TERMINATOR);

  packet.finish();

  packet_tx_sequence++;
  header_transmitted = true;
}

void Telemetry::transmit_data() {
  if (!header_transmitted) {
    error("Must transmit header before transmitting data.");
    return;
  }

  size_t packet_legnth = 2; // opcode + sequence
  for (int data_idx = 0; data_idx < data_count; data_idx++) {
    packet_legnth += 1; // data ID
    packet_legnth += data[data_idx]->get_payload_length();
  }
  packet_legnth++;  // terminator "record"

  FixedLengthTransmitPacket packet(packet_legnth);

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

}
