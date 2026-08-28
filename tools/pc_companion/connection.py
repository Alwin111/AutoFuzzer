"""
AutoFuzzer PC Companion — Serial Connection

Handles USB serial communication with the ESP32.
Provides command sending and data reception.
"""

import time
import threading
import serial
import serial.tools.list_ports
from typing import Optional, Callable


class AutoFuzzerConnection:
    """Manages serial connection to AutoFuzzer ESP32."""

    def __init__(self, port: Optional[str] = None, baud: int = 115200):
        self.port = port
        self.baud = baud
        self._serial: Optional[serial.Serial] = None
        self._running = False
        self._reader_thread: Optional[threading.Thread] = None
        self._callback: Optional[Callable[[str], None]] = None
        self._buffer = ""

    @staticmethod
    def list_ports() -> list[dict]:
        """List available serial ports."""
        ports = serial.tools.list_ports.comports()
        return [
            {
                "port": p.device,
                "description": p.description,
                "hwid": p.hwid,
            }
            for p in ports
        ]

    @staticmethod
    def auto_detect() -> Optional[str]:
        """Auto-detect ESP32 serial port."""
        ports = serial.tools.list_ports.comports()
        for p in ports:
            desc = (p.description or "").lower()
            if any(k in desc for k in ["cp210", "ch340", "ftdi", "silicon labs", "usb-uart", "usb serial"]):
                return p.device
        return None

    def connect(self, port: Optional[str] = None) -> bool:
        """Connect to the ESP32."""
        if self._serial and self._serial.is_open:
            return True

        target_port = port or self.port or self.auto_detect()
        if not target_port:
            print("[!] No ESP32 serial port detected.")
            print("    Available ports:")
            for p in self.list_ports():
                print(f"      {p['port']} — {p['description']}")
            return False

        try:
            self._serial = serial.Serial(target_port, self.baud, timeout=1)
            time.sleep(2)  # Wait for ESP32 reset
            self.port = target_port
            print(f"[+] Connected to {target_port} @ {self.baud} baud")
            return True
        except serial.SerialException as e:
            print(f"[!] Failed to connect: {e}")
            return False

    def disconnect(self):
        """Disconnect from the ESP32."""
        self._running = False
        if self._reader_thread and self._reader_thread.is_alive():
            self._reader_thread.join(timeout=2)
        if self._serial and self._serial.is_open:
            self._serial.close()
            self._serial = None
            print("[+] Disconnected")

    def is_connected(self) -> bool:
        """Check if connected."""
        return self._serial is not None and self._serial.is_open

    def send_command(self, cmd: str) -> bool:
        """Send a command to the ESP32."""
        if not self.is_connected():
            print("[!] Not connected")
            return False
        try:
            self._serial.write(f"{cmd}\n".encode())
            return True
        except serial.SerialException as e:
            print(f"[!] Send failed: {e}")
            return False

    def read_line(self, timeout: float = 1.0) -> Optional[str]:
        """Read a single line from the ESP32."""
        if not self.is_connected():
            return None
        try:
            old_timeout = self._serial.timeout
            self._serial.timeout = timeout
            line = self._serial.readline().decode("utf-8", errors="replace").strip()
            self._serial.timeout = old_timeout
            return line if line else None
        except serial.SerialException:
            return None

    def read_all_lines(self, timeout: float = 2.0) -> list[str]:
        """Read all available lines within timeout."""
        lines = []
        start = time.time()
        while time.time() - start < timeout:
            line = self.read_line(timeout=0.1)
            if line:
                lines.append(line)
            else:
                time.sleep(0.05)
        return lines

    def start_reader(self, callback: Callable[[str], None]):
        """Start background reader thread."""
        if self._running:
            return
        self._callback = callback
        self._running = True
        self._reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self._reader_thread.start()

    def stop_reader(self):
        """Stop background reader thread."""
        self._running = False

    def _reader_loop(self):
        """Background reader loop."""
        while self._running and self.is_connected():
            try:
                line = self._serial.readline().decode("utf-8", errors="replace").strip()
                if line and self._callback:
                    self._callback(line)
            except serial.SerialException:
                break
            except Exception:
                time.sleep(0.01)

    def send_and_wait(self, cmd: str, wait: float = 2.0) -> list[str]:
        """Send a command and collect response lines."""
        self.send_command(cmd)
        time.sleep(0.1)
        return self.read_all_lines(timeout=wait)

    def get_status(self) -> dict:
        """Query ESP32 status."""
        lines = self.send_and_wait("STATUS", wait=1.0)
        status = {}
        for line in lines:
            if ":" in line:
                key, _, val = line.partition(":")
                status[key.strip()] = val.strip()
        return status

    def export_failures(self) -> list[dict]:
        """Request failure export from ESP32 and parse JSON records."""
        import json

        lines = self.send_and_wait("EXPORT", wait=5.0)

        # Find the JSON block between markers
        in_block = False
        json_lines = []
        for line in lines:
            if "AUTOFUZZER_EXPORT_START" in line:
                in_block = True
                continue
            if "AUTOFUZZER_EXPORT_END" in line:
                in_block = False
                continue
            if in_block:
                json_lines.append(line)

        if not json_lines:
            return []

        try:
            json_str = "\n".join(json_lines)
            data = json.loads(json_str)
            return data.get("records", [])
        except json.JSONDecodeError as e:
            print(f"[!] Failed to parse failure export: {e}")
            return []
