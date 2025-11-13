# On-Site EEPROM Programming Guide for Customers

## Overview

This guide provides options for customers who need to update PMBus device EEPROM configuration **on-site**, potentially while the system is running.

---

## Quick Reference: Customer Solutions

### Solution 1: Native Linux I2C (Recommended - Safest)

**Hardware Required:**
- Linux system with I2C bus (Raspberry Pi, embedded Linux, industrial PC)
- Direct I2C connection to PMBus device

**Advantages:**
- ✅ **Most reliable** - Direct I2C communication
- ✅ **Safe programming** - Automatic driver unbind/rebind
- ✅ **No additional hardware** - Uses built-in I2C
- ✅ **Production ready** - Tested and validated

**Usage:**
```bash
# Safe programming (auto-manages kernel drivers)
sudo ./LT_PMBusApp -d /dev/i2c-1 -S new_config.hex

# System automatically:
# 1. Unbinds active drivers (stops monitoring)
# 2. Programs EEPROM
# 3. Waits for device stabilization
# 4. Rebinds drivers (restores monitoring)
```

**Best for:**
- Embedded Linux systems
- Industrial PCs with I2C ports
- Raspberry Pi in control systems
- Any system with native I2C hardware

---

### Solution 2: USB-to-I2C Bridge with MCP2221A

**Hardware Required:**
- Microchip MCP2221A USB-to-I2C adapter (~$5-10)
- Linux system with USB port

**Advantages:**
- ✅ **Plug-and-play** - Appears as `/dev/i2c-*` automatically
- ✅ **Portable** - Works on any Linux system
- ✅ **Safe programming** - Same driver management as native I2C
- ✅ **Cost effective** - Inexpensive USB adapter

**Setup:**
```bash
# 1. Plug in MCP2221A
# 2. Verify detection
ls /dev/i2c-*
lsusb | grep "04d8:00dd"  # Should show MCP2221A

# 3. Use normally
sudo ./LT_PMBusApp -d /dev/i2c-2 -S new_config.hex
```

**Requirements:**
- Kernel driver support (most modern Linux has this built-in)
- For older kernels: Requires Linux 5.7+ or custom kernel build

**Best for:**
- Field service laptops
- Portable programming stations
- Systems without built-in I2C
- Customer self-service scenarios

---

### Solution 3: USB-to-I2C with DC1613A (Linduino)

**Hardware Required:**
- Linear Technology DC1613A (Linduino) board
- Custom firmware (requires one-time setup)

**Advantages:**
- ✅ **Uses existing LT hardware** - Many customers already have this
- ✅ **Binary protocol** - Fast and efficient
- ✅ **Optional PEC support** - Packet error checking

**Setup (One-Time):**
```bash
# 1. Backup original firmware
./backup_dc1613a.sh

# 2. Flash custom serial bridge firmware
# (Upload PMBus_Serial_Bridge.ino via Arduino IDE)

# 3. Use with pmbus_dpsm
./LT_PMBusApp -d /dev/ttyUSB0 -p new_config.hex
```

**Restore Original:**
```bash
./restore_dc1613a.sh dc1613a_backups/dc1613a_*_flash.hex
```

**Considerations:**
- ⚠ Requires firmware modification (reversible)
- ⚠ Needs physical access to RESET button (or WSL2 setup)
- ⚠ Serial protocol instead of native I2C

**Best for:**
- Customers with existing DC1613A hardware
- Development environments
- Lab testing scenarios

---

## Safe Programming Feature (All Solutions)

### The Problem
Programming PMBus EEPROM while drivers are active can cause:
- I2C bus contention
- Corrupted writes
- System instability
- Lost telemetry data

### The Solution: `-S` Option
```bash
sudo ./LT_PMBusApp -d /dev/i2c-1 -S config.hex
```

**What it does:**
1. **Automatically unbinds kernel drivers** (stops device monitoring)
2. **Programs EEPROM** (exclusive I2C bus access)
3. **Verifies write** (ensures data integrity)
4. **Waits for stabilization** (2 seconds for device reset)
5. **Automatically rebinds drivers** (restores system monitoring)

**Safety features:**
- ✅ RAII pattern - Drivers rebind even if interrupted (Ctrl+C)
- ✅ Error handling - Safe cleanup on failures
- ✅ State tracking - Knows which drivers to restore
- ✅ Requires sudo - Prevents accidental use

