# Safe EEPROM Programming Demo

## Overview

The `-S` (Safe Program) option automatically manages Linux kernel drivers when programming PMBus device EEPROMs, preventing conflicts and ensuring safe operation.

## How It Works

The safe programming feature performs these steps automatically:

```
1. Detects devices on the I2C bus with bound kernel drivers
2. Unbinds drivers (stops telemetry temporarily)
3. Programs EEPROM safely
4. Verifies programming
5. Waits for device to stabilize
6. Rebinds drivers (resumes telemetry)
```

## Usage

### Basic Safe Programming

```bash
# Safe program with automatic driver management
sudo ./LT_PMBusApp -d /dev/i2c-0 -S config.hex
```

**Note:** Requires `sudo` because unbinding/rebinding kernel drivers needs root access.

### Standard Programming (Without Driver Management)

```bash
# Regular programming (may conflict with running drivers)
./LT_PMBusApp -d /dev/i2c-0 -p config.hex
```

## Example Output

```bash
$ sudo ./LT_PMBusApp -d /dev/i2c-0 -S ltc2977_config.hex

Safe Program with file ltc2977_config.hex (auto-unbind/rebind drivers)
Operate with device /dev/i2c-0

=== Step 1: Unbinding kernel drivers ===
Found 1 device(s) with drivers bound on bus 0
Unbinding device 0-005b from driver pmbus...
  ✓ Successfully unbound
Successfully unbound 1 device(s)
Telemetry is now stopped. Programming can proceed safely.

=== Step 2: Programming EEPROM ===
Recording type: 02 Data Length: 000e
NVM Address: 0x5b
Programming NVM...
[Progress: ████████████████████████████████] 100%
NVM Programmed Successfully
Verifying NVM...
[Progress: ████████████████████████████████] 100%
NVM Verified Successfully

Waiting 2 seconds for device to stabilize...

=== Step 3: Rebinding kernel drivers ===
Rebinding 1 device(s)...
Rebinding device 0-005b to driver pmbus...
  ✓ Successfully rebound
All drivers rebound successfully. Telemetry should resume.

=== Safe Programming Complete ===
Device has been programmed and drivers rebound.
Telemetry should resume automatically.
```

## When To Use Safe Programming

### ✅ **Use `-S` (Safe Mode) When:**
- Device is actively monitored by Linux (hwmon drivers loaded)
- System is in production/live operation
- You want automated driver management
- Telemetry interruption is acceptable (5-10 seconds)

### ⚠️ **Use `-p` (Standard Mode) When:**
- No kernel drivers are bound to the device
- Programming during initial setup
- You've manually unbound drivers already
- Device is not in active use

## Technical Details

### What Gets Unbound

The tool automatically finds and unbinds:
- **pmbus** driver (PMBus devices)
- **lm75** driver (temperature sensors)
- **max34440** driver (Maxim power devices)
- Any other I2C drivers on the specified bus

### Timing Considerations

| Phase | Duration | Notes |
|-------|----------|-------|
| Driver unbind | 100-200ms | Per device |
| EEPROM program | 2-5s | Depends on file size |
| Device reset | 2s | Stabilization time |
| Driver rebind | 500ms | Per device |
| **Total** | **5-10s** | Telemetry gap |

### What Happens to the System

During programming:
1. **Telemetry stops** - `/sys/class/hwmon/hwmon*` readings unavailable
2. **Device outputs** - May remain stable or briefly turn off (device-dependent)
3. **System logs** - Kernel logs driver unbind/rebind events
4. **Monitoring tools** - May show device offline

After programming:
1. **Telemetry resumes** - New configuration values reported
2. **Device reinitializes** - Uses new EEPROM settings
3. **Normal operation** - System returns to normal state

## Error Handling

### If Unbind Fails

```
Cannot open /sys/bus/i2c/drivers/pmbus/unbind (may need sudo)
```
**Solution:** Run with `sudo`

### If Device Won't Rebind

```
Device 0-005b failed to rebind to driver
Manual intervention may be required.
```

**Solution:** Manually rebind:
```bash
echo "0-005b" | sudo tee /sys/bus/i2c/drivers/pmbus/bind
```

### If Programming Fails Mid-Operation

The destructor automatically attempts to rebind drivers even if programming fails. If you interrupt with Ctrl+C, drivers should still rebind.

## Advanced Usage

### Check Driver Status Before Programming

```bash
# See which devices have drivers bound
ls -l /sys/bus/i2c/devices/*/driver

# Example output:
lrwxrwxrwx ... 0-005b/driver -> ../../../bus/i2c/drivers/pmbus
```

### Manual Driver Management (Alternative)

If you prefer manual control:

```bash
# 1. Unbind manually
echo "0-005b" | sudo tee /sys/bus/i2c/drivers/pmbus/unbind

# 2. Program normally
./LT_PMBusApp -d /dev/i2c-0 -p config.hex

# 3. Rebind manually
echo "0-005b" | sudo tee /sys/bus/i2c/drivers/pmbus/bind
```

## Limitations

1. **Requires sudo** - Kernel driver management needs root access
2. **Telemetry interruption** - 5-10 second gap in monitoring
3. **I2C devices only** - Serial bridges (`/dev/ttyUSB*`) don't need driver unbinding
4. **Device-specific behavior** - Some devices may require power cycle

## Safety Features

✅ **Automatic rebind** - Even on error or interruption (destructor-based)
✅ **Device detection** - Only unbinds drivers on the specified bus
✅ **Verification wait** - 2-second stabilization time before rebind
✅ **Status reporting** - Clear feedback on each operation
✅ **Error recovery** - Attempts to restore system state on failure

## Troubleshooting

### "No devices with bound drivers found"

This is normal if:
- No kernel drivers are using the bus
- Device is not detected by Linux
- Already unbound manually

**Action:** Can proceed with standard `-p` option

### Device Not Working After Programming

1. Check device is powered
2. Check I2C connections
3. Try power cycling the device
4. Check dmesg for errors: `sudo dmesg | tail`
5. Force driver reload: `sudo modprobe -r pmbus && sudo modprobe pmbus`

### Kernel Logs Show Errors

```bash
# Check for I2C or driver errors
sudo dmesg | grep -i "i2c\|pmbus"
```

Common errors:
- I2C transaction timeout → Check physical connections
- Device not responding → May need power cycle
- Driver probe failed → Device may be in bad state

## Testing

See [SAFE_PROGRAMMING_TEST.sh](./SAFE_PROGRAMMING_TEST.sh) for a complete test script.

## Related Documentation

- [USB_I2C_BRIDGE.md](./USB_I2C_BRIDGE.md) - USB-to-I2C adapter support
- [QUICKSTART.md](./QUICKSTART.md) - General usage guide
- [README.md](./README.md) - Main project documentation
