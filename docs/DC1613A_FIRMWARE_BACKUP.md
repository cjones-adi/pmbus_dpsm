# DC1613A (Linduino) Firmware Backup and Restore Guide

## Overview

The DC1613A uses an **ATmega32U4** microcontroller (Arduino Leonardo compatible). You can backup the existing QuikEval firmware before flashing custom code, then restore it later.

---

## Method 1: Using avrdude (Recommended)

### Install avrdude
```bash
sudo apt-get update
sudo apt-get install avrdude
```

### Backup Current Firmware

**Step 1: Identify the USB serial port**
```bash
ls -la /dev/ttyACM* /dev/ttyUSB*
# Your DC1613A is at /dev/ttyUSB0 (FTDI chip)
# But for programming, it uses a different USB connection
```

**Step 2: Put Linduino in bootloader mode**
- Press RESET button twice quickly (double-tap)
- LED will fade in/out - bootloader active for 8 seconds
- Device will appear as `/dev/ttyACM*` temporarily

**Step 3: Backup flash memory**
```bash
# As soon as you double-tap reset:
avrdude -p atmega32u4 -c avr109 -P /dev/ttyACM0 -b 57600 \
  -U flash:r:dc1613a_backup_flash.hex:i

# Backup EEPROM too:
avrdude -p atmega32u4 -c avr109 -P /dev/ttyACM0 -b 57600 \
  -U eeprom:r:dc1613a_backup_eeprom.hex:i

# Backup fuses (optional):
avrdude -p atmega32u4 -c avr109 -P /dev/ttyACM0 -b 57600 \
  -U lfuse:r:dc1613a_backup_lfuse.hex:i \
  -U hfuse:r:dc1613a_backup_hfuse.hex:i \
  -U efuse:r:dc1613a_backup_efuse.hex:i
```

**Expected output:**
```
avrdude: AVR device initialized and ready to accept instructions
Reading | ################################################## | 100% 0.00s
avrdude: Device signature = 0x1e9587 (probably m32u4)
avrdude: reading flash memory:
Reading | ################################################## | 100% 3.21s
avrdude: writing output file "dc1613a_backup_flash.hex"
avrdude done.  Thank you.
```

**Files created:**
- `dc1613a_backup_flash.hex` - Main program (28KB)
- `dc1613a_backup_eeprom.hex` - EEPROM data (1KB)
- `dc1613a_backup_*fuse.hex` - Fuse settings

---

## Method 2: Using Arduino IDE

### Install Arduino IDE
```bash
# Download from https://www.arduino.cc/en/software
# Or use snap:
sudo snap install arduino
```

### Backup Using Arduino IDE

1. **Open Arduino IDE**
2. **Tools → Board → Arduino AVR Boards → Arduino Leonardo**
3. **Tools → Port → /dev/ttyACM0** (after double-tap reset)
4. **Sketch → Upload Using Programmer** (this reads first)
5. **Use third-party tool to export hex:**
   - File → Preferences → Show verbose output during upload
   - Upload any sketch, copy the .hex path from output
   - The temporary .hex file IS your backup

**Note:** Arduino IDE doesn't have a direct "read firmware" feature, so avrdude is better.

---

## Method 3: Using arduino-cli (Modern CLI Tool)

### Install arduino-cli
```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
sudo mv bin/arduino-cli /usr/local/bin/
```

### Backup with arduino-cli
```bash
# Initialize
arduino-cli config init
arduino-cli core update-index
arduino-cli core install arduino:avr

# Backup (requires avrdude integration)
arduino-cli upload --fqbn arduino:avr:leonardo \
  --port /dev/ttyACM0 \
  --verify \
  --verbose
```

---

## Restore Original Firmware

### Using avrdude
```bash
# Put Linduino in bootloader mode (double-tap reset)
# Then immediately:

# Restore flash
avrdude -p atmega32u4 -c avr109 -P /dev/ttyACM0 -b 57600 \
  -U flash:w:dc1613a_backup_flash.hex:i

# Restore EEPROM
avrdude -p atmega32u4 -c avr109 -P /dev/ttyACM0 -b 57600 \
  -U eeprom:w:dc1613a_backup_eeprom.hex:i
```