### Example Output
```
[INFO] Device: /dev/i2c-1
[INFO] Scanning for bound drivers...
[INFO] Found driver 'pmbus' bound to 1-004e
[INFO] Unbinding driver 'pmbus' from 1-004e
[INFO] Programming EEPROM from config.hex
[INFO] Programming complete (5.2s)
[INFO] Verifying EEPROM...
[INFO] Verification passed
[INFO] Waiting 2s for device stabilization...
[INFO] Rebinding driver 'pmbus' to 1-004e
[INFO] Driver management complete
```

---

## Deployment Recommendations

### For System Integrators
**Recommended Setup:**
- Linux system with native I2C
- Use Solution 1 (Native Linux I2C)
- Always use `-S` safe programming option

**Workflow:**
```bash
# 1. Prepare new configuration
vi new_config.hex

# 2. Program safely
sudo ./LT_PMBusApp -d /dev/i2c-1 -S new_config.hex

# 3. Verify system operation
dmesg | tail -20
./LT_PMBusApp -d /dev/i2c-1 -i  # Detect devices
```

### For Field Service
**Recommended Setup:**
- Laptop with USB port
- MCP2221A adapter (Solution 2)
- Pre-configured hex files

**Workflow:**
```bash
# 1. Connect MCP2221A to PMBus device
# 2. Identify I2C bus
ls /dev/i2c-*  # Usually /dev/i2c-2 or higher

# 3. Program
sudo ./LT_PMBusApp -d /dev/i2c-2 -S customer_config.hex
```

### For Development/Lab
**Recommended Setup:**
- Any solution (1, 2, or 3)
- Regular backups of working configurations

**Workflow:**
```bash
# Backup current config
./LT_PMBusApp -d /dev/i2c-1 -e current_backup.hex

# Test new config
sudo ./LT_PMBusApp -d /dev/i2c-1 -S test_config.hex

# Revert if needed
sudo ./LT_PMBusApp -d /dev/i2c-1 -S current_backup.hex
```

---

## Hardware Comparison

| Solution | Cost | Setup Time | Reliability | Portability |
|----------|------|------------|-------------|-------------|
| **Native I2C** | $0 (built-in) | None | ★★★★★ | N/A (embedded) |
| **MCP2221A** | ~$8 | 5 min | ★★★★☆ | ★★★★★ |
| **DC1613A** | ~$150 (if not owned) | 30 min | ★★★☆☆ | ★★★★☆ |

---

## Common Scenarios

### Scenario 1: Factory Configuration
**Environment:** Production line with embedded Linux controller

**Solution:** Native Linux I2C (Solution 1)
```bash
# Program each board as it comes through
for board in /dev/i2c-{1..8}; do
    sudo ./LT_PMBusApp -d $board -S production_config.hex
done
```

### Scenario 2: Customer Site Update
**Environment:** Customer facility, device in operation

**Solution:** MCP2221A USB adapter (Solution 2)
```bash
# Service tech brings laptop + MCP2221A
sudo ./LT_PMBusApp -d /dev/i2c-2 -S customer_update_v2.hex
```

### Scenario 3: Development Iteration
**Environment:** Lab bench testing

**Solution:** Any (Native I2C or MCP2221A)
```bash
# Quick iteration cycles
./LT_PMBusApp -d /dev/i2c-1 -e baseline.hex
sudo ./LT_PMBusApp -d /dev/i2c-1 -S test_v1.hex
# Test...
sudo ./LT_PMBusApp -d /dev/i2c-1 -S baseline.hex  # Revert
```

### Scenario 4: Remote System
**Environment:** Embedded system accessible via SSH

**Solution:** Native Linux I2C (Solution 1)
```bash
# SSH into remote system
ssh user@remote-system

# Upload config
scp new_config.hex user@remote-system:/tmp/

# Program remotely
sudo ./LT_PMBusApp -d /dev/i2c-1 -S /tmp/new_config.hex
```

---

## Safety Guidelines

### DO:
- ✅ Always use `-S` option for safe programming in production
- ✅ Backup current configuration before changes
- ✅ Verify programming with `-e` export and compare
- ✅ Test new configurations in lab before field deployment
- ✅ Keep hex files version controlled

