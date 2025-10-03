# PMBus Consolidated Framework Examples

This directory contains example applications demonstrating the use of the PMBus consolidated framework.

## Examples

### 1. basic_discovery.c
**Purpose**: Demonstrates basic device discovery functionality.

**Features**:
- Scan I2C bus for PMBus devices
- Display device information (manufacturer, model, revision)
- Show device capabilities (PEC support, pages, etc.)
- Display discovery statistics

**Usage**:
```bash
./basic_discovery [-b bus_number] [-h]
```

**Example Output**:
```
PMBus Device Discovery Example
==============================

Initialized PMBus transport on bus 1

Scanning for PMBus devices...

Found 2 PMBus device(s):

Device 1:
  Address: 0x5C
  Type: Power Supply Controller
  Manufacturer ID: Linear Technology
  Model: LTC2978
  Revision: 1.0
  Capabilities:
    PEC Support: Yes
    Extended Commands: Yes
    Group Commands: No
    Pages: 8

Device 2:
  Address: 0x60
  Type: Power Supply Controller
  Manufacturer ID: Analog Devices
  Model: LTC2974
  Revision: 2.1
  Capabilities:
    PEC Support: Yes
    Extended Commands: Yes
    Group Commands: Yes
    Pages: 4

Discovery Statistics:
  Addresses scanned: 112
  Devices found: 2
  Scan duration: 1250 ms
  Average response time: 11.16 ms
```

### 2. telemetry_monitor.c
**Purpose**: Continuous monitoring of PMBus device telemetry data.

**Features**:
- Real-time telemetry monitoring
- Voltage, current, power, and temperature measurements
- Efficiency calculations
- Status flag monitoring
- Configurable update intervals
- Transport statistics

**Usage**:
```bash
./telemetry_monitor [-b bus_number] [-a address] [-i interval] [-h]
```

**Options**:
- `-b bus_number`: I2C bus number (default: 1)
- `-a address`: Specific device address in hex (default: monitor all)
- `-i interval`: Update interval in seconds (default: 1)

**Example Output**:
```
PMBus Telemetry Monitor
=======================
Monitoring all discovered devices
Update interval: 1 second(s)

[14:32:15] Device 0x5C Telemetry:
  Input Voltage:    12.045 V
  Output Voltage:    3.301 V
  Input Current:     0.825 A
  Output Current:    2.950 A
  Input Power:       9.937 W
  Output Power:      9.738 W
  Internal Temp:    45.2 °C
  Efficiency:       98.00 %

[14:32:15] Device 0x60 Telemetry:
  Input Voltage:    12.043 V
  Output Voltage:    1.801 V
  Input Current:     1.250 A
  Output Current:    8.125 A
  Input Power:      15.054 W
  Output Power:     14.633 W
  Internal Temp:    52.8 °C
  Efficiency:       97.20 %
```

### 3. command_shell.c
**Purpose**: Interactive command-line interface for PMBus operations.

**Features**:
- Interactive shell with command history
- Device discovery and selection
- Raw PMBus command execution (read/write)
- Built-in telemetry commands
- Data format conversions
- Transport statistics
- Fault management

**Usage**:
```bash
./command_shell [-b bus_number] [-h]
```

**Available Commands**:

**General Commands**:
- `help` - Show help message
- `list` - List discovered devices
- `select <address>` - Select device by address (hex)
- `current` - Show currently selected device
- `scan` - Re-scan for devices
- `stats` - Show transport statistics
- `quit`/`exit` - Exit the shell

**PMBus Commands**:
- `read <command>` - Read PMBus command (hex)
- `write <command> <data>` - Write PMBus command with data (hex)
- `page <page>` - Set page number
- `clear_faults` - Clear all faults
- `operation <mode>` - Set operation mode

**Telemetry Commands**:
- `voltage` - Read voltage measurements
- `current` - Read current measurements
- `power` - Read power measurements
- `temperature` - Read temperature measurements
- `status` - Read status registers
- `telemetry` - Read all telemetry data

**Example Session**:
```
PMBus Interactive Command Shell
================================
Bus: 1
Type 'help' for available commands, 'scan' to discover devices.

pmbus> scan
Scanning for devices...
Found 2 device(s)

pmbus> list
Discovered PMBus Devices:
=========================
  0x5C - Power Supply Controller (Linear Technology LTC2978)
  0x60 - Power Supply Controller (Analog Devices LTC2974)

* = currently selected device

pmbus> select 5C
Selected device 0x5C

pmbus> telemetry
Device 0x5C Telemetry Data:
=============================
Input Voltage:     12.045 V
Output Voltage:     3.301 V
Input Current:      0.825 A
Output Current:     2.950 A
Input Power:        9.937 W
Output Power:       9.738 W
Internal Temp:     45.2 °C

pmbus> read 8B
Command 0x8B returned 2 bytes: 45 0C 
  As uint16: 3141 (0x0C45)
  As LINEAR11: 12.045

pmbus> stats
Transport Statistics:
====================
Total transactions:     24
Successful transactions: 24
Failed transactions:    0
Success rate:          100.00%
PEC errors:            0
Timeout errors:        0
Retry count:           0
Avg transaction time:  8.75 ms

pmbus> quit
Goodbye!
```

## Building the Examples

The examples are built automatically when you build the main framework:

```bash
cd consolidated_framework
mkdir build && cd build
cmake ..
make
```

The example binaries will be created in the `build/examples/` directory.

### Manual Build

To build individual examples manually:

```bash
gcc -o basic_discovery basic_discovery.c -I../include -L../build/src -lpmbus
gcc -o telemetry_monitor telemetry_monitor.c -I../include -L../build/src -lpmbus
gcc -o command_shell command_shell.c -I../include -L../build/src -lpmbus
```

## Running the Examples

### Prerequisites
1. PMBus devices connected to I2C bus
2. Appropriate I2C bus permissions (run as root or add user to i2c group)
3. I2C development tools installed

### Permission Setup
```bash
# Add user to i2c group
sudo usermod -a -G i2c $USER

# Or run with sudo
sudo ./basic_discovery -b 1
```

### Common Issues

**Permission Denied**: 
- Run with `sudo` or add user to `i2c` group
- Check if I2C devices exist: `ls /dev/i2c-*`

**No Devices Found**:
- Verify I2C bus number with `i2cdetect -l`
- Check device connections and power
- Try different bus numbers

**Communication Errors**:
- Check I2C pull-up resistors
- Verify device addresses don't conflict
- Reduce bus speed if necessary

## Integration Tips

1. **Error Handling**: All examples demonstrate proper error handling patterns
2. **Resource Cleanup**: Examples show correct cleanup of framework resources
3. **Configuration**: Examples use reasonable default configurations
4. **Logging**: Enable framework logging for debugging: `cmake -DPMBUS_ENABLE_LOGGING=ON`
5. **Statistics**: Enable statistics collection: `cmake -DPMBUS_ENABLE_STATISTICS=ON`

## Customization

The examples can be easily modified to:
- Add custom device types
- Implement specific command sequences
- Add data logging capabilities
- Integrate with monitoring systems
- Provide web or GUI interfaces
