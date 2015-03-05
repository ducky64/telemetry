from collections import namedtuple, deque
import time

import serial
from test.test_dis import outer

# lifted from https://stackoverflow.com/questions/36932/how-can-i-represent-an-enum-in-python
def enum(*sequential, **named):
    enums = dict(zip(sequential, range(len(sequential))), **named)
    return type('Enum', (), enums)

# Global constants defined by the telemetry protocol.
# TODO: parse constants from cpp header
SOF_BYTE = [0x05, 0x39]

OPCODE_HEADER = 0x81
OPCODE_DATA = 0x01

DATAID_TERMINATOR = 0x00

DATATYPE_NUMERIC = 0x01
DATATYPE_NUMERIC_ARRAY = 0x02

NUMERIC_SUBTYPE_UINT = 0x01
NUMERIC_SUBTYPE_SINT = 0x02
NUMERIC_SUBTYPE_FLOAT = 0x03

RECORDID_TERMINATOR = 0x00

# Deserialization functions that (destructively) reads data from the input "stream".
def deserialize_uint8(byte_stream):
# TODO: handle overflow
  return byte_stream.popleft()

def deserialize_bool(byte_stream):
# TODO: handle overflow
  return not(byte_stream.popleft() == 0)

def deserialize_uint16(byte_stream):
  # TODO: handle overflow
  return byte_stream.popleft() << 8 | byte_stream.popleft()

def deserialize_uint32(byte_stream):
  # TODO: handle overflow
  return (byte_stream.popleft() << 24 
         | byte_stream.popleft() << 16
         | byte_stream.popleft() << 8
         | byte_stream.popleft())

def deserialize_string(byte_stream):
  # TODO: handle overflow
  outstr = ""
  data = byte_stream.popleft()
  while data:
    outstr += chr(data)
    data = byte_stream.popleft()
  return outstr

PACKET_LENGTH_BYTES = 2 # number of bytes in the packet length field

class TelemetryDeserializationError(Exception):
  pass

class NoRecordIdError(TelemetryDeserializationError):
  pass
class MissingKvrError(TelemetryDeserializationError):
  pass

datatype_registry = {}
class TelemetryData:
  def __repr__(self):
    out = self.__class__.__name__
    for _, record_desc in self.get_kvrs_dict().items():
      record_name, _ = record_desc
      out += " %s=%s" % (record_name, repr(getattr(self, record_name)))
    return out
  
  @classmethod
  def get_kvrs_dict(cls):
    """Returns a dict of record id => (record name, deserialization function) known by this class.
    The record name is used as an instance variable name if the KVR is read in.
    """
    return {
      0x01: ('internal_name', deserialize_string),
      0x02: ('display_name', deserialize_string),
      0x03: ('units', deserialize_string),
      # TODO: make more robust by allowing defaults / partial parses
      # add record 0x08 = freeze
    }
  
  @staticmethod
  def decode_header(data_id, byte_stream):
    """Decodes a data header from the telemetry stream, automatically detecting and returning
    the correct TelemetryData subclass object.
    """
    opcode = byte_stream[0]
    if opcode not in datatype_registry:
      raise NoOpcodeError("No opcode %02x" % opcode)
    data_cls = datatype_registry[opcode]
    return data_cls(data_id, byte_stream)
  
  def __init__(self, data_id, byte_stream):
    self.data_id = data_id
    self.data_type = deserialize_uint8(byte_stream)
    
    self.internal_name = "%02x" % data_id
    self.display_name = self.internal_name
    self.units = ""
    
    self.decode_kvrs(byte_stream)
    
  def decode_kvrs(self, byte_stream):
    """Destructively reads in a sequence of KVRs from the input stream, writing
    the known ones as instance variables and throwing exceptions on unknowns.
    """
    kvrs_dict = self.get_kvrs_dict()
    while True:
      record_id = deserialize_uint8(byte_stream)
      if record_id == RECORDID_TERMINATOR:
        break
      elif record_id not in kvrs_dict:
        raise NoRecordIdError("No RecordId %02x in %s" % (record_id, self.__class__.__name__))
      record_name, record_deserializer = kvrs_dict[record_id]
      setattr(self, record_name, record_deserializer(byte_stream))
      
    # check that all KVRs have been read in / defaulted
    for record_id, record_desc in kvrs_dict.items():
      record_name, _ = record_desc
      if not hasattr(self, record_name):
        raise NoRecordIdError("%s missing record %02x: %s" % (self.__class__.__name__, record_id, record_name))

  def deserialize_data(self, byte_stream):
    """Destructively reads in the data of this type from the input stream.
    """
    raise NotImplementedError
  
  
  
class UnknownNumericSubtype(Exception):
  pass
  
