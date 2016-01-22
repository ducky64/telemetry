# telemetry
Telemetry protocol with spec, transmit-side library, and visualization client.

## Introduction
The telemetry library is designed to provide a simple and efficient way to get data off an embedded platform and both visualized and logged on a PC. The intended use case is streaming data from an embedded system to a PC through an UART interface with either a Bluetooth or UART bridge. Since data definitions are transmitted, users only need to alter code on the transmitting side, eliminating the need to keep both transmitter and visualizer definitions in sync.

The server-side (transmitter) code is written in C++ and designed with embedded constraints in mind (no dynamic memory allocation is done). Templated classes allow most native data types and a hardware abstraction layer allows multiple platforms (currently Arduino and mBed).

The client-side (plotter / visualizer) code is written in Python. A basic telemetry protocol parser is provided that separates and interprets telemetry packets and other data from the received stream. A matplotlib-based GUI is also provided on top of the parser that visualizes numeric data as a line plot and array-numeric data as a waterfall / spectrogram style plot.

The [protocol spec](../master/docs/protocol.tex) defines a binary wire format along with packet structures for data and headers, allowing for other implementations of either side that interoperate.

## Quickstart
### Plotter setup
Telemetry depends on these:
- Python (ideally 3.4 or later, may also work on 2.7)
- numpy 1.9.2 or later
- matplotlib 1.4.3 or later
- PySerial

