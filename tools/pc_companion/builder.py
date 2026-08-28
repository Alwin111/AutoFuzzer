"""
AutoFuzzer PC Companion — Builder

Handles firmware build, flash, and verification
for the DUT (STM32) and optionally the ESP32.

Uses PlatformIO CLI under the hood.
"""

import subprocess
import time
import os
from pathlib import Path
from typing import Optional


class FirmwareBuilder:
    """Build and flash firmware via PlatformIO."""

    def __init__(self, project_root: Optional[str] = None):
        if project_root:
            self.project_root = Path(project_root)
        else:
            # Assume we're run from the repo root
            self.project_root = Path(__file__).parent.parent.parent

        self.esp32_dir = self.project_root / "firmware" / "esp32"
        self.stm32_dir = self.project_root / "firmware" / "stm32"

    def _run_pio(self, work_dir: Path, args: list[str], timeout: int = 120) -> tuple[bool, str]:
        """Run a PlatformIO command."""
        cmd = ["pio"] + args
        try:
            result = subprocess.run(
                cmd,
                cwd=str(work_dir),
                capture_output=True,
                text=True,
                timeout=timeout,
            )
            output = result.stdout + "\n" + result.stderr
            success = result.returncode == 0
            return success, output
        except FileNotFoundError:
            return False, "[!] PlatformIO (pio) not found. Install: pip install platformio"
        except subprocess.TimeoutExpired:
            return False, f"[!] Command timed out after {timeout}s"

    def build_stm32(self) -> tuple[bool, str]:
        """Build STM32 DUT firmware."""
        print("[*] Building STM32 firmware...")
        success, output = self._run_pio(self.stm32_dir, ["run"])
        if success:
            print("[+] STM32 build: SUCCESS")
        else:
            print("[-] STM32 build: FAILED")
            print(output[-500:] if len(output) > 500 else output)
        return success, output

    def build_esp32(self) -> tuple[bool, str]:
        """Build ESP32 fuzzer firmware."""
        print("[*] Building ESP32 firmware...")
        success, output = self._run_pio(self.esp32_dir, ["run"])
        if success:
            print("[+] ESP32 build: SUCCESS")
        else:
            print("[-] ESP32 build: FAILED")
            print(output[-500:] if len(output) > 500 else output)
        return success, output

    def flash_stm32(self, port: Optional[str] = None) -> tuple[bool, str]:
        """Flash STM32 DUT firmware."""
        print("[*] Flashing STM32 firmware...")
        args = ["run", "--target", "upload"]
        if port:
            args.extend(["--upload-port", port])
        success, output = self._run_pio(self.stm32_dir, args, timeout=60)
        if success:
            print("[+] STM32 flash: SUCCESS")
        else:
            print("[-] STM32 flash: FAILED")
        return success, output

    def flash_esp32(self, port: Optional[str] = None) -> tuple[bool, str]:
        """Flash ESP32 fuzzer firmware."""
        print("[*] Flashing ESP32 firmware...")
        args = ["run", "--target", "upload"]
        if port:
            args.extend(["--upload-port", port])
        success, output = self._run_pio(self.esp32_dir, args, timeout=60)
        if success:
            print("[+] ESP32 flash: SUCCESS")
        else:
            print("[-] ESP32 flash: FAILED")
        return success, output

    def clean_stm32(self) -> tuple[bool, str]:
        """Clean STM32 build."""
        return self._run_pio(self.stm32_dir, ["run", "--target", "clean"])

    def clean_esp32(self) -> tuple[bool, str]:
        """Clean ESP32 build."""
        return self._run_pio(self.esp32_dir, ["run", "--target", "clean"])

    def build_and_flash_dut(self, port: Optional[str] = None) -> bool:
        """Build and flash the DUT (STM32) firmware.

        Returns True if both build and flash succeeded.
        """
        success, _ = self.build_stm32()
        if not success:
            return False

        success, _ = self.flash_stm32(port)
        return success

    def build_and_flash_fuzzer(self, port: Optional[str] = None) -> bool:
        """Build and flash the fuzzer (ESP32) firmware."""
        success, _ = self.build_esp32()
        if not success:
            return False

        success, _ = self.flash_esp32(port)
        return success

    def verify_build(self, target: str = "stm32") -> bool:
        """Verify that a firmware target builds successfully.

        Returns True if build succeeds without flashing.
        """
        if target == "stm32":
            success, _ = self.build_stm32()
        elif target == "esp32":
            success, _ = self.build_esp32()
        else:
            print(f"[!] Unknown target: {target}")
            return False

        return success
