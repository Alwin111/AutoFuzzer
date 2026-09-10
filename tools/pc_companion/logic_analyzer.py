"""
AutoFuzzer PC Companion — Logic Analyzer Capture

Integrates with Saleae-compatible 8-channel 24 MHz logic analyzer
using sigrok-cli for passive bus capture.

Architecture:
  ESP32 fuzzing independently
      +
  Logic Analyzer passive capture (independent ground truth)
      ↓
  PC correlation after test/failure

The LA is NOT a dependency for normal fuzzing.
It provides wire-level verification and failure forensics.

Channel mapping (from hardware):
  CH0 = UART TX (ESP32 -> DUT)
  CH1 = UART RX (DUT -> ESP32)
  CH2 = Heartbeat
  CH3 = SPI SCK (when SPI mode)
  CH4 = SPI MOSI (when SPI mode)
  CH5 = SPI MISO (when SPI mode)
  CH6 = I2C SDA (when I2C mode)
  CH7 = I2C SCL (when I2C mode)
"""

import os
import json
import time
import subprocess
import shutil
from pathlib import Path
from typing import Optional
from dataclasses import dataclass, asdict


@dataclass
class LACaptureConfig:
    """Configuration for a logic analyzer capture session."""
    sample_rate: int = 1_000_000   # 1 MHz (adequate for UART/SPI/I2C)
    capture_ms: int = 10_000       # 10 seconds default
    protocol: str = "UART"         # UART, SPI, I2C
    trigger_channel: int = 2       # Heartbeat channel for trigger
    pre_trigger_ms: int = 500      # How much before trigger to keep
    output_dir: str = "captures"


@dataclass
class LACaptureResult:
    """Result of a logic analyzer capture."""
    vcd_path: str
    duration_ms: int
    sample_rate: int
    channels_used: list
    timestamp: str
    size_bytes: int