#### On Windows
For Windows platforms, [Python](https://www.python.org/downloads/), [numpy](http://sourceforge.net/projects/numpy/files/NumPy/), [matplotlib](http://matplotlib.org/downloads.html) and can be downloaded.

Other software can be installed through pip, Python's package manager. pip is located in your Python/Scripts folder, and the installation commands for the necessary packages are:

`pip install pyserial`

`pip install six python-dateutil pyparsing`

#### On Linux
For Linux platforms, the required software should be available from the OS package manager.

On Debian-based platforms, including Ubuntu, the installation commands for the necessary packages are:

`TODO: commands`

### Transmitter library setup
Transmitter library sources are in `telemetry/server-cpp`. Add the folder to your include search directory and add all the `.cpp` files to your build. Your platform should be automatically detected based on common `#define`s, like `ARDUINO` for Arduino targets and `TOOLCHAIN_GCC_ARM` for mbed targets.

For those using a [SCons](http://scons.org/)-based build system, a SConscript file is also included which defines a static library.

### Transmitter library usage
Include the telemetry header in your code:
```c++
#include "telemetry.h"
```

and either the header for Arduino or mbed:
```c++
// Choose ONE.
#include "telemetry-arduino.h"
#include "telemetry-mbed.h"
```
*In future versions, telemetry.h will autodetect the platform and include the correct header.*

Then, instantiate a telemetry HAL (hardware abstraction layer) for your platform. This adapts the platform-specific UART into a standard interface allowing telemetry to be used across multiple platforms.

For Arduino, instantiate it with a Serial object:
```c++
telemetry::ArduinoHalInterface telemetry_hal(Serial1);
telemetry::Telemetry telemetry_obj(telemetry_hal);
```
For mbed, instantiate it with a MODSERIAL object (which provides transmit and receive buffering):
```c++
MODSERIAL telemetry_serial(PTA2, PTA1);  // PTA2 as TX, PTA1 as RX
telemetry::MbedHal telemetry_hal(telemetry_serial);
telemetry::Telemetry telemetry_obj(telemetry_hal);
```
*In future versions, telemetry will include a Serial buffering layer.*

Next, instantiate telemetry data objects. These objects act like their templated data types (for example, you can assign and read from a `telemetry::Numeric<uint32_t>` as if it were a regular `uint32_t`), but can both send updates and be remotely set using the telemetry link.

This example shows three data objects needed for a waterfall visualization of a linescan camera and a line plot for motor command:
```c++
telemetry::Numeric<uint32_t> tele_time_ms(telemetry_obj, "time", "Time", "ms", 0);
telemetry::NumericArray<uint16_t, 128> tele_linescan(telemetry_obj, "linescan", "Linescan", "ADC", 0);
telemetry::Numeric<float> tele_motor_pwm(telemetry_obj, "motor", "Motor PWM", "%DC", 0);
```

In general, the constructor signatures for Telemetry data objects are:
- `template <typename T> Numeric(Telemetry& telemetry_container, const char* internal_name, const char* display_name, const char* units, T init_value)`
  - `Numeric` describes numeric data is type `T`. Only 8-, 16-, and 32-bit unsigned integers and single-precision floating point numbers are currently supported.
  - `telemetry_container`: a reference to a `Telemetry` object to associate this data with.
  - `internal_name`: a string giving this object an internal name to be referenced in code.
  - `display_name`: a string giving this object a human-friendly name.
  - `units`: a string describing the units this data is in (not currently used for purposes other than display, but that may change).
- `template <typename T, uint32 t array_count> telemetry container, const char* internal_name, const char* display_name, const char* units, T elem_init_value)`
  - `NumericArray` describes an array of numeric objects of type `T`. Same constraints on `T` as with `Numeric` apply. `array_count` is a template parameter (like a constant) to avoid dynamic memory allocation.
  - `telemetry_container`: a reference to a `Telemetry` object to associate this data with.
  - `internal_name`: a string giving this object an internal name to be referenced in code.
  - `display_name`: a string giving this object a human-friendly name.
  - `units`: a string describing the units this data is in (not currently used for purposes other than display, but that may change).
  - `elem_init_value`: initial value of array elements.

After instantiating a data object, you can also optionally specify additional parameters:
- `Numeric` can have the limits set. The plotter GUI will set the plot bounds / waterfall intensity bounds if this is set, otherwise it will autoscale. This does NOT affect the embedded code, values will not be clipped.
  - `tele_motor_pwm.set_limits(0.0, 1.0); // lower bound, upper bound`

Once the data objects have been set up, transmit the data definitions:
```c++
telemetry_obj.transmit_header();
```

The telemetry system is set up and ready to use now. Load data to be transmitted into the telemetry object by either using the assign operator or the array indexing operator. For example, to update the linescan data:
```c++
uint16_t* data = camera.read() ;
for ( uint16_t i = 0; i < 128; i++) {
  tele_linescan[i] = data[i]
}
```

Actual IO operations (and hence plotter GUI updates) only happen when `Telemetry`'s `do_io()` method is called and should be done regularly. Only the latest update per `do_io()` is transmitted, intermediate values are clobbered. Remote set operations take effect during a `do_io()` and can also be clobbered by any set operations afterwards.
```c++
telemetry_obj.do_io();
```

You can continue using the UART to transmit other data (like with `printf`s) as long as this doesn't happen during a `Telemetry` `do_io()` operation (which will corrupt the sent data) or contain a start-of-frame sequence (`0x05, 0x39`).

### Plotter GUI Usage
The plotter is located in `telemetry/client-py/plotter.py` and can be directly executed using Python. The arguments can be obtained by running it with `--help`:
- Serial port: like COM1 for Windows or /dev/ttyUSB0 or /dev/ttyACM0 for Linux.
- Baud rate: optional, defaults to 38,400.
- Independent variable name: defaults to `time`.
- Independent variable span: defaults to 10,000 (or 10 seconds, if your units are in milliseconds).

The plotter must be running when the header is transmitted, otherwise it will fail to decode the data packets (and notify you of such). The plotter will automatically reinitialize upon receiving a new header.

This simple plotter graphs all the data against a selected independent variable (like time). Numeric data is plotted as a line graph and array-numeric data is plotted as a waterfall / spectrograph-style graph. Regular UART data (like from `printf`s) will be routed to the console. All received data, including from `printf`s, is logged to a CSV. A new CSV is created each time a new header packet is received, with a timestamped filename.

You can double-click a plot to inspect its latest value numerically and optionally remotely set it to a new value.

If you feel really adventurous, you can also try to mess with the code to plot things in different styles. For example, the plot instantiation function from a received header packet is in `subplots_from_header`. The default just creates a line plot for numeric data and a waterfall plot for array-numeric data. You can make it do fancier things, like overlay a numerical detected track position on the raw camera waterfall plot.

### Protips
Bandwidth limits: the amount of data you can send is limited by your microcontroller's UART rate, the UART-PC interface (like Bluetooth-UART or a USB-UART adapter), and transmission overhead (for example, at high baud rates, the overhead from mbed's putc takes longer than the physical transmission of the character). If you're constantly getting receive errors, try:
- Reducing precision. A 8-bit integer is smaller than a 32-bit integer. If all you're doing is plotting, the difference may be visually imperceptible.
- Decimating samples. Sending every nth sample will reduce the data rate by n. Again, the difference may be imperceptible if plotting, but you may also lose important high frequency components. Use with caution.

## Future Work
These are planned features for the very near future:

### Transmitter Library
- Code cleanup.
- Asynchronous `gets` for mbed RPC compatibility.

### Plotter GUI
- Multiple visualizations styles (including single-frame mode for arrays).

## Known Issues
### Transmitter Library
- No header resend request, so the plotter GUI must be started before the transmitter starts.
- No DMA support.
- No CRC or FEC support.

### Plotter GUI
- Waterfall / spectrogram style plotting is CPU intensive and inefficient.

### mbed HAL
- Serial putc is highly inefficient for bulk data.

## Copyright
*Contents are licensed under the BSD 3-clause ("BSD simplified", "BSD new") license, the terms of which are reproduced below.*

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
