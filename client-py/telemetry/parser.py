from collections import deque
from numbers import Number
import struct
import time

# TODO: MASSIVE REFACTORING EVERYWHERE

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
  
def deserialize_float(byte_stream):
  # TODO: handle overflow
  packed = bytearray([byte_stream.popleft(), 
                      byte_stream.popleft(),
                      byte_stream.popleft(),
                      byte_stream.popleft()])
  return struct.unpack('!f', packed)[0]

def deserialize_numeric(byte_stream, subtype, length):
  if subtype == NUMERIC_SUBTYPE_UINT:
    value = 0
    remaining = length
    while remaining > 0:
      value = value << 8 | deserialize_uint8(byte_stream)
      remaining -= 1
    return value
    # TODO: add support for sint
  elif subtype == NUMERIC_SUBTYPE_FLOAT:
    if length == 4:
      return deserialize_float(byte_stream)
    else:
      raise UnknownNumericSubtype("Unknown float length %02x" % length)
  else:
    raise UnknownNumericSubtype("Unknown subtype %02x" % subtype)

def deserialize_numeric_from_def(data_def, count=None):
  def deserialize_numeric_inner(byte_stream):
    # these should have already been decoded
    assert hasattr(data_def, 'subtype')
    assert hasattr(data_def, 'length')
    if count is not None:
      inner_count = count
      out = []
      while inner_count > 0:
        out.append(deserialize_numeric(byte_stream, data_def.subtype, data_def.length))
        inner_count -= 1
      return out
    else:
      return deserialize_numeric(byte_stream, data_def.subtype, data_def.length)
  return deserialize_numeric_inner

def deserialize_string(byte_stream):
  # TODO: handle overflow
  outstr = ""
  data = byte_stream.popleft()
  while data:
    outstr += chr(data)
    data = byte_stream.popleft()
  return outstr



def serialize_uint8(value):
  if (not isinstance(value, int)) or (value < 0 or value > 255):
    raise ValueError("Invalid uint8: %s" % value)
  return struct.pack('!B', value)

def serialize_uint16(value):
  if (not isinstance(value, int)) or (value < 0 or value > 65535):
    raise ValueError("Invalid uint16: %s" % value) 
  return struct.pack('!H', value)

def serialize_uint32(value):
  if (not isinstance(value, int)) or (value < 0 or value > 2 ** 32 - 1): 
    raise ValueError("Invalid uint32: %s" % value)
  return struct.pack('!L', value)

def serialize_float(value):
  if not isinstance(value, Number):
    raise ValueError("Invalid uintfloat: %s" % value)
  return struct.pack('!f', value)

def serialize_numeric(value, subtype, length):
  if subtype == NUMERIC_SUBTYPE_UINT:
    if length == 1:
      return serialize_uint8(value)
    elif length == 2:
      return serialize_uint16(value)
    elif length == 4:
      return serialize_uint32(value)
    else:
      raise ValueError("Unknown uint length %02x" % length)
  elif subtype == NUMERIC_SUBTYPE_FLOAT:
    if length == 4:
      return serialize_float(value)
    else:
      raise ValueError("Unknown float length %02x" % length)
  else:
    raise ValueError("Unknown subtype %02x" % subtype)



PACKET_LENGTH_BYTES = 2 # number of bytes in the packet length field

class TelemetryDeserializationError(Exception):
  pass

class NoRecordIdError(TelemetryDeserializationError):
  pass
class MissingKvrError(TelemetryDeserializationError):
  pass

datatype_registry = {}
class TelemetryData(object):
  """Abstract base class for telemetry data ID definitions.
  """
  def __repr__(self):
    out = self.__class__.__name__
    for _, record_desc in self.get_kvrs_dict().items():
      record_name, _ = record_desc
      out += " %s=%s" % (record_name, repr(getattr(self, record_name)))
    return out
  
  def get_kvrs_dict(self):
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
      raise NoOpcodeError("No datatype %02x" % opcode)
    data_cls = datatype_registry[opcode]
    return data_cls(data_id, byte_stream)
  
  def __init__(self, data_id, byte_stream):
    self.data_id = data_id
    self.data_type = deserialize_uint8(byte_stream)
    
    self.internal_name = "%02x" % data_id
    self.display_name = self.internal_name
    self.units = ""
    
    self.latest_value = None
    
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
        raise NoRecordIdError("%s missing RecordId %02x (%s) in header" % (self.__class__.__name__, record_id, record_name))

  def deserialize_data(self, byte_stream):
    """Destructively reads in the data of this type from the input stream.
    """
    raise NotImplementedError
  
  def serialize_data(self, value):
    """Returns the serialized version (as bytes) of this data given a value.
    Can raise a ValueError if there is a conversion issue.
    """
    raise NotImplementedError
  
  def get_latest_value(self):
    return self.latest_value
  
  def set_latest_value(self, value):
    self.latest_value = value
  
  
  
