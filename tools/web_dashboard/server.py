#!/usr/bin/env python3
"""
AutoFuzzer Web Dashboard Server

Connects to the ESP32 via serial and streams live data to a browser dashboard
using WebSocket (SocketIO). Provides real-time visualization of:

- Serial output (live log)
- Test campaign progress (packets, time, mutations)
- ACK/NACK counters
- Heartbeat status
- Failure detection
- Robustness score
- Mutation distribution
- Test history

Usage:
    python3 server.py                    # Auto-detect ESP32 port
    python3 server.py --port /dev/ttyUSB1  # Specify port
    python3 server.py --baud 115200       # Specify baud rate

Then open http://localhost:5000 in your browser.
"""

import sys
import os
import json
import time
import re
import threading
import argparse
import glob
from datetime import datetime

# Add parent directory for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'pc_companion'))

from flask import Flask, render_template, jsonify, request
from flask_socketio import SocketIO, emit
import serial
import serial.tools.list_ports

# ============================================================
# Flask App
# ============================================================
app = Flask(__name__)
app.config['SECRET_KEY'] = 'autofuzzer-dashboard'
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# ============================================================
# Global State
# ============================================================
serial_conn = None
serial_thread = None
serial_lock = threading.Lock()
connected = False

# Parsed live state from ESP32 serial output
live_state = {
    'connected': False,
    'port': '',
    'baud': 115200,
    'protocol': 'UNKNOWN',
    'profile': 'UNKNOWN',
    'state': 'IDLE',
    'test_num': 0,
    'total_tests': 0,
    'elapsed_ms': 0,
    'duration_ms': 0,
    'acks': 0,
    'nacks': 0,
    'errors': 0,
    'failures': 0,
    'heartbeat_ok': True,
    'mutation': 'NONE',
    'phase': 'IDLE',
    'robustness_score': 0,
    'verdict': 'N/A',
    'serial_log': [],       # last 500 lines
    'mutations_used': {},   # mutation_name -> count
    'mutation_responses': {},  # mutation_name -> {acks, nacks, errors}
    'timeline': [],         # {time, event, data}
    'failure_history': [],  # past failures
    'campaign_history': [], # past campaigns
}

# Max log lines to keep
MAX_LOG_LINES = 500
MAX_TIMELINE = 200


def auto_detect_port():
    """Auto-detect ESP32 serial port."""
    ports = serial.tools.list_ports.comports()
    esp32_keywords = ['CP210', 'CH340', 'FTDI', 'UART', 'USB-SERIAL', 'Silicon Labs']
    
    candidates = []
    for p in ports:
        desc = (p.description or '').upper()
        mfg = (p.manufacturer or '').upper()
        for kw in esp32_keywords:
            if kw in desc or kw in mfg:
                candidates.append(p.device)
                break
    
    # Fallback: any /dev/ttyUSB* or /dev/ttyACM*
    if not candidates:
        candidates = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*')
    
    return candidates[0] if candidates else None


def open_serial(port, baud):
    """Open serial connection to ESP32."""
    global serial_conn, connected, live_state
    try:
        serial_conn = serial.Serial(port, baud, timeout=0.1)
        time.sleep(0.5)
        connected = True
        live_state['connected'] = True
        live_state['port'] = port
        live_state['baud'] = baud
        return True
    except Exception as e:
        print(f"[!] Failed to open {port}: {e}")
        connected = False
        live_state['connected'] = False
        return False


def serial_reader_thread():
    """Background thread that reads serial data and emits to WebSocket."""
    global serial_conn, connected, live_state
    
    while connected and serial_conn and serial_conn.is_open:
        try:
            if serial_conn.in_waiting > 0:
                raw = serial_conn.readline()
                try:
                    line = raw.decode('utf-8', errors='replace').strip()
                except:
                    line = str(raw).strip()
                
                if line:
                    # Add to log
                    timestamp = datetime.now().strftime('%H:%M:%S')
                    log_entry = f"[{timestamp}] {line}"
                    live_state['serial_log'].append(log_entry)
                    if len(live_state['serial_log']) > MAX_LOG_LINES:
                        live_state['serial_log'] = live_state['serial_log'][-MAX_LOG_LINES:]
                    
                    # Parse the line for state updates
                    parse_serial_line(line)
                    
                    # Emit to all connected browsers
                    socketio.emit('serial_line', {
                        'line': line,
                        'time': timestamp,
                        'state': get_live_state_snapshot()
                    })
            else:
                time.sleep(0.01)
        except Exception as e:
            print(f"[!] Serial read error: {e}")
            connected = False
            live_state['connected'] = False
            socketio.emit('serial_error', {'error': str(e)})
            break
    
    print("[*] Serial reader thread stopped")