---

## Understanding DC1613A USB Connections

The DC1613A has **TWO USB interfaces**:

### 1. FTDI USB-to-Serial (FT232)
- **Appears as:** `/dev/ttyUSB0`
- **Used for:** Communication (QuikEval protocol, PMBus commands)
- **Vendor:** Linear Tech / Analog Devices
- **Cannot be used for:** Programming the ATmega32U4

### 2. ATmega32U4 Native USB
- **Appears as:** `/dev/ttyACM0` (when in bootloader)
- **Used for:** Programming/uploading firmware
- **Bootloader:** Caterina (Arduino Leonardo compatible)
- **Enters bootloader:** Double-tap RESET button

**Important:** You need to access the ATmega32U4's native USB, NOT the FTDI chip!

---

## Quick Backup Script

Create `backup_dc1613a.sh`:
```bash
#!/bin/bash

BACKUP_DIR="dc1613a_backups"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BACKUP_NAME="dc1613a_${TIMESTAMP}"

echo "=== DC1613A Firmware Backup Tool ==="
echo ""
echo "Instructions:"
echo "1. When prompted, double-tap the RESET button on DC1613A"
echo "2. Wait for LED to start fading in/out"
echo "3. Press Enter within 8 seconds"
echo ""
read -p "Ready? Press Enter when in bootloader mode..."

mkdir -p "$BACKUP_DIR"

# Try /dev/ttyACM* devices
for port in /dev/ttyACM*; do
  if [ -e "$port" ]; then
    echo "Found bootloader at $port"
    echo "Backing up flash memory..."
    
    avrdude -p atmega32u4 -c avr109 -P "$port" -b 57600 \
      -U flash:r:"${BACKUP_DIR}/${BACKUP_NAME}_flash.hex":i
    
    if [ $? -eq 0 ]; then
      echo "Backing up EEPROM..."
      avrdude -p atmega32u4 -c avr109 -P "$port" -b 57600 \
        -U eeprom:r:"${BACKUP_DIR}/${BACKUP_NAME}_eeprom.hex":i
      
      echo ""
      echo "✅ Backup complete!"
      echo "Files saved to:"
      ls -lh "${BACKUP_DIR}/${BACKUP_NAME}"*
      exit 0
    fi
  fi
done

echo "❌ Error: Could not find bootloader device"
echo "Make sure you double-tapped RESET and ran this within 8 seconds"
exit 1
```

Make it executable:
```bash
chmod +x backup_dc1613a.sh
```

---

## Quick Restore Script

Create `restore_dc1613a.sh`:
```bash
#!/bin/bash

if [ -z "$1" ]; then
  echo "Usage: $0 <backup_flash.hex>"
  echo ""
  echo "Available backups:"
  ls -1 dc1613a_backups/*_flash.hex 2>/dev/null | sed 's/_flash.hex$//'
  exit 1
fi

FLASH_FILE="$1"
EEPROM_FILE="${FLASH_FILE/_flash.hex/_eeprom.hex}"

if [ ! -f "$FLASH_FILE" ]; then
  echo "Error: Flash file not found: $FLASH_FILE"
  exit 1
fi

echo "=== DC1613A Firmware Restore Tool ==="
echo ""
echo "Will restore:"
echo "  Flash:  $FLASH_FILE"
[ -f "$EEPROM_FILE" ] && echo "  EEPROM: $EEPROM_FILE"
echo ""
echo "Instructions:"
echo "1. When prompted, double-tap the RESET button on DC1613A"
echo "2. Wait for LED to start fading in/out"
echo "3. Press Enter within 8 seconds"
echo ""
read -p "Ready? Press Enter when in bootloader mode..."

# Try /dev/ttyACM* devices
for port in /dev/ttyACM*; do
  if [ -e "$port" ]; then
    echo "Found bootloader at $port"
    echo "Restoring flash memory..."
    
    avrdude -p atmega32u4 -c avr109 -P "$port" -b 57600 \
      -U flash:w:"$FLASH_FILE":i
    
    if [ $? -eq 0 ]; then
      if [ -f "$EEPROM_FILE" ]; then
        echo "Restoring EEPROM..."
        avrdude -p atmega32u4 -c avr109 -P "$port" -b 57600 \
          -U eeprom:w:"$EEPROM_FILE":i
      fi
      
      echo ""
      echo "✅ Restore complete!"
      echo "Device will reboot automatically"
      exit 0
    fi
  fi
done

echo "❌ Error: Could not find bootloader device"
exit 1
```