### DON'T:
- ❌ Use `-p` option on systems with active drivers
- ❌ Program without backups
- ❌ Interrupt programming (wait for completion)
- ❌ Power cycle during programming
- ❌ Program without verifying hex file integrity

---

## Troubleshooting

### Problem: "Permission denied"
**Solution:**
```bash
# Add user to i2c group
sudo usermod -a -G i2c $USER
# Or use sudo
sudo ./LT_PMBusApp -d /dev/i2c-1 -S config.hex
```

### Problem: "Device busy"
**Solution:** Use `-S` option instead of `-p`
```bash
# Wrong - may fail if drivers active
./LT_PMBusApp -d /dev/i2c-1 -p config.hex

# Right - handles drivers automatically
sudo ./LT_PMBusApp -d /dev/i2c-1 -S config.hex
```

### Problem: "MCP2221A not detected"
**Solution:**
```bash
# Check USB connection
lsusb | grep 04d8:00dd

# Check kernel driver
lsmod | grep mcp2221

# For older kernels, may need kernel update
uname -r  # Check version >= 5.7
```

### Problem: "Programming verification failed"
**Solution:**
```bash
# 1. Check I2C connection quality
# 2. Verify hex file integrity
md5sum config.hex

# 3. Try slower speed (if supported)
# 4. Check for I2C bus noise/interference
```

---

## File Checklist for Customers

Provide customers with:

1. **Binary:**
   - `LT_PMBusApp` - Main programming tool

2. **Scripts:**
   - `SAFE_PROGRAMMING_TEST.sh` - Pre-flight check script
   - `backup_dc1613a.sh` - DC1613A firmware backup (if applicable)
   - `restore_dc1613a.sh` - DC1613A firmware restore (if applicable)

3. **Documentation:**
   - `SAFE_PROGRAMMING_DEMO.md` - Safe programming user guide
   - `DC1613A_FIRMWARE_BACKUP.md` - DC1613A specific guide (if applicable)
   - This file (`CUSTOMER_DEPLOYMENT_GUIDE.md`)

4. **Configuration Files:**
   - Customer-specific `.hex` files
   - Baseline/factory `.hex` files for recovery

---

## Quick Start for Customers

### Step 1: Choose Hardware
- **Have native I2C?** → Use Solution 1
- **Need portable USB?** → Buy MCP2221A (Solution 2)
- **Already have DC1613A?** → Use Solution 3

### Step 2: Test Setup
```bash
# Check I2C devices
ls /dev/i2c-*

# Detect PMBus devices
./LT_PMBusApp -d /dev/i2c-1 -i
```

### Step 3: Backup Current Config
```bash
./LT_PMBusApp -d /dev/i2c-1 -e backup_$(date +%Y%m%d).hex
```

### Step 4: Program Safely
```bash
sudo ./LT_PMBusApp -d /dev/i2c-1 -S new_config.hex
```

### Step 5: Verify
```bash
# Re-detect to confirm device is online
./LT_PMBusApp -d /dev/i2c-1 -i

# Check system logs
dmesg | tail -20
```

---

## Support Information

### For Technical Support
- Documentation: `/home/cj/pmbus_dpsm/SAFE_PROGRAMMING_DEMO.md`
- Test script: `/home/cj/pmbus_dpsm/SAFE_PROGRAMMING_TEST.sh`
- Source code: `analogdevicesinc/pmbus_dpsm`

### Kernel Requirements
- **Minimum:** Linux 3.10+
- **MCP2221A:** Linux 5.7+ (or custom build with CONFIG_HID_MCP2221)
- **Native I2C:** Any kernel with i2c-dev support

### Hardware Requirements
- **Solution 1:** I2C bus hardware
- **Solution 2:** USB port + MCP2221A adapter
- **Solution 3:** USB port + DC1613A with custom firmware

---

## Summary

✅ **Three tested solutions** for on-site EEPROM programming
✅ **Safe programming feature** prevents system disruption
✅ **Automatic driver management** for production environments
✅ **Reversible operations** with backup/restore capability
✅ **Portable options** for field service scenarios
✅ **Production ready** with comprehensive error handling

**Recommended for most customers:**
- MCP2221A USB adapter (Solution 2) - Best balance of cost, portability, and reliability
- Always use `-S` safe programming option in production

---

*Last updated: November 13, 2025*
