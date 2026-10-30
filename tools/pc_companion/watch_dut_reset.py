#!/usr/bin/env python3
"""
Watch DUT VCP device and notify the AutoFuzzer ESP32 when the DUT resets
or disconnects. The script watches for /dev/ttyACM0 disappearance and sends
`DUTRESET` command to the ESP32 via the AutoFuzzerConnection.

Usage: python tools/pc_companion/watch_dut_reset.py [--dut /dev/ttyACM0] [--esp /dev/ttyUSB0]
"""
import time
import os
import argparse
from tools.pc_companion.connection import AutoFuzzerConnection


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--dut', default='/dev/ttyACM0')
    p.add_argument('--esp', default=None)
    args = p.parse_args()

    conn = AutoFuzzerConnection(port=args.esp)
    if not conn.connect():
        print('[!] Could not connect to ESP32. Exiting.')
        return

    dut_path = args.dut
    prev_exists = os.path.exists(dut_path)
    print(f'Watching {dut_path} (exists={prev_exists})')

    try:
        while True:
            exists = os.path.exists(dut_path)
            if prev_exists and not exists:
                print('[*] DUT device disappeared — sending DUTRESET to ESP32')
                conn.send_and_wait('DUTRESET', wait=0.5)
            prev_exists = exists
            time.sleep(0.5)
    except KeyboardInterrupt:
        print('\nExiting watcher')
    finally:
        conn.disconnect()

if __name__ == '__main__':
    main()