class LogicAnalyzerCapture:
    """Manages logic analyzer captures via sigrok-cli."""

    # Channel definitions per protocol mode
    CHANNEL_MAP = {
        "UART": {
            "tx": 0, "rx": 1, "heartbeat": 2,
            "names": {0: "UART_TX", 1: "UART_RX", 2: "HEARTBEAT"}
        },
        "SPI": {
            "sck": 3, "mosi": 4, "miso": 5, "cs": 6, "heartbeat": 2,
            "names": {2: "HEARTBEAT", 3: "SPI_SCK", 4: "SPI_MOSI", 5: "SPI_MISO", 6: "SPI_CS"}
        },
        "I2C": {
            "sda": 6, "scl": 7, "heartbeat": 2,
            "names": {2: "HEARTBEAT", 6: "I2C_SDA", 7: "I2C_SCL"}
        },
    }

    def __init__(self):
        self._sigrok_available = self._check_sigrok()
        self._capture_dir = Path("captures")
        self._capture_dir.mkdir(exist_ok=True)

    def _check_sigrok(self) -> bool:
        """Check if sigrok-cli is installed."""
        return shutil.which("sigrok-cli") is not None

    def is_available(self) -> bool:
        """Check if LA capture is available."""
        return self._sigrok_available

    def _detect_driver(self) -> Optional[str]:
        """Auto-detect available logic analyzer driver."""
        if not self._sigrok_available:
            return None
        try:
            result = subprocess.run(
                ["sigrok-cli", "--scan"],
                capture_output=True, text=True, timeout=10
            )
            for line in result.stdout.splitlines():
                line_lower = line.lower()
                if "fx2lafw" in line_lower or "saleae" in line_lower:
                    return "fx2lafw"
                elif "demo" in line_lower:
                    return "demo"
            # Fallback to demo if no real device found
            return "demo"
        except (subprocess.TimeoutExpired, FileNotFoundError):
            return "demo"

    def get_device_list(self) -> list:
        """List available logic analyzer devices."""
        if not self._sigrok_available:
            return []
        try:
            result = subprocess.run(
                ["sigrok-cli", "--scan", "--driver", "fx2lafw"],
                capture_output=True, text=True, timeout=10
            )
            devices = []
            for line in result.stdout.splitlines():
                if "fx2lafw" in line or "saleae" in line.lower():
                    devices.append(line.strip())
            return devices
        except (subprocess.TimeoutExpired, FileNotFoundError):
            return []

    def start_capture(self, config: LACaptureConfig, driver: str = None) -> Optional[LACaptureResult]:
        """
        Start a logic analyzer capture.

        This runs sigrok-cli in the background for the specified duration.
        Returns the capture result when complete.
        """
        if not self._sigrok_available:
            print("[!] sigrok-cli not found. Install with: sudo apt install sigrok-cli")
            return None

        # Auto-detect driver if not specified
        if driver is None:
            driver = self._detect_driver()
            if not driver:
                print("[!] No logic analyzer device found. Connect one or use 'demo' for testing.")
                return None

        # Build channel list based on protocol
        channel_info = self.CHANNEL_MAP.get(config.protocol, self.CHANNEL_MAP["UART"])
        channels = list(channel_info["names"].keys())
        channel_names = channel_info["names"]

        # Generate output filename
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        filename = f"capture_{config.protocol}_{timestamp}.vcd"
        output_path = self._capture_dir / filename

        # Build sigrok-cli command
        decoder = self._get_decoder(config.protocol)
        num_samples = config.sample_rate * config.capture_ms // 1000
        cmd = [
            "sigrok-cli",
            "-d", driver,
            "--samples", str(num_samples),
            "-O", "vcd",  # Force VCD output format
            "-o", str(output_path),
        ]
        # Add samplerate config and channels only for real devices
        if driver != "demo":
            cmd.extend(["-c", f"samplerate={config.sample_rate}"])
            cmd.extend(["-C", ",".join(str(c) for c in channels)])
            # Add protocol decoder only for real devices
            if decoder:
                cmd.extend(["-P", decoder])

        print(f"[*] Starting LA capture: {config.protocol} @ {config.sample_rate/1e6:.1f} MHz")
        print(f"    Duration: {config.capture_ms} ms")
        print(f"    Channels: {channels}")
        print(f"    Output: {output_path}")

        try:
            # Run capture (blocking for now)
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=config.capture_ms // 1000 + 10)

            if result.returncode != 0:
                print(f"[!] sigrok-cli error: {result.stderr}")
                return None

            # Check if output file exists
            if not output_path.exists():
                print("[!] Capture file not created")
                return None

            file_size = output_path.stat().st_size

            capture_result = LACaptureResult(
                vcd_path=str(output_path),
                duration_ms=config.capture_ms,
                sample_rate=config.sample_rate,
                channels_used=channels,
                timestamp=timestamp,
                size_bytes=file_size,
            )

            print(f"[+] Capture complete: {file_size} bytes")
            return capture_result

        except subprocess.TimeoutExpired:
            print("[!] Capture timed out")
            return None
        except FileNotFoundError:
            print("[!] sigrok-cli not found")
            return None

    def _get_decoder(self, protocol: str) -> str:
        """Get the sigrok protocol decoder name."""
        decoders = {
            "UART": "uart",
            "SPI": "spi",
            "I2C": "i2c",
        }
        return decoders.get(protocol, "uart")

    def parse_vcd(self, vcd_path: str) -> dict:
        """
        Parse a VCD (Value Change Dump) file and extract timing data.

        Returns a dictionary with:
          - channels: dict of channel_id -> list of (timestamp_ns, value) tuples
          - metadata: dict with timing info
        """
        channels = {}
        metadata = {}
        id_to_name = {}  # Map VCD IDs (!, ", #, etc.) to channel names

        try:
            with open(vcd_path, "rb") as f:
                raw = f.read()
            try:
                text = raw.decode("utf-8")
            except UnicodeDecodeError:
                text = raw.decode("latin-1")
            
            lines = text.splitlines()
            current_timestamp = 0

            for line in lines:
                line = line.strip()

                # Parse header metadata
                if line.startswith("$timescale"):
                    metadata["timescale"] = line
                elif line.startswith("$var"):
                    # Variable definition: $var wire 1 ! D0 $end
                    parts = line.split()
                    if len(parts) >= 5:
                        vcd_id = parts[3]  # !, ", #, etc.
                        ch_name = parts[4]  # D0, D1, etc.
                        id_to_name[vcd_id] = ch_name
                        channels[ch_name] = []
                elif line.startswith("#"):
                    # Timestamp line may also contain value changes: #0 1! 0" 0#
                    parts = line.split(None, 1)
                    try:
                        current_timestamp = int(parts[0][1:])
                    except ValueError:
                        pass
                    # Parse value changes on the same line
                    rest = parts[1] if len(parts) > 1 else ""
                    if rest and rest[0] in "01xzXZ":
                        line = rest  # Fall through to value parsing below
                    else:
                        continue
                if line and line[0] in "01xzXZ" and len(line) >= 2:
                    # Value change on current timestamp: 0! means D0=0
                    # Parse all value changes on this line
                    i = 0
                    while i < len(line):
                        if line[i] in "01xzXZ":
                            value = line[i]
                            i += 1
                            # Next char(s) is the channel ID
                            if i < len(line):
                                vcd_id = line[i]
                                i += 1
                                if vcd_id in id_to_name:
                                    ch_name = id_to_name[vcd_id]
                                    int_val = int(value) if value in "01" else value
                                    channels[ch_name].append((current_timestamp, int_val))
                        else:
                            i += 1

        except Exception as e:
            print(f"[!] VCD parse error: {e}")
            return {"channels": {}, "metadata": {}}

        return {"channels": channels, "metadata": metadata}

    def correlate_with_esp32(self, capture: LACaptureResult, esp32_log: list[str]) -> dict:
        """
        Correlate LA capture data with ESP32 serial log.

        Input:
          capture: LACaptureResult from LA
          esp32_log: list of serial log lines from ESP32

        Output:
          Correlation timeline showing:
          - When ESP32 sent each packet
          - When LA observed bytes on wire
          - When DUT responded
          - When heartbeat disappeared
          - Timing discrepancies
        """
        correlation = {
            "capture": asdict(capture),
            "esp32_events": [],
            "la_events": [],
            "discrepancies": [],
            "timeline": [],
        }

        # Parse ESP32 log for key events
        for line in esp32_log:
            if "CAMPAIGN STARTED" in line:
                correlation["esp32_events"].append({"type": "campaign_start", "line": line})
            elif "RESP seq=" in line:
                correlation["esp32_events"].append({"type": "response", "line": line})
            elif "CRASH DETECTED" in line:
                correlation["esp32_events"].append({"type": "failure", "line": line})
            elif "Heartbeat TIMEOUT" in line:
                correlation["esp32_events"].append({"type": "heartbeat_timeout", "line": line})
            elif "Heartbeat RECOVERED" in line:
                correlation["esp32_events"].append({"type": "heartbeat_recovered", "line": line})

        # Parse VCD for key events
        if capture.vcd_path and os.path.exists(capture.vcd_path):
            vcd_data = self.parse_vcd(capture.vcd_path)
            correlation["la_events"] = [
                {"channel": ch, "events": len(events)}
                for ch, events in vcd_data.get("channels", {}).items()
            ]

        return correlation

    def generate_capture_report(self, capture: LACaptureResult, correlation: dict, output_dir: str) -> str:
        """
        Generate a capture report combining LA data with ESP32 correlation.

        Returns path to generated report.
        """
        report_dir = Path(output_dir)
        report_dir.mkdir(parents=True, exist_ok=True)

        report = {
            "capture": asdict(capture),
            "correlation": correlation,
            "analysis": {
                "uart_timing": "TODO: Implement UART bit timing analysis",
                "signal_integrity": "TODO: Implement edge timing analysis",
                "bus_health": "TODO: Implement stuck-high/low detection",
            },
            "generated_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        }

        report_path = report_dir / "la_capture_report.json"
        with open(report_path, "w") as f:
            json.dump(report, f, indent=2, default=str)

        print(f"[+] LA capture report: {report_path}")
        return str(report_path)


# ============================================
# Convenience functions
# ============================================

def quick_capture(protocol: str = "UART", duration_ms: int = 5000) -> Optional[LACaptureResult]:
    """Quick convenience function for a short capture."""
    la = LogicAnalyzerCapture()
    if not la.is_available():
        print("[!] Logic analyzer not available (sigrok-cli missing)")
        return None

    config = LACaptureConfig(
        protocol=protocol,
        capture_ms=duration_ms,
    )
    return la.start_capture(config)


if __name__ == "__main__":
    # Test the module
    la = LogicAnalyzerCapture()
    print(f"sigrok-cli available: {la.is_available()}")

    if la.is_available():
        devices = la.get_device_list()
        print(f"Devices found: {devices}")
    else:
        print("Install sigrok-cli: sudo apt install sigrok-cli")
