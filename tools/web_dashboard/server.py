#!/usr/bin/env python3
"""
AutoFuzzer Web Dashboard Server — stable detached launch
===============================================

Usage (detached):
    nohup python3 tools/web_dashboard/server.py \
            --port /dev/ttyUSB0 --baud 115200 --web-port 5000 \
            > .freebuff/preview-log 2>&1 < /dev/null &
    disown $!

Then open http://localhost:5000 in the browser.
"""

import os
import sys
import time
import re
import argparse
import glob
import socket
import subprocess
import threading


def _ensure_workdir():
    here = os.path.dirname(os.path.realpath(__file__))
    project_root = os.path.abspath(os.path.join(here, '..'))
    if project_root not in sys.path:
        sys.path.insert(0, project_root)
    os.chdir(project_root)


def _wait_for_port(host, port, timeout=8):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            s = socket.create_connection((host, port), timeout=0.5)
            s.close()
            return True
        except OSError:
            time.sleep(0.2)
    return False


def main():
    _ensure_workdir()

    from flask import Flask, render_template, jsonify, request
    from flask_socketio import SocketIO
    import serial
    import serial.tools.list_ports

    app = Flask(__name__)
    app.config['SECRET_KEY'] = 'autofuzzer-dashboard'
    socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

    # ---------------- Global mutable state ----------------
    serial_conn = None
    serial_lock = threading.Lock()
    connected = False            # user-visible connection state
    auto_reconnect = True        # try to reopen the port if USB drops
    reconnecting = False
    last_target_port = None
    last_target_baud = 115200

    live_state = {
        'connected': False,
        'reconnecting': False,
        'port': '',
        'baud': 115200,
        'protocol': 'UNKNOWN',
        'profile': 'UNKNOWN',
        'state': 'IDLE',
        'packets': 0,
        'acks': 0,
        'nacks': 0,
        'errors': 0,
        'failures': 0,
        'heartbeat_ok': False,
        'mutation': 'NONE',
        'phase': 'IDLE',
        'elapsed_s': 0,
        'duration_s': 0,
        'robustness_score': 0,
        'verdict': 'N/A',
        'serial_log': [],
        'mutations_used': {},
        'timeline': [],
        'failure_history': [],
        'campaign_history': [],
    }

    MAX_LOG_LINES = 500
    MAX_TIMELINE = 200

    # ---------------- Helpers ----------------
    def auto_detect_port():
        """Best-effort heuristic for the ESP32 (CP210x / Silicon Labs) serial port."""
        ports = serial.tools.list_ports.comports()
        esp32_keywords = ['CP210', 'SILICON LABS', 'ESP32']
        for p in ports:
            desc = (p.description or '').upper()
            mfg = (p.manufacturer or '').upper()
            if any(k in desc or k in mfg for k in esp32_keywords):
                return p.device
        for p in ports:
            desc = (p.description or '').upper()
            if 'USB' in desc or 'ACM' in p.device or 'USB' in p.device:
                return p.device
        return (glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*') or [None])[0]

    def get_live_state_snapshot():
        return {
            'connected': connected and bool(serial_conn and serial_conn.is_open),
            'reconnecting': reconnecting,
            'port': live_state['port'],
            'baud': live_state['baud'],
            'protocol': live_state['protocol'],
            'profile': live_state['profile'],
            'state': live_state['state'],
            'packets': live_state['packets'],
            'acks': live_state['acks'],
            'nacks': live_state['nacks'],
            'errors': live_state['errors'],
            'failures': live_state['failures'],
            'heartbeat_ok': live_state['heartbeat_ok'],
            'mutation': live_state['mutation'],
            'phase': live_state['phase'],
            'elapsed_s': live_state['elapsed_s'],
            'duration_s': live_state['duration_s'],
            'robustness_score': live_state['robustness_score'],
            'verdict': live_state['verdict'],
            'mutations_used': dict(live_state['mutations_used']),
            'timeline': live_state['timeline'][-MAX_TIMELINE:],
            'log_tail': live_state['serial_log'][-30:],
            'failure_history': list(live_state['failure_history']),
            'campaign_history': list(live_state['campaign_history']),
        }

    def push_event(event, data=None):
        live_state['timeline'].append({'time': time.time(), 'event': event, 'data': data})
        if len(live_state['timeline']) > MAX_TIMELINE:
            live_state['timeline'] = live_state['timeline'][-MAX_TIMELINE:]

    def parse_serial_line(line):
        upper = line.upper()

        # ---- Heartbeat (check LOST first so DETECTED never masks a timeout) ----
        if 'HEARTBEAT TIMEOUT' in upper or 'HB: LOST' in upper or 'HEARTBEAT: LOST' in upper:
            live_state['heartbeat_ok'] = False
        elif 'HEARTBEAT DETECTED' in upper or 'HB: OK' in upper or 'HEARTBEAT: OK' in upper:
            live_state['heartbeat_ok'] = True

        # ---- Campaign start: "PROTOCOL CHECK PASS — CAMPAIGN STARTED: UART protocol, QUICK profile" ----
        if 'CAMPAIGN STARTED' in upper or 'CAMPAIGN START' in upper:
            live_state['state'] = 'RUNNING'
            push_event('CAMPAIGN_START')
            m = re.search(r'(\w+)\s+protocol,\s*(\w+)\s+profile', line, re.IGNORECASE)
            if m:
                live_state['protocol'] = m.group(1).upper()
                live_state['profile'] = m.group(2).upper()
                push_event('PROFILE', f"{m.group(1).upper()} {m.group(2).upper()}")
            else:
                m2 = re.search(r'Campaign START:\s*(\w+)\s+profile', line, re.IGNORECASE)
                if m2:
                    live_state['profile'] = m2.group(1).upper()
            live_state['packets'] = 0
            live_state['acks'] = 0
            live_state['nacks'] = 0
            live_state['errors'] = 0
            live_state['failures'] = 0
            live_state['mutations_used'] = {}
            live_state['verdict'] = 'N/A'
            live_state['robustness_score'] = 0

        # ---- Protocol set commands / messages ----
        if 'PROTOCOL SET TO UART' in upper or 'SETUART' in upper:
            live_state['protocol'] = 'UART'
        elif 'PROTOCOL SET TO SPI' in upper or 'SETSPI' in upper:
            live_state['protocol'] = 'SPI'
        elif 'PROTOCOL SET TO I2C' in upper or 'SETI2C' in upper:
            live_state['protocol'] = 'I2C'

        # ---- Phase: "Phase -> BASELINE (5000/30000 ms)" ----
        pm = re.search(r'phase\s*(?:->|:)\s*(\w+)', line, re.IGNORECASE)
        if pm:
            live_state['phase'] = pm.group(1).upper()
            push_event('PHASE', live_state['phase'])
        tm = re.search(r'\((\d+)/(\d+)\s*ms\)', line)
        if tm:
            live_state['elapsed_s'] = int(tm.group(1)) // 1000
            live_state['duration_s'] = int(tm.group(2)) // 1000
        else:
            tm2 = re.search(r'time\s*[:=]\s*(\d+)/(\d+)', line, re.IGNORECASE)
            if tm2:
                live_state['elapsed_s'] = int(tm2.group(1))
                live_state['duration_s'] = int(tm2.group(2))

        # ---- Per-packet DUT response: "RESP seq=12 status=0x00 ACK" ----
        rm = re.search(r'resp\s+seq=(\d+)\s+status=0x([0-9A-Fa-f]+)\s*(\w*)', line, re.IGNORECASE)
        if rm:
            status_byte = int(rm.group(2), 16)
            if status_byte == 0x00:
                live_state['acks'] += 1
            else:
                live_state['nacks'] += 1

        # ---- Packet count from live output / STATUS ----
        if m := re.search(r'packets?\s*[:=]\s*(\d+)', line, re.IGNORECASE):
            live_state['packets'] = int(m.group(1))

        # ---- Mutation: "Mutation: BAD_CRC" or similar ----
        if m := re.search(r'mutation\s*[:=]\s*([\w/]+)', line, re.IGNORECASE):
            name = m.group(1).upper()
            live_state['mutation'] = name
            live_state['mutations_used'][name] = live_state['mutations_used'].get(name, 0) + 1

        # ---- Failure detection ----
        if 'FAILURE DETECTED' in upper or '!!! FAILURE' in upper:
            live_state['failures'] += 1
            live_state['state'] = 'FAILURE'
            live_state['failure_history'].append({
                'time': time.time(), 'mutation': live_state['mutation'],
            })
            push_event('FAILURE', live_state['mutation'])

        # ---- Completion / results ----
        if 'CAMPAIGN COMPLETE' in upper or 'TEST COMPLETE' in upper:
            live_state['state'] = 'COMPLETE'
            push_event('CAMPAIGN_COMPLETE')
            cm = re.search(r'(\d+) packets,\s*(\d+) acks,\s*(\d+) nacks,\s*(\d+) failures', line, re.IGNORECASE)
            if cm:
                live_state['packets'] = int(cm.group(1))
                live_state['acks'] = int(cm.group(2))
                live_state['nacks'] = int(cm.group(3))
                live_state['failures'] = int(cm.group(4))

        if 'ABORT' in upper and 'INCONCLUSIVE' in upper:
            live_state['state'] = 'COMPLETE'
            live_state['verdict'] = 'INCONCLUSIVE'
            push_event('VERDICT', 'INCONCLUSIVE')

        if 'AUTOFUZZER TEST RESULT' in upper:
            live_state['state'] = 'COMPLETE'

        if m := re.search(r'robust(?:ness)?\s*score\s*[:=]?\s*(\d+)\s*/\s*100', line, re.IGNORECASE):
            live_state['robustness_score'] = int(m.group(1))

        if m := re.search(r'verdict\s*[:=]?\s*(PASS|FAIL|INCONCLUSIVE)', line, re.IGNORECASE):
            live_state['verdict'] = m.group(1).upper()
            push_event('VERDICT', m.group(1).upper())

        # ---- STATUS: "State: FUZZING" etc. ----
        if m := re.search(r'state\s*[:=]\s*(\w+)', line, re.IGNORECASE):
            sv = m.group(1).upper()
            mapping = {'FUZZING': 'RUNNING', 'IDLE': 'IDLE', 'PAUSED': 'PAUSED',
                       'FAILURE': 'FAILURE', 'REPLAY': 'REPLAY', 'COMPLETE': 'COMPLETE',
                       'RUNNING': 'RUNNING', 'RESULT': 'COMPLETE'}
            if sv in mapping:
                live_state['state'] = mapping[sv]

        # ---- Protocol check waiting: "Protocol check: UART — waiting for DUT heartbeat..." ----
        if 'PROTOCOL CHECK' in upper and 'WAITING' in upper:
            live_state['state'] = 'PROTOCOL_CHECK'
            pm2 = re.search(r'protocol check:\s*(\w+)', line, re.IGNORECASE)
            if pm2:
                live_state['protocol'] = pm2.group(1).upper()

        # ---- Replay / minimize progress ----
        if m := re.search(r'replay\s*#(\d+)\s*/\s*(\d+)', line, re.IGNORECASE):
            push_event('REPLAY', f"Attempt {m.group(1)}/{m.group(2)}")
        if 'REPLAY RESULT' in upper:
            push_event('REPLAY_DONE', line.strip())
        if 'MINIMIZE COMPLETE' in upper:
            push_event('MINIMIZED', line.strip())

    # ---------------- Serial thread (with auto-reconnect) ----------------
    def serial_reader_thread():
        nonlocal connected, reconnecting, serial_conn, last_target_port, last_target_baud
        last_err = None
        while True:
            if connected and serial_conn and serial_conn.is_open:
                try:
                    if serial_conn.in_waiting > 0:
                        raw = serial_conn.readline()
                        line = raw.decode('utf-8', errors='replace').strip()
                        if line:
                            timestamp = time.strftime('%H:%M:%S')
                            live_state['serial_log'].append(f"[{timestamp}] {line}")
                            if len(live_state['serial_log']) > MAX_LOG_LINES:
                                live_state['serial_log'] = live_state['serial_log'][-MAX_LOG_LINES:]
                            parse_serial_line(line)
                            socketio.emit('serial_line', {
                                'line': line,
                                'time': timestamp,
                                'state': get_live_state_snapshot()
                            })
                    else:
                        time.sleep(0.01)
                        last_err = None
                except (serial.SerialException, OSError) as exc:
                    msg = str(exc)
                    if last_err != msg:
                        last_err = msg
                        print(f"[!] Serial lost: {msg}")
                    try:
                        serial_conn.close()
                    except Exception:
                        pass
                    serial_conn = None
                    if auto_reconnect and last_target_port:
                        reconnecting = True
                        live_state['reconnecting'] = True
                        socketio.emit('serial_line', {
                            'line': 'USB disconnected — waiting for device to come back...',
                            'time': time.strftime('%H:%M:%S'),
                            'state': get_live_state_snapshot()
                        })
                    else:
                        connected = False
                        live_state['connected'] = False
                    time.sleep(0.5)
                    continue
                except Exception as exc:
                    if last_err != str(exc):
                        last_err = str(exc)
                        print(f"[!] Serial read error: {exc}")
                    time.sleep(0.2)
                    continue
            elif reconnecting and last_target_port:
                # Try to reopen the same port every 2 seconds
                try:
                    time.sleep(2)
                    candidate = last_target_port
                    if not os.path.exists(candidate):
                        # Device may come back with a different number; rescan
                        cand2 = auto_detect_port()
                        if cand2:
                            candidate = cand2
                    if candidate and os.path.exists(candidate):
                        conn = serial.Serial(candidate, last_target_baud, timeout=0.1)
                        time.sleep(0.5)
                        if conn.is_open:
                            serial_conn = conn
                            connected = True
                            reconnecting = False
                            live_state['reconnecting'] = False
                            live_state['connected'] = True
                            live_state['port'] = candidate
                            last_target_port = candidate
                            print(f"[*] Reconnected to {candidate}")
                            socketio.emit('serial_line', {
                                'line': f'Reconnected to {candidate} @ {last_target_baud} baud',
                                'time': time.strftime('%H:%M:%S'),
                                'state': get_live_state_snapshot()
                            })
                except Exception:
                    pass  # keep waiting; loop retries
            else:
                time.sleep(0.1)

    def open_serial(port, baud):
        nonlocal serial_conn
        try:
            serial_conn = serial.Serial(port, baud, timeout=0.1)
            time.sleep(0.5)
            if serial_conn.is_open:
                return True
        except Exception:
            serial_conn = None
            return False
        serial_conn = None
        return False

    def close_serial():
        nonlocal serial_conn
        if serial_conn:
            try:
                serial_conn.close()
            except Exception:
                pass
            serial_conn = None

    def start_reader():
        t = threading.Thread(target=serial_reader_thread, daemon=True)
        t.start()
        return t

    def do_connect(port, baud):
        """Shared connect logic. Returns (ok, message)."""
        nonlocal connected, reconnecting, last_target_port, last_target_baud
        close_serial()
        if open_serial(port, baud):
            connected = True
            reconnecting = False
            last_target_port = port
            last_target_baud = baud
            live_state['connected'] = True
            live_state['reconnecting'] = False
            live_state['port'] = port
            live_state['baud'] = baud
            start_reader()
            return True, f"Connected to {port} @ {baud} baud"
        return False, f"Could not open {port}"

    # ---------------- Flask routes ----------------
    @app.route('/')
    def index():
        return render_template('index.html')

    @app.route('/api/status')
    def api_status():
        return jsonify(get_live_state_snapshot())

    @app.route('/api/connect', methods=['POST'])
    def api_connect():
        data = request.json or {}
        port = data.get('port')
        baud = int(data.get('baud', 115200))
        if not port:
            port = auto_detect_port()
        if not port:
            return jsonify({'error': 'No ESP32 found. Connect via USB.'}), 404
        if connected and serial_conn and serial_conn.is_open:
            return jsonify({'status': 'already_connected', 'port': live_state['port']})
        ok, msg = do_connect(port, baud)
        if ok:
            socketio.emit('serial_line', {
                'line': msg, 'time': time.strftime('%H:%M:%S'),
                'state': get_live_state_snapshot()
            })
            return jsonify({'status': 'connected', 'port': port, 'baud': baud})
        return jsonify({'error': msg}), 500

    @app.route('/api/disconnect', methods=['POST'])
    def api_disconnect():
        nonlocal connected, reconnecting, auto_reconnect
        auto_reconnect = False
        close_serial()
        connected = False
        reconnecting = False
        live_state['connected'] = False
        live_state['reconnecting'] = False
        live_state['port'] = ''
        live_state['heartbeat_ok'] = False
        socketio.emit('serial_line', {
            'line': 'Disconnected from ESP32',
            'time': time.strftime('%H:%M:%S'),
            'state': get_live_state_snapshot()
        })
        auto_reconnect = True   # re-enable for future connects
        return jsonify({'status': 'disconnected'})

    @app.route('/api/send', methods=['POST'])
    def api_send():
        nonlocal connected, last_target_port, last_target_baud
        data = request.json or {}
        cmd = data.get('command', '').strip()
        if not cmd:
            return jsonify({'error': 'No command'}), 400
        if not (serial_conn and serial_conn.is_open):
            # Auto-connect if disconnected (e.g. first use after server start)
            port = auto_detect_port()
            if port:
                ok, msg = do_connect(port, 115200)
                if not ok:
                    return jsonify({'error': msg}), 400
            else:
                return jsonify({'error': 'Not connected — no ESP32 found.'}), 400
        try:
            with serial_lock:
                serial_conn.write((cmd + '\n').encode('utf-8'))
            return jsonify({'status': 'sent', 'command': cmd})
        except Exception as exc:
            print(f"[!] Serial write error for command '{cmd}': {exc}")
            return jsonify({'error': str(exc)}), 500

    @app.route('/api/ports')
    def api_ports():
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
        for key in live_state:
            if key == 'serial_log':
                continue
            val = live_state[key]
            if isinstance(val, dict):
                live_state[key] = {}
            elif isinstance(val, (list, tuple)):
                live_state[key] = []
            elif isinstance(val, str):
                live_state[key] = 'UNKNOWN' if key != 'state' else 'IDLE'
            elif isinstance(val, bool):
                live_state[key] = False
            else:
                live_state[key] = 0
        live_state['state'] = 'IDLE'
        live_state['heartbeat_ok'] = False
        live_state['connected'] = connected and bool(serial_conn and serial_conn.is_open)
        return jsonify({'status': 'reset'})

    # ---------------- SocketIO events ----------------
    @socketio.on('connect')
    def handle_connect():
        from flask_socketio import emit
        emit('connected', {'state': get_live_state_snapshot()})

    @socketio.on('request_state')
    def handle_request_state():
        from flask_socketio import emit
        emit('state_update', get_live_state_snapshot())

    # ---------------- Parse argv ----------------
    parser = argparse.ArgumentParser(description='AutoFuzzer Web Dashboard')
    parser.add_argument('--port', '-p', help='Serial port (auto-detected if omitted)')
    parser.add_argument('--baud', '-b', type=int, default=115200, help='Baud rate')
    parser.add_argument('--host', default='0.0.0.0', help='Web server host')
    parser.add_argument('--web-port', type=int, default=5000, help='Web server port')
    parser.add_argument('--detach', action='store_true', help='Self-detach into a child process')
    args = parser.parse_args()

    host = args.host
    port = args.web_port
    serial_port = args.port or auto_detect_port()
    baud = args.baud

    # ---------------- Self-detach for nohup stability ----------------
    if args.detach:
        if os.environ.get('AUTOFUZZER_DASHBOARD_CHILD') != '1':
            env = dict(os.environ)
            env['AUTOFUZZER_DASHBOARD_CHILD'] = '1'
            try:
                proc = subprocess.Popen(
                    [sys.executable, __file__, '--port', str(serial_port or ''),
                     '--baud', str(baud), '--host', host, '--web-port', str(port)],
                    env=env,
                    stdin=subprocess.DEVNULL,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    start_new_session=True,
                )
            except Exception as exc:
                print(f"[!] Failed to spawn child server: {exc}", file=sys.stderr)
                raise SystemExit(1)
            if not _wait_for_port(host, port, timeout=12):
                print(f"[!] Dashboard child did not bind :{port} within 12s — check logs.", file=sys.stderr)
                raise SystemExit(2)
            print(f"[*] Dashboard child listening on :{port} (pid {proc.pid})")
            raise SystemExit(0)

    # ---------------- Real Flask server (child) ----------------
    print(f"[*] Auto-detect ESP32 port: {auto_detect_port() or 'none'}")
    if not serial_port:
        serial_port = auto_detect_port()
    if serial_port:
        print(f"[*] ESP32 on {serial_port}")
        ok, msg = do_connect(serial_port, baud)
        print(f"[*] {msg}" if ok else f"[!] {msg} — use the dashboard Connect button")
    else:
        print("[*] No ESP32 detected. Connect via USB, then use the dashboard.")

    print(f"[*] Dashboard: http://{host}:{port}")
    print("=" * 50)

    socketio.run(app, host=host, port=port, debug=False, allow_unsafe_werkzeug=True)


if __name__ == '__main__':
    main()
