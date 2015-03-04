from collections import namedtuple, deque
import time

import serial

TelemetryPacket = namedtuple('TelemetryPacket', ['opcode', 'sequence', 'payload'])

KvRecord = namedtuple('KvRecord', ['record_id', 'value'])
DataHeader = namedtuple('DataHeader', ['data_id', 'data_type', 'kv_records'])
HeaderPacket = namedtuple('DataDefinition', ['data_headers'])

DataRecord = namedtuple('DataRecord', ['data_id', 'value'])
DataPacket = namedtuple('Data', ['data_records'])

# TODO: parse constants from cpp header
SOF_BYTE = [0x05, 0x39]

OPCODE_HEADER = 0x81
OPCODE_DATA = 0x01

DATAID_TERMINATOR = 0x00

DATATYPE_NUMERIC = 0x01
DATATYPE_NUMERIC_ARRAY = 0x02

RECORDID_TERMINATOR = 0x00
RECORDID_INTERNAL_NAME = 0x01
RECORDID_DISPLAY_NAME = 0x02
RECORDID_UNITS = 0x03

RECORDID_OVERRIDE_CTL = 0x08
RECORDID_OVERRIDE_DATA = 0x08

RECORDID_NUMERIC_SUBTYPE = 0x40
RECORDID_NUMERIC_LENGTH = 0x41

RECORDID_NUMERIC_ARRAY_SUBTYPE = 0x40
RECORDID_NUMERIC_ARRAY_LENGTH = 0x41
RECORDID_NUMERIC_ARRAY_COUNT = 0x42

RECORDID_GLOBAL_END = 0x40

PACKET_LENGTH_BYTES = 2

# lifted from https://stackoverflow.com/questions/36932/how-can-i-represent-an-enum-in-python
def enum(*sequential, **named):
    enums = dict(zip(sequential, range(len(sequential))), **named)
    return type('Enum', (), enums)
  
DecoderState = enum('SOF', 'LENGTH', 'DATA')

class TelemetryPacketDecoder():
  def __init__(self, bytes):
    self.bytes = bytes

  def read_uint8(self):
    return self.bytes.popleft()
  
  def read_uint16(self):
    return self.bytes.popleft() << 8 | self.bytes.popleft()

  def read_uint32(self):
    return (self.bytes.popleft() << 24 
           | self.bytes.popleft() << 16
           | self.bytes.popleft() << 8
           | self.bytes.popleft())
  
  def read_string(self):
    outstr = ""
    data = self.bytes.popleft()
    while data:
      outstr += chr(data)
      data = self.bytes.popleft()
    return outstr
  
  def decode(self):
    opcode = self.read_uint8()
    sequence = self.read_uint8()
    
    if opcode == OPCODE_HEADER:
      payload = self.decode_header_packet()
#     elif opcode == OPCODE_DATA:
#       payload = self.decode_data_packet()
    else:
      payload = self.bytes
    
    return TelemetryPacket(opcode, sequence, payload)
    
  def decode_header_packet(self):
    data_headers = {}
    next_data_header = self.decode_data_header()
    while next_data_header != DATAID_TERMINATOR:
      data_headers[next_data_header.data_id] = next_data_header
      next_data_header = self.decode_data_header()      
    return HeaderPacket(data_headers) 

  def decode_data_header(self):
    kvrs = {}
    data_id = self.read_uint8()
    if data_id == DATAID_TERMINATOR:
      return DATAID_TERMINATOR
    data_type = self.read_uint8()
    next_kvr = self.decode_kvr(data_type)
    while next_kvr != RECORDID_TERMINATOR:
      kvrs[next_kvr.record_id] = next_kvr
      next_kvr = self.decode_kvr(data_type)
    return DataHeader(data_id, data_type, kvrs)

  def decode_kvr(self, data_type):
    record_id = self.read_uint8()
    if record_id == RECORDID_TERMINATOR:
      return RECORDID_TERMINATOR
    
    if record_id < RECORDID_GLOBAL_END:
      if record_id == RECORDID_INTERNAL_NAME:
        return KvRecord('internal_name', self.read_string())
      elif record_id == RECORDID_DISPLAY_NAME:
        return KvRecord('display_name', self.read_string())
      elif record_id == RECORDID_UNITS:
        return KvRecord('units', self.read_string())
      else:
        raise RuntimeError("Unknown record ID %i for datatype %i" 
                           % (record_id, data_type))
    elif data_type == DATATYPE_NUMERIC:
      if (record_id == RECORDID_NUMERIC_SUBTYPE):
        return KvRecord('subtype', self.read_uint8())
      elif (record_id == RECORDID_NUMERIC_LENGTH):
        return KvRecord('length', self.read_uint8())
      else:
        raise RuntimeError("Unknown record ID %i for datatype %i" 
                           % (record_id, data_type))
    elif data_type == DATATYPE_NUMERIC_ARRAY:
      if (record_id == RECORDID_NUMERIC_ARRAY_SUBTYPE):
        return KvRecord('subtype', self.read_uint8())
      elif (record_id == RECORDID_NUMERIC_ARRAY_LENGTH):
        return KvRecord('length', self.read_uint8())
      elif (record_id == RECORDID_NUMERIC_ARRAY_COUNT):
        return KvRecord('count', self.read_uint8())
      else:
        raise RuntimeError("Unknown record ID %i for datatype %i" 
                           % (record_id, data_type))
        
  def decode_data_packet(self):
    pass

class TelemetrySerial:
  def __init__(self, port):
    self.serial = serial.Serial(port)
    
    self.rx_packets = deque()  # queued decoded packets
    
    # decoder state machine variables
    self.decoder_state = DecoderState.SOF;  # expected next byte
    self.decoder_pos = 0; # position within decoder_State
    self.packet_length = 0;  # expected packet length
    self.packet_buffer = deque()
    
    self.data_buffer = deque()

  def process_rx(self):
    while self.serial.inWaiting():
      rx_byte = ord(self.serial.read())
      if self.decoder_state == DecoderState.SOF:
        self.packet_buffer.append(rx_byte)
        
        if rx_byte == SOF_BYTE[self.decoder_pos]:
          self.decoder_pos += 1
          if self.decoder_pos == len(SOF_BYTE):
            self.packet_length = 0
            self.decoder_pos = 0
            self.decoder_state = DecoderState.LENGTH
        else:
          self.data_buffer.extend(self.packet_buffer)
          self.packet_buffer = deque()
          self.decoder_pos = 0
      elif self.decoder_state == DecoderState.LENGTH:
        self.packet_length = self.packet_length << 8 | rx_byte
        self.decoder_pos += 1
        if self.decoder_pos == PACKET_LENGTH_BYTES:
          self.packet_buffer = deque()
          self.decoder_pos = 0
          self.decoder_state = DecoderState.DATA
      elif self.decoder_state == DecoderState.DATA:
        self.packet_buffer.append(rx_byte)
        self.decoder_pos += 1
        if self.decoder_pos == self.packet_length:
          decoded = TelemetryPacketDecoder(self.packet_buffer).decode()
          self.rx_packets.append(decoded)
          print(decoded)
          self.packet_buffer = deque()
          
          self.decoder_pos = 0
          self.decoder_state = DecoderState.SOF
      else:
        raise RuntimeError("Unknown DecoderState")    

if __name__ == "__main__":
  telemetry = TelemetrySerial("COM14")
  while True:
    telemetry.process_rx()
    time.sleep(0.1)
    