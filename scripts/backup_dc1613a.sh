#!/bin/bash
# DC1613A Firmware Backup Script
# Saves the original QuikEval firmware before flashing custom code

set -e

BACKUP_DIR="dc1613a_backups"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BACKUP_NAME="dc1613a_${TIMESTAMP}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

SERIAL_PORT="/dev/ttyUSB0"
USE_SOFTWARE_RESET=1

echo -e "${BLUE}=== DC1613A Firmware Backup Tool ===${NC}"
echo ""
echo "This will backup the current firmware from your DC1613A (Linduino)."
echo ""
echo -e "${YELLOW}Reset Method:${NC}"
if [ "$USE_SOFTWARE_RESET" = "1" ]; then
    echo "  Using software reset (1200 baud trigger) - no button press needed!"
else
    echo "  Using manual reset button"
fi
echo ""
echo -e "${YELLOW}Note:${NC} The DC1613A has TWO USB interfaces:"
echo "  - FTDI chip (/dev/ttyUSB0) - for communication"
echo "  - ATmega32U4 (/dev/ttyACM*) - for programming (appears in bootloader)"
echo ""

# Check if avrdude is installed
if ! command -v avrdude &> /dev/null; then
    echo -e "${RED}Error: avrdude is not installed${NC}"
    echo ""
    echo "Install with:"
    echo "  sudo apt-get install avrdude"
    exit 1
fi

# Check if python3 is available for software reset
if [ "$USE_SOFTWARE_RESET" = "1" ]; then
    if ! command -v python3 &> /dev/null; then
        echo -e "${RED}Error: python3 not found (needed for software reset)${NC}"
        echo "Install: sudo apt install python3"
        exit 1
    fi
    
    # Check for pyserial
    if ! python3 -c "import serial" 2>/dev/null; then
        echo -e "${YELLOW}Python serial library not found. Installing...${NC}"
        sudo apt install -y python3-serial || {
            echo -e "${RED}Failed to install python3-serial${NC}"
            echo "Try: sudo apt install python3-pip && pip3 install pyserial"
            exit 1
        }
    fi
    
    # Check if serial port exists
    if [ ! -e "$SERIAL_PORT" ]; then
        echo -e "${RED}Error: $SERIAL_PORT not found${NC}"
        echo ""
        echo "Available serial ports:"
        ls -la /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "  None found"
        exit 1
    fi
fi

if [ "$USE_SOFTWARE_RESET" = "1" ]; then
    echo -e "${BLUE}Triggering bootloader via software reset (1200 baud)...${NC}"
    
    # Use Python to trigger reset
    python3 << EOF
import serial
import time
try:
    print("Opening $SERIAL_PORT at 1200 baud...")
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
    
    echo "Waiting for bootloader to start..."
    sleep 2
else
    echo -e "${YELLOW}Instructions:${NC}"
    echo "1. Double-tap (quickly press twice) the RESET button on DC1613A"
    echo "2. Watch for LED to start fading in/out"
    echo "3. Press Enter within 8 seconds"
    echo ""
    read -p "Ready? Double-tap RESET now, then press Enter..."
fi

# Create backup directory
mkdir -p "$BACKUP_DIR"

# Try to find bootloader device (with retries for software reset)
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
        echo "  Attempt $attempt/$MAX_ATTEMPTS - waiting for bootloader..."
        sleep 1
    fi
done

if [ -z "$BOOTLOADER_PORT" ]; then
  if [ "$USE_SOFTWARE_RESET" = "1" ]; then
      echo -e "${YELLOW}⚠ Software reset didn't trigger bootloader${NC}"
      echo ""
      echo "Possible causes:"
      echo "  - WSL2 USB pass-through limitation"
      echo "  - FTDI chip doesn't relay DTR to ATmega32U4"
      echo "  - Firmware doesn't support 1200 baud reset"
      echo ""
      echo -e "${BLUE}Falling back to manual reset method...${NC}"
      echo ""
      echo -e "${YELLOW}Manual Instructions:${NC}"
      echo "1. If DC1613A is in an enclosure, you'll need physical access"
      echo "2. Locate the RESET button (small tactile switch)"
      echo "3. Double-tap (quickly press twice) the RESET button"
      echo "4. Watch for LED to start fading in/out"
      echo "5. Press Enter within 8 seconds"
      echo ""
      read -p "Ready? Double-tap RESET now, then press Enter..."
      
      # Try again with manual reset
      for attempt in $(seq 1 3); do
          for port in /dev/ttyACM*; do
              if [ -e "$port" ]; then
                  BOOTLOADER_PORT="$port"
                  break 2
              fi
          done
          if [ $attempt -lt 3 ]; then
              echo "  Waiting... (attempt $attempt/3)"
              sleep 1
          fi
      done
  fi
  
  if [ -z "$BOOTLOADER_PORT" ]; then
      echo -e "${RED}❌ Error: Could not find bootloader device${NC}"
      echo ""
      echo "Final troubleshooting steps:"
      echo "  1. Verify DC1613A is powered and connected"
      echo "  2. Check: lsusb | grep 'Linear Tech'"
      echo "  3. Try a different USB port (not through hub)"
      echo "  4. Check device permissions: ls -la /dev/ttyUSB* /dev/ttyACM*"
      echo ""
      echo "For WSL2 users:"
      echo "  - In Windows PowerShell (Admin): usbipd wsl list"
      echo "  - Attach device: usbipd wsl attach --busid X-Y"
      echo "  - The ATmega32U4 may need separate attachment"
      echo ""
      echo "Alternative: Use physical access to RESET button or send device"
      echo "to a system with native Linux USB support."
      exit 1
  fi
fi

echo -e "${GREEN}Found bootloader at $BOOTLOADER_PORT${NC}"
echo ""

# Backup flash memory
echo -e "${BLUE}[1/2] Backing up flash memory (28KB)...${NC}"
if avrdude -p atmega32u4 -c avr109 -P "$BOOTLOADER_PORT" -b 57600 \
     -U flash:r:"${BACKUP_DIR}/${BACKUP_NAME}_flash.hex":i 2>&1 | \
     grep -E "(Reading|writing|Device signature|avrdude done)"; then
  echo -e "${GREEN}✓ Flash backup complete${NC}"
else
  echo -e "${RED}❌ Flash backup failed${NC}"
  exit 1
fi

echo ""

# Backup EEPROM
echo -e "${BLUE}[2/2] Backing up EEPROM (1KB)...${NC}"
if avrdude -p atmega32u4 -c avr109 -P "$BOOTLOADER_PORT" -b 57600 \
     -U eeprom:r:"${BACKUP_DIR}/${BACKUP_NAME}_eeprom.hex":i 2>&1 | \
     grep -E "(Reading|writing|Device signature|avrdude done)"; then
  echo -e "${GREEN}✓ EEPROM backup complete${NC}"
else
  echo -e "${YELLOW}⚠ EEPROM backup failed (not critical)${NC}"
fi

echo ""
echo -e "${GREEN}✅ Backup complete!${NC}"
echo ""
echo "Files saved:"
ls -lh "${BACKUP_DIR}/${BACKUP_NAME}"*.hex

echo ""
echo -e "${BLUE}Backup location:${NC}"
echo "  $(pwd)/${BACKUP_DIR}/"
echo ""
echo -e "${YELLOW}Important:${NC} Keep these files safe!"
echo "You'll need them to restore the original QuikEval firmware."
echo ""
echo "To restore later, run:"
echo "  ./restore_dc1613a.sh ${BACKUP_DIR}/${BACKUP_NAME}_flash.hex"
