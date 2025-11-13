# PMBus EEPROM Programming - Documentation Index

## Quick Navigation

### 🚀 Start Here (For Customers)
- **[CUSTOMER_DEPLOYMENT_GUIDE.md](CUSTOMER_DEPLOYMENT_GUIDE.md)** - Complete deployment guide with 3 hardware solutions
  - Best for: Field service, system integrators, on-site updates
  - Covers: Hardware options, safety guidelines, troubleshooting

### 📖 User Guides
- **[SAFE_PROGRAMMING_DEMO.md](SAFE_PROGRAMMING_DEMO.md)** - User guide for safe programming feature
  - Best for: End users learning the `-S` option
  - Covers: Usage examples, output interpretation, timing

- **[QUICKSTART.md](QUICKSTART.md)** - Quick start guide (from previous session)
  - Best for: Getting started quickly
  - Covers: Basic setup and first commands

### 🔧 Hardware Specific
- **[DC1613A_FIRMWARE_BACKUP.md](DC1613A_FIRMWARE_BACKUP.md)** - DC1613A backup/restore complete guide
  - Best for: DC1613A users needing firmware management
  - Covers: avrdude usage, bootloader, manual methods

- **[DC1613A_SOFTWARE_RESET.md](DC1613A_SOFTWARE_RESET.md)** - Software reset without button access
  - Best for: Enclosed DC1613A devices
  - Covers: 1200 baud reset, Python scripts, alternatives

- **[DC1613A_SUMMARY.md](DC1613A_SUMMARY.md)** - DC1613A options and decision matrix
  - Best for: Choosing best approach for DC1613A
  - Covers: WSL2 issues, usbipd, kernel rebuild

### 💻 Developer Documentation
- **[SAFE_PROGRAMMING_IMPLEMENTATION.md](SAFE_PROGRAMMING_IMPLEMENTATION.md)** - Technical implementation details
  - Best for: Developers understanding the code
  - Covers: Architecture, RAII pattern, code structure

- **[USB_I2C_BRIDGE.md](USB_I2C_BRIDGE.md)** - Serial protocol specification (from previous session)
  - Best for: Understanding serial bridge protocol
  - Covers: Binary protocol, packet format, PEC

- **[INTEGRATION_SUMMARY.md](INTEGRATION_SUMMARY.md)** - Integration guide (from previous session)
  - Best for: Understanding how components fit together
  - Covers: Class hierarchy, build system

### 📊 Summary Documents
- **[FEATURE_SUMMARY.md](FEATURE_SUMMARY.md)** - Complete feature summary
  - Best for: Project overview, what was implemented
  - Covers: All files, decisions, metrics, status

### 🧪 Testing
- **[SAFE_PROGRAMMING_TEST.sh](SAFE_PROGRAMMING_TEST.sh)** - Automated test script
  - Usage: `sudo ./SAFE_PROGRAMMING_TEST.sh`
  - Purpose: Validate setup before programming

- **[backup_dc1613a.sh](backup_dc1613a.sh)** - DC1613A firmware backup script
  - Usage: `./backup_dc1613a.sh`
  - Purpose: Save original QuikEval firmware

- **[restore_dc1613a.sh](restore_dc1613a.sh)** - DC1613A firmware restore script
  - Usage: `./restore_dc1613a.sh backup_file.hex`
  - Purpose: Restore original firmware

---

## Documentation by Role

### Field Service Technician
1. Read: **CUSTOMER_DEPLOYMENT_GUIDE.md** (Solution 2: MCP2221A)
2. Hardware: Buy MCP2221A USB adapter (~$8)
3. Script: Use `SAFE_PROGRAMMING_TEST.sh` for pre-flight checks
4. Command: `sudo ./LT_PMBusApp -d /dev/i2c-2 -S config.hex`

### System Integrator
1. Read: **CUSTOMER_DEPLOYMENT_GUIDE.md** (Solution 1: Native I2C)
2. Hardware: Use built-in I2C bus
3. Reference: **SAFE_PROGRAMMING_DEMO.md** for options
4. Command: `sudo ./LT_PMBusApp -d /dev/i2c-1 -S config.hex`

