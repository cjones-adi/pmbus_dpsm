#!/bin/bash
# DC1613A Firmware Restore Script
# Restores previously backed up firmware

set -e

SERIAL_PORT="/dev/ttyUSB0"
USE_SOFTWARE_RESET=1

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

if [ -z "$1" ]; then
  echo -e "${BLUE}=== DC1613A Firmware Restore Tool ===${NC}"
  echo ""
  echo "Usage: $0 <backup_flash.hex>"
  echo ""
  echo -e "${YELLOW}Available backups:${NC}"
  if [ -d "dc1613a_backups" ]; then
    for backup in dc1613a_backups/*_flash.hex; do
      if [ -f "$backup" ]; then
        base=$(basename "$backup" _flash.hex)
        size=$(stat -f%z "$backup" 2>/dev/null || stat -c%s "$backup" 2>/dev/null)
        echo "  $backup ($(numfmt --to=iec-i --suffix=B $size 2>/dev/null || echo $size bytes))"
      fi
    done
  else
    echo "  (none found - run ./backup_dc1613a.sh first)"
  fi
  exit 1
fi

FLASH_FILE="$1"
EEPROM_FILE="${FLASH_FILE/_flash.hex/_eeprom.hex}"

if [ ! -f "$FLASH_FILE" ]; then
  echo -e "${RED}Error: Flash file not found: $FLASH_FILE${NC}"
  exit 1
fi

echo -e "${BLUE}=== DC1613A Firmware Restore Tool ===${NC}"
echo ""
echo "Will restore:"
echo "  Flash:  $FLASH_FILE"
if [ -f "$EEPROM_FILE" ]; then
  echo "  EEPROM: $EEPROM_FILE"
else
  echo "  EEPROM: (not found, will skip)"
fi
echo ""
echo -e "${YELLOW}WARNING: This will overwrite the current firmware!${NC}"
echo ""

read -p "Continue? (y/N): " confirm
if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
  echo "Cancelled."
  exit 0
fi

echo ""

# Check if avrdude is installed
if ! command -v avrdude &> /dev/null; then
    echo -e "${RED}Error: avrdude is not installed${NC}"
    echo ""
    echo "Install with:"
    echo "  sudo apt-get install avrdude"
    exit 1
fi

# Check dependencies for software reset
if [ "$USE_SOFTWARE_RESET" = "1" ]; then
    if ! command -v python3 &> /dev/null; then
        echo -e "${RED}Error: python3 not found${NC}"
        exit 1
    fi
    
    if ! python3 -c "import serial" 2>/dev/null; then
        echo -e "${YELLOW}Installing python3-serial...${NC}"
        sudo apt install -y python3-serial || {
            echo -e "${RED}Failed to install python3-serial${NC}"
            exit 1
        }
    fi
    
    if [ ! -e "$SERIAL_PORT" ]; then
        echo -e "${RED}Error: $SERIAL_PORT not found${NC}"
        exit 1
    fi
fi

echo -e "${YELLOW}Reset Method:${NC}"
if [ "$USE_SOFTWARE_RESET" = "1" ]; then
    echo "  Using software reset (1200 baud trigger)"
    echo ""
    echo -e "${BLUE}Triggering bootloader...${NC}"
    
    python3 << EOF
import serial
import time
try:
    ser = serial.Serial('$SERIAL_PORT', 1200)
    time.sleep(0.1)
    ser.close()
    print("✓ Reset signal sent")
except Exception as e:
    print(f"✗ Error: {e}")
    exit(1)
EOF
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}Failed to trigger software reset${NC}"
        exit 1
    fi
    
    echo "Waiting for bootloader..."
    sleep 2
else
    echo "  Using manual reset button"
    echo ""
    echo -e "${YELLOW}Instructions:${NC}"
    echo "1. Locate the RESET button on DC1613A"
    echo "2. When prompted below, double-tap (quickly press twice) the RESET button"
    echo "3. Watch for LED to start fading in/out (bootloader mode)"
    echo "4. Press Enter in this terminal within 8 seconds"
    echo ""
    read -p "Ready? Double-tap RESET now, then press Enter..."
fi

# Wait a moment for device to appear
sleep 1

# Try to find bootloader device (with retries)
BOOTLOADER_PORT=""
MAX_ATTEMPTS=5

for attempt in $(seq 1 $MAX_ATTEMPTS); do
    for port in /dev/ttyACM*; do
        if [ -e "$port" ]; then
            BOOTLOADER_PORT="$port"
            break 2
        fi
    done
    
    if [ $attempt -lt $MAX_ATTEMPTS ]; then
        echo "  Attempt $attempt/$MAX_ATTEMPTS..."
        sleep 1
    fi
done

if [ -z "$BOOTLOADER_PORT" ]; then
  echo -e "${RED}❌ Error: Could not find bootloader device${NC}"
  echo ""
  echo "Troubleshooting:"
  if [ "$USE_SOFTWARE_RESET" = "1" ]; then
      echo "  1. Verify DC1613A is connected"
      echo "  2. Try direct USB connection (not through hub)"
      echo "  3. Try manual reset: Set USE_SOFTWARE_RESET=0"
  else
      echo "  1. Make sure you double-tapped the RESET button"
      echo "  2. Check that LED is fading in/out"
      echo "  3. Run this script again within 8 seconds of double-tap"
  fi
  exit 1
fi

echo -e "${GREEN}Found bootloader at $BOOTLOADER_PORT${NC}"
echo ""

# Restore flash memory
echo -e "${BLUE}[1/2] Restoring flash memory...${NC}"
if avrdude -p atmega32u4 -c avr109 -P "$BOOTLOADER_PORT" -b 57600 \
     -U flash:w:"$FLASH_FILE":i 2>&1 | \
     grep -E "(Reading|writing|Device signature|avrdude done)"; then
  echo -e "${GREEN}✓ Flash restore complete${NC}"
else
  echo -e "${RED}❌ Flash restore failed${NC}"
  exit 1
fi

echo ""

# Restore EEPROM if available
if [ -f "$EEPROM_FILE" ]; then
  echo -e "${BLUE}[2/2] Restoring EEPROM...${NC}"
  if avrdude -p atmega32u4 -c avr109 -P "$BOOTLOADER_PORT" -b 57600 \
       -U eeprom:w:"$EEPROM_FILE":i 2>&1 | \
       grep -E "(Reading|writing|Device signature|avrdude done)"; then
    echo -e "${GREEN}✓ EEPROM restore complete${NC}"
  else
    echo -e "${YELLOW}⚠ EEPROM restore failed (not critical)${NC}"
  fi
else
  echo -e "${YELLOW}[2/2] Skipping EEPROM (backup file not found)${NC}"
fi

echo ""
echo -e "${GREEN}✅ Restore complete!${NC}"
echo ""
echo "The DC1613A will reboot automatically and return to normal operation."
echo "After ~2 seconds, it should appear as /dev/ttyUSB0 (FTDI interface)."
echo ""
echo "Verify with:"
echo "  ls -la /dev/ttyUSB0"