def parse_serial_line(line):
    """Parse ESP32 serial output to extract state information."""
    global live_state
    upper = line.upper()
    
    # Campaign start
    if 'CAMPAIGN START' in upper:
        live_state['state'] = 'RUNNING'
        live_state['timeline'].append({'time': time.time(), 'event': 'CAMPAIGN_START'})
        
        # Extract profile info
        m = re.search(r'(\w+)\s+(\d+\w*)\s*profile', line, re.IGNORECASE)
        if m:
            live_state['profile'] = f"{m.group(1)} {m.group(2)}"
    
    # Protocol selection
    if 'SETUART' in upper or 'PROTOCOL: UART' in upper:
        live_state['protocol'] = 'UART'
    elif 'SETSPI' in upper or 'PROTOCOL: SPI' in upper:
        live_state['protocol'] = 'SPI'
    elif 'SETI2C' in upper or 'PROTOCOL: I2C' in upper:
        live_state['protocol'] = 'I2C'
    
    # Phase transitions
    phase_match = re.search(r'phase\s*(?:->|:)\s*(\w+)', line, re.IGNORECASE)
    if phase_match:
        new_phase = phase_match.group(1).upper()
        live_state['phase'] = new_phase
        live_state['timeline'].append({'time': time.time(), 'event': 'PHASE', 'data': new_phase})
    
    # Packet count
    packets_match = re.search(r'(?:packets?|tests?)\s*[:=]\s*(\d+)', line, re.IGNORECASE)
    if packets_match:
        live_state['test_num'] = int(packets_match.group(1))
    
    # ACK count
    ack_match = re.search(r'\ba\s*[:=]\s*(\d+)', line, re.IGNORECASE)
    if ack_match:
        live_state['acks'] = int(ack_match.group(1))
    elif 'ACK' in upper and ':' in line:
        ack_match2 = re.search(r'ack[s]?\s*[:=]\s*(\d+)', line, re.IGNORECASE)
        if ack_match2:
            live_state['acks'] = int(ack_match2.group(1))
    
    # NACK count
    nack_match = re.search(r'\bn\s*[:=]\s*(\d+)', line, re.IGNORECASE)
    if nack_match:
        live_state['nacks'] = int(nack_match.group(1))
    elif 'NACK' in upper and ':' in line:
        nack_match2 = re.search(r'nack[s]?\s*[:=]\s*(\d+)', line, re.IGNORECASE)
        if nack_match2:
            live_state['nacks'] = int(nack_match2.group(1))
    
    # Error count
    err_match = re.search(r'(?:err(?:or)?s?)\s*[:=]\s*(\d+)', line, re.IGNORECASE)
    if err_match:
        live_state['errors'] = int(err_match.group(1))
    
    # Heartbeat
    if 'ALIVE' in upper or 'HEARTBEAT: OK' in upper or 'HB: OK' in upper:
        live_state['heartbeat_ok'] = True
    elif 'DEAD' in upper or 'HEARTBEAT: LOST' in upper or 'HB: LOST' in upper or 'HEARTBEAT TIMEOUT' in upper:
        live_state['heartbeat_ok'] = False
    
    # Mutation name
    mut_match = re.search(r'mutation\s*[:=]\s*(\w+)', line, re.IGNORECASE)
    if mut_match:
        mut_name = mut_match.group(1).upper()
        live_state['mutation'] = mut_name
        live_state['mutations_used'][mut_name] = live_state['mutations_used'].get(mut_name, 0) + 1
    
    # Time elapsed
    time_match = re.search(r'time\s*[:=]\s*(\d+)/(\d+)', line, re.IGNORECASE)
    if time_match:
        live_state['elapsed_ms'] = int(time_match.group(1))
        live_state['duration_ms'] = int(time_match.group(2))
    
    # Failure detected
    if 'FAILURE DETECTED' in upper or '!!! FAILURE' in upper:
        live_state['failures'] += 1
        live_state['state'] = 'FAILURE'
        live_state['timeline'].append({
            'time': time.time(), 'event': 'FAILURE',
            'data': live_state['mutation']
        })
    
    # Campaign complete
    if 'CAMPAIGN COMPLETE' in upper or 'TEST COMPLETE' in upper or 'TEST RESULT' in upper:
        live_state['state'] = 'COMPLETE'
        live_state['timeline'].append({'time': time.time(), 'event': 'CAMPAIGN_COMPLETE'})
    
    # Robustness score
    score_match = re.search(r'robustness\s*score\s*[:=]?\s*(\d+)/100', line, re.IGNORECASE)
    if score_match:
        live_state['robustness_score'] = int(score_match.group(1))
    
    # Verdict
    verdict_match = re.search(r'verdict\s*[:=]?\s*(PASS|FAIL|INCONCLUSIVE)', line, re.IGNORECASE)
    if verdict_match:
        live_state['verdict'] = verdict_match.group(1).upper()
        live_state['timeline'].append({
            'time': time.time(), 'event': 'VERDICT',
            'data': live_state['verdict']
        })
    
    # State
    state_match = re.search(r'state\s*[:=]?\s*(\w+)', line, re.IGNORECASE)
    if state_match:
        state_val = state_match.group(1).upper()
        if state_val in ['IDLE', 'RUNNING', 'PAUSED', 'FAILURE', 'REPLAY', 'COMPLETE']:
            live_state['state'] = state_val
    
    # Test result summary (from RESULT command)
    if 'AUTOFUZZER TEST RESULT' in upper:
        live_state['state'] = 'RESULT'
    
    # Replay results
    replay_match = re.search(r'replay\s*#?(\d+)', line, re.IGNORECASE)
    if replay_match:
        live_state['timeline'].append({
            'time': time.time(), 'event': 'REPLAY',
            'data': f"Attempt {replay_match.group(1)}"
        })
    
    # Robustness score at end
    if 'ROBUSTNESS SCORE' in upper:
        score_match2 = re.search(r'(\d+)/100', line)
        if score_match2:
            live_state['robustness_score'] = int(score_match2.group(1))


