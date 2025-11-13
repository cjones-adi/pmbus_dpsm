# USB-to-I2C Bridge Integration

## Overview

The pmbus_dpsm project now supports USB-to-I2C bridge devices (such as DC1613A, FTDI adapters, and similar devices) in addition to native Linux I2C devices.

## Supported Interfaces

### 1. Native I2C (default)
- Uses `/dev/i2c-*` devices
- Direct hardware I2C on Raspberry Pi, BeagleBone, etc.
- Requires `i2c-dev` kernel module

### 2. USB-to-I2C Serial Bridge (new)
- Uses `/dev/ttyUSB*` or `/dev/ttyACM*` devices
- Supports DC1613A, FTDI, and compatible adapters
- Automatic detection based on device path

## Usage

### With Native I2C
```bash
# Default (uses /dev/i2c-0)
./LT_PMBusApp -i

# Specific I2C device
./LT_PMBusApp -d /dev/i2c-1 -i
```

### With USB-to-I2C Bridge
```bash
# USB serial bridge
./LT_PMBusApp -d /dev/ttyUSB0 -i

# USB ACM device
./LT_PMBusApp -d /dev/ttyACM0 -i
```

## Device Auto-Detection

The application automatically detects the device type:
- **Serial/USB devices**: Paths containing `/dev/tty*` or character devices with major numbers 4, 188, or 166
- **I2C devices**: All other devices (typically `/dev/i2c-*`)

## USB-to-I2C Bridge Protocol

The serial bridge implementation uses a simple packet-based protocol:

### Command Packet Format
```
[CMD][LEN_LOW][LEN_HIGH][DATA...]
```

### Response Packet Format
```
[STATUS][LEN_LOW][LEN_HIGH][DATA...]
```

### Supported Commands
- `0x01` - Write Byte
- `0x02` - Read Byte
- `0x03` - Write Word
- `0x04` - Read Word
- `0x05` - Write Block
- `0x06` - Read Block
- `0x07` - Send Byte
- `0x08` - Probe Bus
- `0x09` - Set PEC Mode
- `0x0A` - Set Speed

### Status Codes
- `0x00` - OK
- `0x01` - NACK
- `0x02` - Error

## Serial Port Configuration
- Baud rate: 115200
- Data bits: 8
- Parity: None
- Stop bits: 1
- Flow control: None

## Hardware Requirements

### DC1613A (Recommended)
- Linduino-based USB-to-I2C adapter
- QuikEval-compatible connector
- Full PMBus protocol support

### Alternative USB-to-I2C Adapters
Any USB-to-I2C bridge that:
1. Appears as a serial device (`/dev/ttyUSB*` or `/dev/ttyACM*`)
2. Implements the command protocol described above
3. Supports SMBus operations

## Building

### With Automake
```bash
./configure
make
```

### With CMake
```bash
mkdir build
cd build
cmake ..
make
```

## Firmware for USB-to-I2C Adapters

To use this with a DC1613A or similar Linduino-based device, you'll need compatible firmware that implements the serial protocol. A reference implementation can be created based on the existing Linduino PMBus libraries.

## Troubleshooting

### Device Not Found
```bash
# Check if device exists
ls -l /dev/ttyUSB*

# Check permissions
sudo chmod 666 /dev/ttyUSB0

# Add user to dialout group (permanent solution)
sudo usermod -a -G dialout $USER
# Logout and login again
```

### Communication Errors
```bash
# Check if another program is using the device
lsof | grep ttyUSB

# Try different baud rates (modify code if needed)
# Default is 115200

# Check USB connection
dmesg | tail -20
```

### No Devices Detected
- Verify USB bridge firmware is running
- Check physical connections to PMBus devices
- Try probe command to scan bus

## Example Session

```bash
$ ./LT_PMBusApp -d /dev/ttyUSB0 -i
Detected USB-to-I2C bridge device: /dev/ttyUSB0
Opened USB-to-I2C bridge at /dev/ttyUSB0
********************************************************
* LT_PMBus                                             *
* Menu Version 1.00                                    *
********************************************************

1-Load Main Menu
2-Load Other Menu
8-Clear All Faults
9-Dump/Clear Fault Logs
0-Others

Enter a command:
```

## API Usage

### Creating Serial Bridge Instances
```cpp
#include "LT_SMBusSerialNoPec.h"
#include "LT_SMBusSerialPec.h"

// Without PEC
LT_SMBusSerialNoPec *smbus = new LT_SMBusSerialNoPec("/dev/ttyUSB0");

// With PEC
LT_SMBusSerialPec *smbus = new LT_SMBusSerialPec("/dev/ttyUSB0");

// Use with PMBus
LT_PMBus *pmbus = new LT_PMBus(smbus);
```

## Future Enhancements

- [ ] Add support for different baud rates via command line
- [ ] Implement firmware for Linduino/DC1613A
- [ ] Add USB vendor/product ID detection
- [ ] Support for I2C repeated start
- [ ] Implement Process Call and Block Process Call
- [ ] Add debugging/logging modes

## Related Documentation

- [LT_PMBus README](../README.md)
- [Linduino Documentation](https://www.analog.com/en/design-center/evaluation-hardware-and-software/evaluation-boards-kits/dc2026c.html)
- [SMBus Specification](http://smbus.org/specs/)
- [PMBus Specification](https://pmbus.org/)
