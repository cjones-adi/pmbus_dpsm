# Quick Start Guide - USB-to-I2C Bridge Integration

## For Users

### 1. Check Your Device
```bash
# List USB serial devices
ls -l /dev/ttyUSB* /dev/ttyACM*

# Check if you have permission
id -nG | grep dialout
```

### 2. Fix Permissions (if needed)
```bash
# Add yourself to dialout group
sudo usermod -a -G dialout $USER

# Logout and login, or use:
newgrp dialout
```

### 3. Run the Application
```bash
cd /home/cj/pmbus_dpsm

# With USB bridge
./src/LT_PMBusApp -d /dev/ttyUSB0 -i

# With native I2C (default)
./src/LT_PMBusApp -i
```

### 4. Expected Output
```
Detected USB-to-I2C bridge device: /dev/ttyUSB0
Opened USB-to-I2C bridge at /dev/ttyUSB0
********************************************************
* LT_PMBus                                             *
* Menu Version 1.00                                    *
********************************************************
```

## For Developers

### Adding Serial Bridge Support to Your Code

```cpp
#include "LT_SMBusSerialNoPec.h"
#include "LT_PMBus.h"

// Create serial bridge instance
LT_SMBusSerialNoPec *smbus = new LT_SMBusSerialNoPec("/dev/ttyUSB0");

// Use with PMBus
LT_PMBus *pmbus = new LT_PMBus(smbus);

// Detect devices
LT_PMBusDetect *detector = new LT_PMBusDetect(pmbus);
detector->detect();

// Get detected devices
LT_PMBusDevice **devices = detector->getDevices();
```

### Implementing Bridge Firmware

If you're creating firmware for a DC1613A or similar device:

```cpp
// Arduino/Linduino sketch structure

#include <LT_PMBus.h>
#include <LT_SMBus.h>

// Command definitions
#define CMD_WRITE_BYTE  0x01
#define CMD_READ_BYTE   0x02
// ... etc

void setup() {
  Serial.begin(115200);
  Wire.begin();
}

void loop() {
  if (Serial.available() >= 3) {
    uint8_t cmd = Serial.read();
    uint16_t len = Serial.read() | (Serial.read() << 8);
    
    // Read data if present
    uint8_t data[256];
    Serial.readBytes(data, len);
    
    // Process command
    switch(cmd) {
      case CMD_WRITE_BYTE:
        handleWriteByte(data);
        break;
      case CMD_READ_BYTE:
        handleReadByte(data);
        break;
      // ... handle other commands
    }
  }
}
```

### Protocol Details

#### Write Byte Command
```
TX: [0x01][0x03][0x00][ADDR][CMD][DATA]
RX: [0x00][0x00][0x00]
```

#### Read Byte Command
```
TX: [0x02][0x02][0x00][ADDR][CMD]
RX: [0x00][0x01][0x00][DATA]
```

## Testing

### Test Script
```bash
#!/bin/bash

DEVICE="/dev/ttyUSB0"
APP="./src/LT_PMBusApp"

echo "Testing USB-to-I2C Bridge Integration"

# Test 1: Device detection
echo "Test 1: Device Detection"
$APP -d $DEVICE -i <<EOF
0
EOF

# Test 2: Probe bus (if available in menu)
echo "Test 2: Probing PMBus"
# Add more tests as needed
```

### Debug Mode
To enable verbose output, modify LT_SMBusSerial.cpp:

```cpp
#define DEBUG_SERIAL_BRIDGE 1

#if DEBUG_SERIAL_BRIDGE
  printf("Sending command: 0x%02x, len: %d\n", cmd, data_len);
#endif
```

## Troubleshooting

### Error: "Failed to open serial device"
- Check device exists: `ls -l /dev/ttyUSB0`
- Check permissions: `ls -l /dev/ttyUSB0`
- Add to group: `sudo usermod -a -G dialout $USER`

### Error: "Device not responding"
- Check USB connection
- Verify firmware is running on bridge
- Try different baud rate
- Check physical I2C connections

### No Devices Detected
- Verify PMBus devices are powered
- Check I2C pull-up resistors
- Verify addresses with native I2C tools
- Try bus probe command

## Common Commands

```bash
# Build project
make clean && make

# Install to system
sudo make install

# Run with debug
gdb --args ./src/LT_PMBusApp -d /dev/ttyUSB0 -i

# Monitor serial traffic
sudo cat /dev/ttyUSB0 | hexdump -C

# Check USB device info
lsusb
dmesg | tail -20
```

## Example Session

```
$ ./src/LT_PMBusApp -d /dev/ttyUSB0 -i
Operate with device /dev/ttyUSB0
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

Enter a command: 1

Found: LTC3880 at 0x30
Found: LTC2974 at 0x32
Found: LTC2977 at 0x33

1-Select Device
2-Read Device
3-Control Device
...
```

## Performance Notes

- Native I2C: ~400 kHz typical
- USB-Serial Bridge: ~100 kHz typical (limited by serial latency)
- Round-trip time: ~10ms per transaction
- Suitable for configuration and monitoring
- Not recommended for high-speed data acquisition

## References

- [USB_I2C_BRIDGE.md](./USB_I2C_BRIDGE.md) - Full documentation
- [INTEGRATION_SUMMARY.md](./INTEGRATION_SUMMARY.md) - Technical details
- [README.md](./README.md) - Main project README
- SMBus Specification: http://smbus.org/specs/
- PMBus Specification: https://pmbus.org/