class NumericData(TelemetryData):
  @classmethod
  def get_kvrs_dict(cls):
    newdict = super(NumericData, cls).get_kvrs_dict().copy()
    newdict.update({ 
      0x40: ('subtype', deserialize_uint8),
      0x41: ('length', deserialize_uint8),
    })
    return newdict
    
  def deserialize_data(self, byte_stream):
    if self.subtype == NUMERIC_SUBTYPE_UINT:
      value = 0
      remaining = self.length
      while remaining > 0:
        value = value << 8 | deserialize_uint8(byte_stream)
        remaining -= 1
      return value
      # TODO: add support for sint / floats
    else:
      raise UnknownNumericSubtype("Unknown subtype %02x" % self.subtype)
  
datatype_registry[DATATYPE_NUMERIC] = NumericData

class NumericArrayData(TelemetryData):
  @classmethod
  def get_kvrs_dict(cls):
    newdict = super(NumericData, cls).get_kvrs_dict().copy()
    newdict.update({ 
      0x40: ('subtype', deserialize_uint8),
      0x41: ('length', deserialize_uint8),
      0x42: ('count', deserialize_uint8),
    })
    return newdict 
datatype_registry[DATATYPE_NUMERIC_ARRAY] = NumericArrayData



class NoOpcodeError(TelemetryDeserializationError):
  pass
class DuplicateDataIdError(TelemetryDeserializationError):
  pass

opcodes_registry = {}
class TelemetryPacket:
  @staticmethod
  def decode(byte_stream):
    opcode = byte_stream[0]
    if opcode not in opcodes_registry:
      raise NoOpcodeError("No opcode %02x" % opcode)
    packet_cls = opcodes_registry[opcode]
    return packet_cls(byte_stream)
  
  def __init__(self, byte_stream):
    self.opcode = deserialize_uint8(byte_stream)
    self.sequence = deserialize_uint8(byte_stream)
    self.decode_payload(byte_stream)
    
  def decode_payload(self, byte_stream):
    raise NotImplementedError

class HeaderPacket(TelemetryPacket):
  def __repr__(self):
    return "Header [%i]: %s" % (self.sequence, repr(self.data))
    
  def decode_payload(self, byte_stream):
    self.data = {}
    while True:
      data_id = deserialize_uint8(byte_stream)
      if data_id == DATAID_TERMINATOR:
        break
      elif data_id in self.data:
        raise DuplicateDataIdError("Duplicate DataId %02x" % data_id)
      self.data[data_id] = TelemetryData.decode_header(data_id, byte_stream)
      
opcodes_registry[OPCODE_HEADER] = HeaderPacket

class DataPacket(TelemetryPacket):
  pass

opcodes_registry[OPCODE_DATA] = HeaderPacket  



class TelemetrySerial:
  DecoderState = enum('SOF', 'LENGTH', 'DATA')
  
  def __init__(self, serial):
    self.serial = serial
    
    self.rx_packets = deque()  # queued decoded packets
    
    # decoder state machine variables
    self.decoder_state = self.DecoderState.SOF;  # expected next byte
    self.decoder_pos = 0; # position within decoder_State
    self.packet_length = 0;  # expected packet length
    self.packet_buffer = deque()
    
    self.data_buffer = deque()

  def process_rx(self):
    while self.serial.inWaiting():
      rx_byte = ord(self.serial.read())
      if self.decoder_state == self.DecoderState.SOF:
        self.packet_buffer.append(rx_byte)
        
        if rx_byte == SOF_BYTE[self.decoder_pos]:
          self.decoder_pos += 1
          if self.decoder_pos == len(SOF_BYTE):
            self.packet_length = 0
            self.decoder_pos = 0
            self.decoder_state = self.DecoderState.LENGTH
        else:
          self.data_buffer.extend(self.packet_buffer)
          for byte in self.packet_buffer:
            print(chr(byte), end="")
          self.packet_buffer = deque()
          self.decoder_pos = 0
      elif self.decoder_state == self.DecoderState.LENGTH:
        self.packet_length = self.packet_length << 8 | rx_byte
        self.decoder_pos += 1
        if self.decoder_pos == PACKET_LENGTH_BYTES:
          self.packet_buffer = deque()
          self.decoder_pos = 0
          self.decoder_state = self.DecoderState.DATA
      elif self.decoder_state == self.DecoderState.DATA:
        self.packet_buffer.append(rx_byte)
        self.decoder_pos += 1
        if self.decoder_pos == self.packet_length:
          decoded = TelemetryPacket.decode(self.packet_buffer)
          self.rx_packets.append(decoded)
          print("")
          print(decoded)
          self.packet_buffer = deque()
          
          self.decoder_pos = 0
          self.decoder_state = self.DecoderState.SOF
          # TODO: add byte destuffing
      else:
        raise RuntimeError("Unknown DecoderState")    

if __name__ == "__main__":
  telemetry = TelemetrySerial(serial.Serial("COM61", baudrate=1000000))
  while True:
    telemetry.process_rx()
    time.sleep(0.1)
    