Make it executable:
```bash
chmod +x restore_dc1613a.sh
```

---

## Testing the Backup

### Verify backup file integrity:
```bash
# Check file size (should be ~28KB for flash)
ls -lh dc1613a_backup_flash.hex

# View hex file content (Intel HEX format)
head -20 dc1613a_backup_flash.hex
```

**Expected format:**
```
:100000000C9472000C949A000C949A000C949A0038
:100010000C949A000C949A000C949A000C949A00F0
:100020000C949A000C949A000C9429100C940310E1
...
:00000001FF
```

---

## Troubleshooting

### Problem: "avrdude: ser_open(): can't open device"
**Cause:** Not in bootloader mode or wrong port
**Solution:** 
- Double-tap RESET button
- Check `/dev/ttyACM*` appears within 8 seconds
- Run avrdude command immediately

### Problem: "avrdude: butterfly_recv(): programmer is not responding"
**Cause:** Bootloader timeout (8 second window expired)
**Solution:** Double-tap RESET again and retry quickly

### Problem: Port disappears after programming
**Cause:** Normal - device reboots into application mode
**Solution:** This is expected. Device becomes `/dev/ttyUSB0` after reboot

### Problem: Wrong USB device selected
**Cause:** Using `/dev/ttyUSB0` instead of `/dev/ttyACM0`
**Solution:** 
- `/dev/ttyUSB0` = FTDI chip (communication only)
- `/dev/ttyACM0` = ATmega32U4 bootloader (programming only)

---

## Storage Recommendations

### Keep backups safe:
```bash
# Create timestamped backup directory
mkdir -p ~/dc1613a_firmware_backups/$(date +%Y%m%d)

# Copy backup files
cp dc1613a_backup_*.hex ~/dc1613a_firmware_backups/$(date +%Y%m%d)/

# Add README
cat > ~/dc1613a_firmware_backups/$(date +%Y%m%d)/README.txt << 'EOF'
DC1613A Linduino Original Firmware Backup
==========================================
Date: $(date)
Device: Linear Technology DC1613A
MCU: ATmega32U4
Bootloader: Caterina (Arduino Leonardo compatible)

Files:
- *_flash.hex   - Main program memory (28KB)
- *_eeprom.hex  - EEPROM data (1KB)
- *_*fuse.hex   - Fuse bit settings

To restore:
avrdude -p atmega32u4 -c avr109 -P /dev/ttyACM0 -b 57600 \
  -U flash:w:*_flash.hex:i
EOF

# Create tarball for archival
cd ~/dc1613a_firmware_backups
tar -czf dc1613a_backup_$(date +%Y%m%d).tar.gz $(date +%Y%m%d)/
```

---

## Summary - Quick Steps

### Backup (before custom firmware):
```bash
sudo apt-get install avrdude
# Double-tap RESET on DC1613A
avrdude -p atmega32u4 -c avr109 -P /dev/ttyACM0 -b 57600 \
  -U flash:r:dc1613a_original.hex:i
```

### Restore (back to QuikEval):
```bash
# Double-tap RESET on DC1613A
avrdude -p atmega32u4 -c avr109 -P /dev/ttyACM0 -b 57600 \
  -U flash:w:dc1613a_original.hex:i
```

**Total time:** ~30 seconds per operation
**Risk level:** Very low (bootloader is protected)
**Reversible:** Yes, 100% reversible

---

## Next Steps

After backing up the original firmware, you can:
1. Flash custom serial protocol firmware for pmbus_dpsm
2. Test the serial bridge implementation
3. Easily restore original QuikEval firmware when needed
4. Switch between firmwares as needed

The bootloader is write-protected, so even if firmware upload fails, you can always retry.