class UnknownNumericSubtype(Exception):
  pass
  
class NumericData(TelemetryData):
  def get_kvrs_dict(self):
    newdict = super(NumericData, self).get_kvrs_dict().copy()
    newdict.update({ 
      0x40: ('subtype', deserialize_uint8),
      0x41: ('length', deserialize_uint8),
      0x42: ('limits', deserialize_numeric_from_def(self, count=2)),
    })
    return newdict
    
  def deserialize_data(self, byte_stream):
    return deserialize_numeric(byte_stream, self.subtype, self.length)
  
  def serialize_data(self, value):
    return serialize_numeric(value, self.subtype, self.length)
  
datatype_registry[DATATYPE_NUMERIC] = NumericData

class NumericArray(TelemetryData):
  def get_kvrs_dict(self):
    newdict = super(NumericArray, self).get_kvrs_dict().copy()
    newdict.update({ 
      0x40: ('subtype', deserialize_uint8),
      0x41: ('length', deserialize_uint8),
      0x42: ('limits', deserialize_numeric_from_def(self, count=2)),
      0x50: ('count', deserialize_uint32),
    })
    return newdict 
  
  def deserialize_data(self, byte_stream):
    out = []
    for _ in range(self.count):
      out.append(deserialize_numeric(byte_stream, self.subtype, self.length))
    return out
  
  def serialize_data(self, value):
    if len(value) != self.count:
      raise ValueError("Length mismatch: got %i, expected %i"
                       % (len(value), self.count))
    out = bytes()
    for elt in value:
      out += serialize_numeric(elt, self.subtype, self.length)
    return out
  
datatype_registry[DATATYPE_NUMERIC_ARRAY] = NumericArray

class PacketSizeError(TelemetryDeserializationError):
  pass
class NoOpcodeError(TelemetryDeserializationError):
  pass
class DuplicateDataIdError(TelemetryDeserializationError):
  pass
class UndefinedDataIdError(TelemetryDeserializationError):
  pass

opcodes_registry = {}
class TelemetryPacket(object):
  """Abstract base class for telemetry packets.
  """
  @staticmethod
  def decode(byte_stream, context):
    opcode = byte_stream[0]
    if opcode not in opcodes_registry:
      raise NoOpcodeError("No opcode %02x" % opcode)
    packet_cls = opcodes_registry[opcode]
    return packet_cls(byte_stream, context)
  
  def __init__(self, byte_stream, context):
    self.opcode = deserialize_uint8(byte_stream)
    self.sequence = deserialize_uint8(byte_stream)
    self.decode_payload(byte_stream, context)
    if len(byte_stream) > 0:
      raise PacketSizeError("%i unused bytes in packet" % len(byte_stream))
      
  def decode_payload(self, byte_stream, context):
    raise NotImplementedError

class HeaderPacket(TelemetryPacket):
  def __repr__(self):
    return "[%i]Header: %s" % (self.sequence, repr(self.data))
    
  def decode_payload(self, byte_stream, context):
    self.data = {}
    while True:
      data_id = deserialize_uint8(byte_stream)
      if data_id == DATAID_TERMINATOR:
        break
      elif data_id in self.data:
        raise DuplicateDataIdError("Duplicate DataId %02x" % data_id)
      self.data[data_id] = TelemetryData.decode_header(data_id, byte_stream)
      
  def get_data_defs(self):
    """Returns the data defs defined in this header as a dict of data ID to
    TelemetryData objects.
    """
    return self.data
  
  def get_data_names(self):
    data_names = []
    for data_def in self.data.values():
      data_names.append(data_def.internal_name)
    return data_names
      
opcodes_registry[OPCODE_HEADER] = HeaderPacket

