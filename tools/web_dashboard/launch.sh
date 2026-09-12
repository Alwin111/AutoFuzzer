#!/bin/bash
# AutoFuzzer Web Dashboard — Quick Launch
# Usage: ./launch.sh [--port /dev/ttyUSB1] [--baud 115200]
cd "$(dirname "$0")"
echo "============================================"
echo "  AUTO FUZZER — Web Dashboard"
echo "============================================"
echo ""
echo "  Open http://localhost:5000 in your browser"
echo ""
python3 server.py "$@"