def get_live_state_snapshot():
    """Return a copy of current state for WebSocket emission."""
    return {
        'connected': live_state['connected'],
        'port': live_state['port'],
        'baud': live_state['baud'],
        'protocol': live_state['protocol'],
        'profile': live_state['profile'],
        'state': live_state['state'],
        'test_num': live_state['test_num'],
        'total_tests': live_state['total_tests'],
        'elapsed_ms': live_state['elapsed_ms'],
        'duration_ms': live_state['duration_ms'],
        'acks': live_state['acks'],
        'nacks': live_state['nacks'],
        'errors': live_state['errors'],
        'failures': live_state['failures'],
        'heartbeat_ok': live_state['heartbeat_ok'],
        'mutation': live_state['mutation'],
        'phase': live_state['phase'],
        'robustness_score': live_state['robustness_score'],
        'verdict': live_state['verdict'],
        'mutations_used': live_state['mutations_used'],
        'timeline': live_state['timeline'][-MAX_TIMELINE:],
        'failure_history': live_state['failure_history'],
        'campaign_history': live_state['campaign_history'],
    }


# ============================================================
# Flask Routes
# ============================================================
@app.route('/')
def index():
    return render_template('index.html')


@app.route('/api/status')
def api_status():
    return jsonify(get_live_state_snapshot())