### Lab Engineer / Developer
1. Read: **SAFE_PROGRAMMING_IMPLEMENTATION.md**
2. Reference: **USB_I2C_BRIDGE.md** for serial protocol
3. Hardware: Any solution (native I2C, MCP2221A, or DC1613A)
4. Testing: Use **QUICKSTART.md** for basic operations

### DC1613A Owner
1. Read: **DC1613A_FIRMWARE_BACKUP.md** (complete guide)
2. Backup: Run `./backup_dc1613a.sh` first
3. Problem with enclosed unit? See **DC1613A_SOFTWARE_RESET.md**
4. Troubleshooting: Check **DC1613A_SUMMARY.md**

### Software Developer
1. Read: **SAFE_PROGRAMMING_IMPLEMENTATION.md** (architecture)
2. Read: **INTEGRATION_SUMMARY.md** (how it fits together)
3. Source: `src/LT_I2CDriverManager.cpp/h` (driver management)
4. Source: `src/LT_SMBusSerial.cpp/h` (serial bridge base)

---

## Documentation by Topic

### Safe Programming Feature
- **SAFE_PROGRAMMING_DEMO.md** - User guide
- **SAFE_PROGRAMMING_IMPLEMENTATION.md** - Technical details
- **SAFE_PROGRAMMING_TEST.sh** - Test script
- **CUSTOMER_DEPLOYMENT_GUIDE.md** - Deployment guide

### DC1613A (Linduino)
- **DC1613A_FIRMWARE_BACKUP.md** - Backup/restore guide
- **DC1613A_SOFTWARE_RESET.md** - Software reset methods
- **DC1613A_SUMMARY.md** - Decision guide
- **backup_dc1613a.sh** - Backup script
- **restore_dc1613a.sh** - Restore script

### USB-to-I2C Bridge
- **USB_I2C_BRIDGE.md** - Serial protocol spec
- **INTEGRATION_SUMMARY.md** - Integration details
- **CUSTOMER_DEPLOYMENT_GUIDE.md** - Hardware options

### General Usage
- **QUICKSTART.md** - Getting started
- **CUSTOMER_DEPLOYMENT_GUIDE.md** - Complete deployment
- **FEATURE_SUMMARY.md** - Everything implemented

---

## Common Tasks

### Task: Program EEPROM on Production System
**Files to read:**
1. CUSTOMER_DEPLOYMENT_GUIDE.md (choose hardware)
2. SAFE_PROGRAMMING_DEMO.md (learn -S option)

**Command:**
```bash
sudo ./LT_PMBusApp -d /dev/i2c-1 -S config.hex
```

### Task: Setup Portable Programming Station
**Files to read:**
1. CUSTOMER_DEPLOYMENT_GUIDE.md (Solution 2: MCP2221A)
2. SAFE_PROGRAMMING_TEST.sh (validation)

**Hardware:**
- Laptop with Linux
- MCP2221A USB adapter
- I2C cables

### Task: Backup DC1613A Firmware
**Files to read:**
1. DC1613A_FIRMWARE_BACKUP.md (complete guide)
2. DC1613A_SOFTWARE_RESET.md (if enclosed)

**Command:**
```bash
./backup_dc1613a.sh
```

### Task: Understand How It Works
**Files to read:**
1. SAFE_PROGRAMMING_IMPLEMENTATION.md (architecture)
2. INTEGRATION_SUMMARY.md (integration)
3. USB_I2C_BRIDGE.md (serial protocol)

**Source files:**
- `src/LT_I2CDriverManager.cpp/h`
- `src/LT_SMBusSerial.cpp/h`

### Task: Troubleshoot Issues
**By symptom:**
- "Permission denied" → SAFE_PROGRAMMING_DEMO.md (Troubleshooting)
- "Device busy" → CUSTOMER_DEPLOYMENT_GUIDE.md (use -S option)
- DC1613A bootloader issues → DC1613A_SUMMARY.md
- MCP2221A not detected → CUSTOMER_DEPLOYMENT_GUIDE.md (kernel requirements)

