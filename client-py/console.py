from __future__ import print_function
import time

import serial

from telemetry.parser import TelemetrySerial

if __name__ == "__main__":
  import argparse
  parser = argparse.ArgumentParser(description='Telemetry packet parser example.')
  parser.add_argument('port', metavar='p', help='serial port to receive on')
  parser.add_argument('--baud', metavar='b', type=int, default=38400,
                      help='serial baud rate')
  args = parser.parse_args()
  
  telemetry = TelemetrySerial(serial.Serial(args.port, baudrate=args.baud))
  
  while True:
    telemetry.process_rx()
    time.sleep(0.1)

    while True:
      next_packet = telemetry.next_rx_packet()
      if not next_packet:
        break
      print('')
      print(next_packet)
    
    while True:
      next_byte = telemetry.next_rx_byte()
      if next_byte is None:
        break
      try:
        print(chr(next_byte), end='')
      except UnicodeEncodeError:
        pass
          