@app.route('/api/connect', methods=['POST'])
def api_connect():
    global serial_thread, connected
    data = request.json or {}
    port = data.get('port')
    baud = int(data.get('baud', 115200))
    
    if not port:
        port = auto_detect_port()
    
    if not port:
        return jsonify({'error': 'No ESP32 found. Connect via USB.'}), 404
    
    if connected and serial_conn and serial_conn.is_open:
        return jsonify({'status': 'already_connected', 'port': live_state['port']})
    
    if open_serial(port, baud):
        serial_thread = threading.Thread(target=serial_reader_thread, daemon=True)
        serial_thread.start()
        return jsonify({'status': 'connected', 'port': port, 'baud': baud})
    else:
        return jsonify({'error': f'Failed to open {port}'}), 500


@app.route('/api/disconnect', methods=['POST'])
def api_disconnect():
    global serial_conn, connected
    connected = False
    if serial_conn and serial_conn.is_open:
        serial_conn.close()
    serial_conn = None
    live_state['connected'] = False
    return jsonify({'status': 'disconnected'})


@app.route('/api/send', methods=['POST'])
def api_send():
    global serial_conn
    data = request.json or {}
    cmd = data.get('command', '').strip()
    if not cmd:
        return jsonify({'error': 'No command'}), 400
    
    if not serial_conn or not serial_conn.is_open:
        return jsonify({'error': 'Not connected'}), 400
    
    try:
        serial_conn.write((cmd + '\n').encode('utf-8'))
        serial_conn.flush()
        return jsonify({'status': 'sent', 'command': cmd})
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/api/ports')
def api_ports():
    """List available serial ports."""
    ports = []
    for p in serial.tools.list_ports.comports():
        ports.append({
            'device': p.device,
            'description': p.description,
            'manufacturer': p.manufacturer or '',
            'hwid': p.hwid or '',
        })
    return jsonify({'ports': ports, 'auto_detect': auto_detect_port()})


@app.route('/api/reset_state', methods=['POST'])
def api_reset_state():
    """Reset all live state."""
    for key in live_state:
        if key == 'connected':
            continue
        elif isinstance(live_state[key], dict):
            live_state[key] = {}
        elif isinstance(live_state[key], list):
            live_state[key] = []
        elif isinstance(live_state[key], str):
            live_state[key] = 'UNKNOWN' if key != 'state' else 'IDLE'
        elif isinstance(live_state[key], bool):
            live_state[key] = True
        else:
            live_state[key] = 0
    live_state['state'] = 'IDLE'
    live_state['heartbeat_ok'] = True
    return jsonify({'status': 'reset'})


# ============================================================
# SocketIO Events
# ============================================================
@socketio.on('connect')
def handle_connect():
    emit('connected', {'state': get_live_state_snapshot()})


@socketio.on('request_state')
def handle_request_state():
    emit('state_update', get_live_state_snapshot())


# ============================================================
# Main
# ============================================================
def main():
    parser = argparse.ArgumentParser(description='AutoFuzzer Web Dashboard')
    parser.add_argument('--port', '-p', help='Serial port (auto-detected if omitted)')
    parser.add_argument('--baud', '-b', type=int, default=115200, help='Baud rate')
    parser.add_argument('--host', default='0.0.0.0', help='Web server host')
    parser.add_argument('--web-port', type=int, default=5000, help='Web server port')
    args = parser.parse_args()
    
    print("=" * 50)
    print("  AUTO FUZZER — Web Dashboard")
    print("=" * 50)
    
    # Try to auto-connect to ESP32
    port = args.port or auto_detect_port()
    if port:
        print(f"[*] Found ESP32 on {port}")
        if open_serial(port, args.baud):
            print(f"[*] Connected at {args.baud} baud")
            serial_thread = threading.Thread(target=serial_reader_thread, daemon=True)
            serial_thread.start()
        else:
            print(f"[!] Could not connect. You can connect from the dashboard.")
    else:
        print("[*] No ESP32 detected. Connect via USB, then use the dashboard.")
    
    print(f"[*] Dashboard: http://localhost:{args.web_port}")
    print(f"[*] Press Ctrl+C to stop")
    print("=" * 50)
    
    socketio.run(app, host=args.host, port=args.web_port, debug=False, allow_unsafe_werkzeug=True)


if __name__ == '__main__':
    main()
