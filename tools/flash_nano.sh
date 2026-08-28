#!/bin/bash
# ============================================
# Flash Arduino Nano with I2C slave firmware
# ============================================
# The CH340 auto-reset doesn't work on cheap Nano clones.
# You MUST press the RESET button during upload.
#
# USAGE:
#   chmod +x tools/flash_nano.sh
#   ./tools/flash_nano.sh
#
# INSTRUCTIONS:
#   1. Run this script
#   2. When you see "UPLOADING...", press and release RESET on Nano
#   3. Wait for "avrdude done"
# ============================================

set -e

PORT="${1:-/dev/ttyUSB0}"
HEX_FILE="firmware/nano/.pio/build/nanoatmega328/firmware.hex"
AVRDUD="$HOME/.platformio/packages/tool-avrdude/avrdude"
AVRCONF="$HOME/.platformio/packages/tool-avrdude/avrdude.conf"

# Check hex exists
if [ ! -f "$HEX_FILE" ]; then
    echo "ERROR: Hex file not found. Building first..."
    cd firmware/nano && ~/.platformio/penv/bin/pio run && cd ../..
fi

echo "============================================"
echo "  Nano Flash Tool"
echo "============================================"
echo ""
echo "Port:       $PORT"
echo "Firmware:   $HEX_FILE"
echo ""
echo "INSTRUCTIONS:"
echo "  1. Press and release the RESET button on the Nano"
echo "     WHEN you see 'Uploading...' below"
echo "  2. The window is about 1-2 seconds"
echo "  3. We'll retry up to 15 times"
echo ""
echo "Press ENTER when ready..."
read

for i in $(seq 1 15); do
    echo "--- Attempt $i/15 ---"
    echo ">>> UPLOADING... press RESET NOW! <<<"
    
    $AVRDUD -C "$AVRCONF" \
        -p atmega328p -c arduino -P "$PORT" -b 57600 -D \
        -U flash:w:$HEX_FILE:i \
        2>&1 | grep -E "Writing|verif|avrdude done|FAILED"
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "============================================"
        echo "  FLASH COMPLETE!"
        echo "============================================"
        echo ""
        echo "Verify by checking serial output:"
        echo "  Should show: 'I2C slave ready at address 0x10'"
        echo ""
        exit 0
    fi
    
    echo "  Failed. Waiting 2 seconds..."
    sleep 2
done

echo ""
echo "FAILED after 15 attempts."
echo ""
echo "Troubleshooting:"
echo "  1. Make sure Nano is connected via USB"
echo "  2. Check the port with: ls /dev/ttyUSB*"
echo "  3. Try pressing RESET button earlier/faster"
echo "  4. Hardware fix: solder 100nF cap between CH340 DTR and ATmega RESET"