---

## File Statistics

### Documentation (Markdown)
| File | Lines | Topic |
|------|-------|-------|
| CUSTOMER_DEPLOYMENT_GUIDE.md | ~500 | Deployment guide |
| DC1613A_FIRMWARE_BACKUP.md | ~450 | DC1613A backup |
| SAFE_PROGRAMMING_DEMO.md | ~400 | User guide |
| DC1613A_SOFTWARE_RESET.md | ~400 | Software reset |
| SAFE_PROGRAMMING_IMPLEMENTATION.md | ~350 | Technical details |
| DC1613A_SUMMARY.md | ~300 | Decision guide |
| FEATURE_SUMMARY.md | ~400 | Project summary |
| USB_I2C_BRIDGE.md | ~300 | Protocol spec |
| INTEGRATION_SUMMARY.md | ~250 | Integration |
| QUICKSTART.md | ~150 | Quick start |

**Total:** ~3,500 lines of documentation

### Scripts (Bash/Shell)
| File | Lines | Purpose |
|------|-------|---------|
| backup_dc1613a.sh | ~225 | Firmware backup |
| restore_dc1613a.sh | ~190 | Firmware restore |
| SAFE_PROGRAMMING_TEST.sh | ~150 | Validation test |

**Total:** ~565 lines of scripts

### Source Code (C++)
| File | Lines | Purpose |
|------|-------|---------|
| LT_I2CDriverManager.cpp | ~250 | Driver management impl |
| LT_I2CDriverManager.h | ~50 | Driver management header |
| LT_SMBusSerial.cpp | ~200 | Serial base class |
| LT_SMBusSerialPec.cpp | ~100 | Serial with PEC |
| LT_SMBusSerialNoPec.cpp | ~80 | Serial without PEC |
| LT_PMBusApp.cpp (changes) | ~50 | Added -S option |

**Total:** ~730 lines of code (new/modified)

---

## Quick Reference

### Safe Programming
```bash
sudo ./LT_PMBusApp -d /dev/i2c-1 -S config.hex
```

### Standard Programming
```bash
./LT_PMBusApp -d /dev/i2c-1 -p config.hex
```

### Device Detection
```bash
./LT_PMBusApp -d /dev/i2c-1 -i
```

### Export Configuration
```bash
./LT_PMBusApp -d /dev/i2c-1 -e backup.hex
```

### DC1613A Backup
```bash
./backup_dc1613a.sh
```

### DC1613A Restore
```bash
./restore_dc1613a.sh backup_file.hex
```

### Pre-Flight Check
```bash
sudo ./SAFE_PROGRAMMING_TEST.sh
```

---

## Hardware Quick Reference

### MCP2221A
- **Cost:** ~$8
- **Interface:** USB → `/dev/i2c-*`
- **Driver:** Kernel (Linux 5.7+)
- **Best for:** Portable, field service

### DC1613A
- **Cost:** ~$150
- **Interface:** USB → `/dev/ttyUSB0`
- **Firmware:** Custom (reversible)
- **Best for:** Existing LT hardware users

### Native I2C
- **Cost:** $0 (built-in)
- **Interface:** `/dev/i2c-*`
- **Driver:** i2c-dev
- **Best for:** Embedded systems

---

## Support

### Technical Issues
1. Check: **CUSTOMER_DEPLOYMENT_GUIDE.md** (Troubleshooting)
2. Check: **SAFE_PROGRAMMING_DEMO.md** (Common Issues)
3. Check: **DC1613A_SUMMARY.md** (Hardware-specific)

### Feature Requests
- Repository: `analogdevicesinc/pmbus_dpsm`
- Branch: `master`

### Questions
- Documentation: Start with **CUSTOMER_DEPLOYMENT_GUIDE.md**
- Technical: See **SAFE_PROGRAMMING_IMPLEMENTATION.md**

---

*Last updated: November 13, 2025*
*Total documentation: 10 files, ~4800 lines*