class DataPacket(TelemetryPacket):
  def __repr__(self):
    return "[%i]Data: %s" % (self.sequence, repr(self.data))
    
  def decode_payload(self, byte_stream, context):
    self.data = {}
    while True:
      data_id = deserialize_uint8(byte_stream)
      if data_id == DATAID_TERMINATOR:
        break
      data_def = context.get_data_def(data_id)
      if not data_def:
        raise UndefinedDataIdError("Received DataId %02x not defined in header" % data_id)
      data_value = data_def.deserialize_data(byte_stream)
      data_def.set_latest_value(data_value)
      self.data[data_def.data_id] = data_value 

  def get_data_dict(self):
    return self.data

  def get_data_by_id(self, data_id):
    if data_id in self.data:
      return self.data[data_id]
    else:
      return None

opcodes_registry[OPCODE_DATA] = DataPacket  



class TelemetryContext(object):
  """Context for telemetry communications, containing the setup information in
  the header.
  """
  def __init__(self, data_defs):
    self.data_defs = data_defs
    
  def get_data_def(self, data_id):
    if data_id in self.data_defs:
      return self.data_defs[data_id]
    else:
      return None



class TelemetrySerial(object):
  """Telemetry serial receiver state machine. Separates out telemetry packets
  from the rest of the stream.
  """
  DecoderState = enum('SOF', 'LENGTH', 'DATA', 'DATA_DESTUFF', 'DATA_DESTUFF_END')
  PACKET_TIMEOUT_THRESHOLD = 0.1  # seconds
  
  def __init__(self, serial):
    self.serial = serial
    
    self.rx_packets = deque()  # queued decoded packets
    
    self.context = TelemetryContext([])
    
    # decoder state machine variables
    self.decoder_state = self.DecoderState.SOF;  # expected next byte
    self.decoder_pos = 0; # position within decoder_State
    self.packet_length = 0;  # expected packet length
    self.packet_buffer = deque()
    
    self.data_buffer = deque()
    
    # decoder packet timeout variables
    self.last_loop_received = False
    self.last_receive_time = time.time()

  def process_rx(self):
    if ((not self.last_loop_received)
        and (time.time() - self.last_receive_time > self.PACKET_TIMEOUT_THRESHOLD)
        and (self.decoder_state != self.DecoderState.SOF and self.decoder_pos > 0)):
      self.decoder_state = self.DecoderState.SOF
      self.decoder_pos = 0
      self.packet_buffer = deque()
      print("Packet timed out; dropping")
      
    self.last_loop_received = False
    
    while self.serial.inWaiting():
      self.last_loop_received = True
      self.last_receive_time = time.time()

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
          try:
            decoded = TelemetryPacket.decode(self.packet_buffer, self.context)
          
            if isinstance(decoded, HeaderPacket):
              self.context = TelemetryContext(decoded.get_data_defs())
            
            self.rx_packets.append(decoded)
          except TelemetryDeserializationError as e:
            print("Deserialization error: %s" % repr(e)) # TODO prettier cleaner
          
          self.packet_buffer = deque()
      
          self.decoder_pos = 0
          if rx_byte == SOF_BYTE[0]:
            self.decoder_state = self.DecoderState.DATA_DESTUFF_END
          else:
            self.decoder_state = self.DecoderState.SOF
        elif rx_byte == SOF_BYTE[0]:
          self.decoder_state = self.DecoderState.DATA_DESTUFF
      elif self.decoder_state == self.DecoderState.DATA_DESTUFF:
        self.decoder_state = self.DecoderState.DATA
      elif self.decoder_state == self.DecoderState.DATA_DESTUFF_END:
        self.decoder_state = self.DecoderState.SOF
      else:
        raise RuntimeError("Unknown DecoderState")

  def transmit_set_packet(self, data_def, value):
    packet = bytearray()
    packet += serialize_uint8(OPCODE_DATA)
    packet += serialize_uint8(data_def.data_id)
    packet += data_def.serialize_data(value)
    packet += serialize_uint8(DATAID_TERMINATOR)
    self.transmit_packet(packet)
  
  def transmit_packet(self, packet):
    header = bytearray()
    for elt in SOF_BYTE:
      header += serialize_uint8(elt)
    header += serialize_uint16(len(packet))
    
    modified_packet = bytearray()
    for packet_byte in packet:
      modified_packet.append(packet_byte)
      if packet_byte == SOF_BYTE[0]:
        modified_packet.append(0x00)
    
    self.serial.write(header + modified_packet)

    # TODO: add CRC support
      
  def next_rx_packet(self):
    if self.rx_packets:
      return self.rx_packets.popleft()
    else:
      return None
    
  def next_rx_byte(self):
    if self.data_buffer:
      return self.data_buffer.popleft()
    else:
      return None
