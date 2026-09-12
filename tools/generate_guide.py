#!/usr/bin/env python3
"""
AutoFuzzer -- Faculty Project Guide (PDF Generator)
Generates a comprehensive PDF document covering all aspects of the project.
"""

import os
import json
from fpdf import FPDF

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGES = os.path.join(BASE, 'docs', 'images')
REPORTS = os.path.join(BASE, 'reports')


class AutoFuzzerGuide(FPDF):
    """Custom PDF class with headers/footers."""

    def __init__(self):
        super().__init__()
        self.set_auto_page_break(auto=True, margin=25)
        self.is_title_page = False

    def header(self):
        if self.is_title_page:
            return
        self.set_font('Helvetica', 'I', 8)
        self.set_text_color(120, 120, 120)
        self.cell(0, 8, 'AutoFuzzer v4.0 -- Project Guide', align='L')
        self.cell(0, 8, f'Page {self.page_no()}', align='R', new_x="LMARGIN", new_y="NEXT")
        self.set_draw_color(200, 200, 200)
        self.line(10, self.get_y(), 200, self.get_y())
        self.ln(4)

    def footer(self):
        if self.is_title_page:
            return
        self.set_y(-15)
        self.set_font('Helvetica', 'I', 7)
        self.set_text_color(150, 150, 150)
        self.cell(0, 10, 'AutoFuzzer -- Embedded Protocol Robustness Testing Platform', align='C')

    def title_page(self):
        self.is_title_page = True
        self.add_page()
        self.ln(40)

        # Title
        self.set_font('Helvetica', 'B', 36)
        self.set_text_color(0, 100, 200)
        self.cell(0, 15, 'AutoFuzzer', align='C', new_x="LMARGIN", new_y="NEXT")

        self.set_font('Helvetica', '', 18)
        self.set_text_color(80, 80, 80)
        self.cell(0, 10, 'Embedded Protocol Robustness', align='C', new_x="LMARGIN", new_y="NEXT")
        self.cell(0, 10, 'Testing Platform', align='C', new_x="LMARGIN", new_y="NEXT")

        self.ln(5)
        self.set_font('Helvetica', '', 14)
        self.set_text_color(0, 150, 100)
        self.cell(0, 10, 'Version 4.0', align='C', new_x="LMARGIN", new_y="NEXT")

        self.ln(15)

        # Subtitle
        self.set_draw_color(0, 100, 200)
        self.set_line_width(0.5)
        self.line(60, self.get_y(), 150, self.get_y())
        self.ln(15)

        self.set_font('Helvetica', '', 11)
        self.set_text_color(60, 60, 60)
        self.multi_cell(0, 7, (
            'A standalone ESP32-based hardware fuzzing device that connects to\n'
            'external microcontrollers and tests communication handler robustness\n'
            'through automated, mutated packet injection across UART, SPI, and I2C protocols.'
        ), align='C')

        self.ln(25)

        # Info box
        self.set_fill_color(240, 245, 255)
        self.set_draw_color(0, 100, 200)
        x_start = 40
        self.rect(x_start, self.get_y(), 130, 50)
        self.ln(5)
        self.set_x(x_start + 10)
        self.set_font('Helvetica', '', 10)
        self.set_text_color(60, 60, 60)
        info_lines = [
            ('Repository:', 'github.com/Alwin111/AutoFuzzer'),
            ('Platform:', 'ESP32 DevKit V1'),
            ('Protocols:', 'UART, SPI, I2C'),
            ('Targets:', 'STM32 Nucleo, Arduino Nano, ESP32'),
            ('Tests Run:', '41 campaigns across 3 boards'),
            ('Test Reports:', '41 reports (28 pass, 13 fail)'),
        ]
        for label, value in info_lines:
            self.set_x(x_start + 10)
            self.set_font('Helvetica', 'B', 10)
            self.cell(35, 7, label)
            self.set_font('Helvetica', '', 10)
            self.cell(0, 7, value, new_x="LMARGIN", new_y="NEXT")
            self.set_x(x_start + 10)

        self.is_title_page = False

    def section_title(self, title, num=None):
        self.add_page()
        self.ln(5)
        self.set_font('Helvetica', 'B', 22)
        self.set_text_color(0, 80, 180)
        prefix = f'{num}. ' if num else ''
        self.cell(0, 12, f'{prefix}{title}', new_x="LMARGIN", new_y="NEXT")
        self.set_draw_color(0, 100, 200)
        self.set_line_width(0.8)
        self.line(10, self.get_y(), 100, self.get_y())
        self.ln(8)

    def sub_title(self, title):
        self.ln(3)
        self.set_font('Helvetica', 'B', 13)
        self.set_text_color(40, 40, 40)
        self.cell(0, 8, title, new_x="LMARGIN", new_y="NEXT")
        self.ln(2)

    def body_text(self, text):
        self.set_font('Helvetica', '', 10)
        self.set_text_color(50, 50, 50)
        self.multi_cell(0, 5.5, text)
        self.ln(2)

    def bullet(self, text, indent=5):
        self.set_font('Helvetica', '', 10)
        self.set_text_color(50, 50, 50)
        self.set_x(self.l_margin + indent)
        self.multi_cell(0, 5.5, '-  ' + text)

    def code_block(self, text):
        self.set_font('Courier', '', 9)
        self.set_text_color(30, 30, 30)
        self.set_fill_color(245, 245, 250)
        self.set_draw_color(200, 200, 210)
        x = self.get_x() + 5
        y = self.get_y()
        lines = text.strip().split('\n')
        h = len(lines) * 4.5 + 6
        self.rect(x - 3, y, 185, h, 'DF')
        self.ln(3)
        for line in lines:
            self.set_x(x)
            self.cell(0, 4.5, line, new_x="LMARGIN", new_y="NEXT")
        self.ln(4)

    def table(self, headers, rows, col_widths=None):
        if not col_widths:
            n = len(headers)
            col_widths = [185 / n] * n

        # Header
        self.set_font('Helvetica', 'B', 9)
        self.set_fill_color(0, 80, 180)
        self.set_text_color(255, 255, 255)
        for i, h in enumerate(headers):
            self.cell(col_widths[i], 7, h, border=1, fill=True, align='C')
        self.ln()

        # Rows
        self.set_font('Helvetica', '', 9)
        self.set_text_color(40, 40, 40)
        fill = False
        for row in rows:
            if self.get_y() > 265:
                self.add_page()
                # Re-draw header
                self.set_font('Helvetica', 'B', 9)
                self.set_fill_color(0, 80, 180)
                self.set_text_color(255, 255, 255)
                for i, h in enumerate(headers):
                    self.cell(col_widths[i], 7, h, border=1, fill=True, align='C')
                self.ln()
                self.set_font('Helvetica', '', 9)
                self.set_text_color(40, 40, 40)

            if fill:
                self.set_fill_color(240, 245, 255)
            else:
                self.set_fill_color(255, 255, 255)
            for i, val in enumerate(row):
                align = 'C' if i > 0 else 'L'
                self.cell(col_widths[i], 6.5, str(val), border=1, fill=True, align=align)
            self.ln()
            fill = not fill
        self.ln(3)

    def add_image_full(self, path, caption=None, max_w=170):
        if not os.path.exists(path):
            self.body_text(f'[Image not found: {path}]')
            return
        img_w = min(max_w, 185)
        self.image(path, x=12, w=img_w)
        if caption:
            self.set_font('Helvetica', 'I', 8)
            self.set_text_color(100, 100, 100)
            self.cell(0, 5, caption, align='C', new_x="LMARGIN", new_y="NEXT")
        self.ln(5)

    def highlight_box(self, title, text, color=(0, 100, 200)):
        self.set_fill_color(color[0], color[1], color[2])
        self.set_text_color(255, 255, 255)
        self.set_font('Helvetica', 'B', 10)
        self.cell(185, 7, f'  {title}', fill=True, new_x="LMARGIN", new_y="NEXT")
        self.set_fill_color(240, 245, 255)
        self.set_text_color(40, 40, 40)
        self.set_font('Helvetica', '', 9)
        x = self.get_x()
        self.set_x(x + 3)
        self.multi_cell(179, 5.5, text, fill=True)
        self.ln(4)


def generate_guide():
    pdf = AutoFuzzerGuide()

    # ================================================================
    # TITLE PAGE
    # ================================================================
    pdf.title_page()

    # ================================================================
    # TABLE OF CONTENTS
    # ================================================================
    pdf.add_page()
    pdf.set_font('Helvetica', 'B', 20)
    pdf.set_text_color(0, 80, 180)
    pdf.cell(0, 12, 'Table of Contents', new_x="LMARGIN", new_y="NEXT")
    pdf.set_draw_color(0, 100, 200)
    pdf.set_line_width(0.8)
    pdf.line(10, pdf.get_y(), 80, pdf.get_y())
    pdf.ln(8)

    toc = [
        '1.  Project Overview & Motivation',
        '2.  System Architecture',
        '3.  Hardware Design',
        '4.  Circuit Diagram & Pin Map',
        '5.  Firmware Architecture',
        '6.  Communication Protocol',
        '7.  Mutation Engine & Test Profiles',
        '8.  Failure Detection & Reproduction Pipeline',
        '9.  OLED User Interface',
        '10. Web Dashboard',
        '11. PC Companion Tool',
        '12. Test Results & Reports',
        '13. Unit Testing',
        '14. Issues Faced & Solutions',
        '15. Future Work',
    ]
    pdf.set_font('Helvetica', '', 11)
    pdf.set_text_color(50, 50, 50)
    for item in toc:
        pdf.cell(0, 7, item, new_x="LMARGIN", new_y="NEXT")

    # ================================================================
    # 1. PROJECT OVERVIEW
    # ================================================================
    pdf.section_title('Project Overview & Motivation', 1)

    pdf.sub_title('What is AutoFuzzer?')
    pdf.body_text(
        'AutoFuzzer is a standalone embedded protocol robustness testing platform. '
        'It uses an ESP32 microcontroller as the primary fuzzer/controller that connects to '
        'an external Device Under Test (DUT) -- such as an STM32 Nucleo, Arduino Nano, or '
        'another ESP32 -- to systematically test communication handler robustness through '
        'automated, mutated packet injection.'
    )
    pdf.body_text(
        'The system runs entirely autonomously on the ESP32. No PC is required for basic '
        'testing. An optional PC companion provides advanced features like report generation, '
        'AI-assisted debugging, and historical failure tracking.'
    )

    pdf.sub_title('Motivation')
    pdf.body_text(
        'Embedded firmware vulnerabilities are a growing security concern. Communication '
        'parsers -- the code that processes incoming UART, SPI, and I2C data -- are among the '
        'most attack-sensitive components. A malformed packet can trigger buffer overflows, '
        'infinite loops, watchdog resets, or complete system lockups.'
    )
    pdf.body_text(
        'Manual testing of these edge cases is time-consuming and error-prone. AutoFuzzer '
        'automates this process: it generates valid, boundary, malformed, corrupted, '
        'truncated, and randomized inputs, sends them to the target, monitors the target\'s '
        'health, and systematically discovers robustness weaknesses.'
    )

    pdf.sub_title('Goals')
    goals = [
        'Connect to a target MCU and test communication protocol robustness',
        'Generate structured test campaigns with prioritized mutation phases',
        'Detect failures through heartbeat monitoring and response parsing',
        'Freeze, replay, reproduce, and minimize failing testcases',
        'Generate detailed reports with robustness scores',
        'Provide a visual web dashboard for real-time monitoring',
        'Optionally analyze failures with AI and propose firmware fixes',
        'Create permanent regression testcases from discovered failures',
    ]
    for g in goals:
        pdf.bullet(g)

    # ================================================================
    # 2. SYSTEM ARCHITECTURE
    # ================================================================
    pdf.section_title('System Architecture', 2)

    pdf.sub_title('Three-Layer Architecture')
    pdf.body_text(
        'AutoFuzzer uses a three-layer architecture with clear separation of concerns:'
    )

    pdf.code_block(
        '                    +-------------------------+\n'
        '                    |     PC / Laptop         |\n'
        '                    |  (Optional)             |\n'
        '                    |  Reports, AI, LA, Build |\n'
        '                    +-----------+-------------+\n'
        '                                | USB Serial\n'
        '                    +-----------v-------------+\n'
        '                    |        ESP32             |\n'
        '                    |  Fuzzing Engine          |\n'
        '                    |  Protocol Engine         |\n'
        '                    |  Heartbeat Monitor       |\n'
        '                    |  Failure Detection       |\n'
        '                    |  Replay / Minimize       |\n'
        '                    |  OLED UI / Buttons       |\n'
        '                    +-----------+-------------+\n'
        '                                | Protocol\n'
        '                    +-----------v-------------+\n'
        '                    |    Target MCU (DUT)      |\n'
        '                    |  Firmware Under Test     |\n'
        '                    +-------------------------+'
    )

    pdf.sub_title('Key Design Principle')
    pdf.highlight_box(
        'STANDALONE OPERATION',
        'The ESP32 performs all fuzzing, monitoring, failure detection, replay, and '
        'minimization independently. The PC companion is optional -- it adds report '
        'generation, AI analysis, and historical tracking, but is never required.',
        (0, 150, 80)
    )

    # ================================================================
    # 3. HARDWARE DESIGN
    # ================================================================
    pdf.section_title('Hardware Design', 3)

    pdf.sub_title('AutoFuzzer v4.0 Prototype')
    pdf.body_text(
        'The AutoFuzzer v4.0 prototype is built on a perfboard with the following components:'
    )

    components = [
        ('ESP32 DevKit V1', 'Main controller -- runs all fuzzing logic'),
        ('SSD1306 OLED (128x64)', 'User interface -- menus, status, results'),
        ('3x LEDs (Green/Red/Blue)', 'PASS / FAIL / ACTIVE indicators'),
        ('4x Tactile Buttons', 'UP / DOWN / SELECT / BACK navigation'),
        ('Piezo Buzzer', 'Audio alerts for failures'),
        ('3.7V LiPo Battery', 'Portable power source'),
        ('TP4056 Module', 'Battery charging protection'),
        ('Logic Analyzer Header', '8-channel Saleae-compatible probe connection'),
        ('Male Header Pins', 'Exposed connections for DUT wiring'),
    ]
    for name, desc in components:
        pdf.bullet(f'{name} -- {desc}')

    pdf.ln(3)
    pdf.add_image_full(
        os.path.join(IMAGES, 'hardware-build.png'),
        'AutoFuzzer v4.0 prototype -- ESP32, OLED, LEDs, buttons, buzzer, battery on perfboard',
        max_w=120
    )

    # ================================================================
    # 4. CIRCUIT DIAGRAM & PIN MAP
    # ================================================================
    pdf.section_title('Circuit Diagram & Pin Map', 4)

    pdf.add_image_full(
        os.path.join(IMAGES, 'circuit-diagram.png'),
        'Full wiring schematic -- ESP32, OLED, LEDs, buttons, buzzer, battery, LA header',
        max_w=140
    )

    pdf.sub_title('ESP32 Pin Map')
    pdf.table(
        ['GPIO', 'Function', 'Module'],
        [
            ['2', 'Green LED (PASS)', 'indicators'],
            ['4', 'Red LED (FAIL)', 'indicators'],
            ['5', 'SPI CS', 'spi_fuzzer'],
            ['12', 'Button: UP', 'buttons'],
            ['13', 'Button: DOWN', 'buttons'],
            ['14', 'Button: BACK', 'buttons'],
            ['15', 'Blue LED (ACTIVE)', 'indicators'],
            ['16', 'UART RX2', 'uart_fuzzer / uart_parser'],
            ['17', 'UART TX2', 'uart_fuzzer'],
            ['18', 'SPI SCK', 'spi_fuzzer'],
            ['19', 'SPI MISO', 'spi_fuzzer'],
            ['21', 'I2C SDA / OLED', 'i2c_fuzzer / oled_ui'],
            ['22', 'I2C SCL / OLED', 'i2c_fuzzer / oled_ui'],
            ['23', 'SPI MOSI', 'spi_fuzzer'],
            ['25', 'Heartbeat Input', 'heartbeat'],
            ['27', 'Button: SELECT', 'buttons'],
            ['32', 'Piezo Buzzer', 'indicators'],
        ],
        [20, 70, 95]
    )

    pdf.sub_title('Wiring: ESP32 to Arduino Nano (UART)')
    pdf.code_block(
        'ESP32 GPIO 17 (TX)  -->  Nano D3 (RX - SoftwareSerial)\n'
        'ESP32 GPIO 16 (RX)  <--  Nano D2 (TX - SoftwareSerial)\n'
        'ESP32 GPIO 25       <--  Nano D4 (Heartbeat)\n'
        'ESP32 GND           -->  Nano GND'
    )

    pdf.sub_title('Wiring: ESP32 to STM32 Nucleo (UART)')
    pdf.code_block(
        'ESP32 GPIO 17 (TX)  -->  STM32 PA10 (USART1_RX)\n'
        'ESP32 GPIO 16 (RX)  <--  STM32 PA9  (USART1_TX)\n'
        'ESP32 GPIO 25       <--  STM32 PB5  (Heartbeat)\n'
        'ESP32 GND           -->  STM32 GND'
    )

    # ================================================================
    # 5. FIRMWARE ARCHITECTURE
    # ================================================================
    pdf.section_title('Firmware Architecture', 5)

    pdf.body_text(
        'The ESP32 firmware is organized into 5 logical modules with 16+ source files:'
    )

    pdf.table(
        ['Module', 'Files', 'Responsibility'],
        [
            ['Core', 'types.h, prng, campaign,\ntestcase, failure, result', 'PRNG, campaigns, scoring, metadata'],
            ['Protocols', 'uart_fuzzer, uart_parser,\nspi_fuzzer, i2c_fuzzer', 'Packet generation, response parsing'],
            ['Monitoring', 'heartbeat, crash_detector', 'Health monitoring, failure classification'],
            ['UI', 'oled_ui, buttons, indicators', 'OLED menus, debounced buttons, LEDs'],
            ['Storage', 'failure_store', 'In-memory failure records, JSON export'],
        ],
        [30, 60, 95]
    )

    pdf.sub_title('Module Descriptions')
    pdf.bullet('Core -- Xorshift32 PRNG with state snapshots, campaign manager with phase-based scheduling, testcase metadata capture, 10-type failure classification, robustness scoring')
    pdf.bullet('Protocols -- UART fuzzer with 10 mutation types, UART response parser (ACK/NACK), SPI fuzzer with 7 distinct behaviors, I2C fuzzer with OLED auto-sleep')
    pdf.bullet('Monitoring -- Edge-based heartbeat with debounce, crash detector with evidence-based classification')
    pdf.bullet('UI -- 15-screen state machine OLED menu, 4-button debounced navigation, 3-LED status, buzzer alerts')
    pdf.bullet('Storage -- In-memory failure store (20 records), JSON export via serial command')

    # ================================================================
    # 6. COMMUNICATION PROTOCOL
    # ================================================================
    pdf.section_title('Communication Protocol', 6)

    pdf.sub_title('Packet Format')
    pdf.code_block(
        'SYNC | CMD | LENGTH | SEQ_LO | SEQ_HI | PAYLOAD[0..n-1] | XOR_CHECK\n'
        ' 0xA5  0x01    n        ..        ..            ..              ..\n\n'
        'XOR_CHECK = CMD ^ LENGTH ^ SEQ_LO ^ SEQ_HI ^ PAYLOAD[0] ^ ... ^ PAYLOAD[n-1]\n'
        'Maximum valid payload: 32 bytes'
    )

    pdf.sub_title('DUT Response Format')
    pdf.code_block(
        'ACK_SYNC | SEQ_LO | SEQ_HI | STATUS\n'
        '  0x5A      ..        ..       ..\n\n'
        'Status 0x00 = ACK (packet accepted)\n'
        'Status 0x02 = NACK (overlength)\n'
        'Status 0x03 = NACK (checksum error)\n'
        'No response = Parser timeout'
    )

    pdf.sub_title('Heartbeat Protocol')
    pdf.body_text(
        'The DUT continuously toggles a GPIO pin at approximately 100ms intervals. '
        'The ESP32 monitors this signal on GPIO 25. If no edge is detected for more '
        'than 350ms, a heartbeat timeout is registered as a suspected failure. The system '
        'then freezes the campaign and captures all state for reproduction.'
    )

    # ================================================================
    # 7. MUTATION ENGINE & TEST PROFILES
    # ================================================================
    pdf.section_title('Mutation Engine & Test Profiles', 7)

    pdf.sub_title('Mutation Types (10 Total)')
    pdf.table(
        ['#', 'Mutation', 'Payload', 'Behavior'],
        [
            ['0', 'VALID', '8 bytes', 'Normal valid packet -- baseline'],
            ['1', 'EMPTY', '0 bytes', 'Zero-length payload'],
            ['2', 'MAX_LEN', '32 bytes', 'Maximum valid payload'],
            ['3', 'OVERLENGTH', '45 bytes', 'Declares >32 byte length'],
            ['4', 'BAD_CRC', '8 bytes', 'Inverted checksum byte'],
            ['5', 'TRUNCATED', '12->6 bytes', 'Half the bytes transmitted'],
            ['6', 'RANDOM', '1-32 bytes', 'PRNG-selected payload'],
            ['7', 'BAD_HEADER', '8 bytes', 'Corrupted sync byte (0x00)'],
            ['8', 'BAD_LEN', '8 bytes', 'Length field set to 0xFF'],
            ['9', 'SEQ_JUMP', '8 bytes', 'Sequence number skips ahead'],
        ],
        [10, 30, 28, 117]
    )

    pdf.sub_title('Test Profiles')
    pdf.table(
        ['Profile', 'Duration', 'Phase Distribution'],
        [
            ['QUICK', '30 seconds', '5s baseline, 5s boundary, 5s overlength, 5s checksum, 5s malformed, 5s random'],
            ['STANDARD', '60 seconds', 'Broader distribution across all 10 mutation types with response correlation'],
            ['DEEP', '5 minutes', 'All mutations, adaptive scheduling, repeated edge cases, recovery testing'],
        ],
        [30, 25, 130]
    )

    pdf.sub_title('Adaptive Mutation Scheduler')
    pdf.body_text(
        'The campaign manager tracks per-mutation statistics (executions, ACKs, NACKs, '
        'timeouts, heartbeat failures) and dynamically adjusts selection probability. '
        'Mutations that produce interesting behavior (NACKs, timeouts) receive higher '
        'weight. A 10% exploration rate prevents starvation of any single mutation type.'
    )

    # ================================================================
    # 8. FAILURE DETECTION & REPRODUCTION
    # ================================================================
    pdf.section_title('Failure Detection & Reproduction Pipeline', 7)

    pdf.highlight_box(
        'CORE ENGINEERING PRINCIPLE',
        'FIND -> FREEZE -> RECORD -> REPLAY -> REPRODUCE -> MINIMIZE -> REPORT -> '
        'ANALYZE -> FIX -> VERIFY -> REGRESSION',
        (200, 60, 0)
    )

    pdf.sub_title('Failure Classification (10 Types)')
    failures = [
        'COMMUNICATION_FAILURE -- Serial link failure',
        'NO_RESPONSE -- DUT did not respond',
        'HEARTBEAT_TIMEOUT -- Heartbeat signal lost (>350ms)',
        'PARSER_TIMEOUT -- Response not received in time',
        'PROTOCOL_ERROR -- Unexpected response format',
        'WATCHDOG_RESET -- DUT watchdog fired',
        'HARDFAULT -- CPU hard fault detected',
        'BUS_ERROR -- I2C/SPI bus error',
        'POWER_FAILURE -- DUT power loss',
        'UNKNOWN_FAILURE -- Unclassified failure',
    ]
    for f in failures:
        pdf.bullet(f)

    pdf.sub_title('Reproduction Pipeline')
    steps = [
        'FAILURE DETECTED -- Campaign stops immediately, all state frozen',
        'CAPTURE -- Protocol, mutation, packet bytes, PRNG seed, sequence, timestamps',
        'REPLAY -- Send exact same packet 3 times (configurable)',
        'CLASSIFY -- 3/3 FAIL = REPRODUCIBLE, 2/3 = INTERMITTENT, 0/3 = NOT REPRODUCED',
        'MINIMIZE -- Binary search for smallest input that still reproduces the failure',
        'REPORT -- Full failure report with metadata, packet dump, reproduction steps',
        'STORE -- Permanent regression testcase in failure library',
    ]
    for s in steps:
        pdf.bullet(s)

    # ================================================================
    # 9. OLED USER INTERFACE
    # ================================================================
    pdf.section_title('OLED User Interface', 8)

    pdf.body_text(
        'The OLED menu system is a 15-screen state machine with debounced button navigation:'
    )

    pdf.code_block(
        'AUTOFUZZER v4.0\n'
        '----------------\n'
        '> NEW TEST\n'
        '  RESULTS\n'
        '  FAILURES\n'
        '  SETTINGS\n'
        '  ABOUT\n'
        '        |\n'
        'SELECT TARGET BOARD\n'
        '--------------------\n'
        '> STM32 NUCLEO\n'
        '  ESP32\n'
        '  ARDUINO NANO\n'
        '        |\n'
        'SELECT PROTOCOL\n'
        '--------------------\n'
        '> UART\n'
        '  SPI\n'
        '  I2C\n'
        '        |\n'
        'WIRING GUIDE (board-specific)\n'
        '        |\n'
        'SELECT TEST PROFILE\n'
        '--------------------\n'
        '> QUICK 30 SEC\n'
        '  STANDARD 1 MIN\n'
        '  DEEP 5 MIN\n'
        '        |\n'
        'START TEST?\n'
        '--------------------\n'
        '> YES   NO'
    )

    pdf.sub_title('Live Fuzzing Display')
    pdf.code_block(
        'UART FUZZING\n'
        '----------------\n'
        'Time:  18/30s\n'
        'Pkts:  173\n'
        'A:15 N:3\n'
        'Mutation: BAD_CRC\n'
        'DUT: ALIVE'
    )

    # ================================================================
    # 10. WEB DASHBOARD
    # ================================================================
    pdf.section_title('Web Dashboard', 9)

    pdf.body_text(
        'A browser-based real-time monitoring dashboard connects to the ESP32 via '
        'USB serial and displays live test data using WebSocket (SocketIO).'
    )

    pdf.sub_title('Launch')
    pdf.code_block(
        'cd tools/web_dashboard\n'
        'python3 server.py              # auto-detect ESP32 port\n'
        'python3 server.py --port /dev/ttyUSB0  # specify port\n'
        '# Then open http://localhost:5000'
    )

    pdf.sub_title('Dashboard Panels')
    pdf.table(
        ['Panel', 'What It Shows'],
        [
            ['DUT Status', 'Heartbeat OK/LOST with pulse animation'],
            ['Campaign', 'State, protocol, profile, phase, mutation'],
            ['ACK/NACK Gauges', 'Ring gauges for response distribution'],
            ['Counters', 'Packets, ACKs, NACKs, Failures, Mutations'],
            ['Packet Timeline', 'Live canvas chart over time'],
            ['Mutation Bars', 'Bar chart of mutation distribution'],
            ['Serial Monitor', 'Color-coded log with timestamps'],
            ['Quick Commands', 'One-click: START, STOP, RESET, UART, SPI, I2C'],
            ['Verdict Banner', 'PASS (green) / FAIL (red) / WAITING (blue)'],
        ],
        [40, 145]
    )

    pdf.sub_title('Technology Stack')
    pdf.bullet('Backend: Python Flask + SocketIO (threading mode)')
    pdf.bullet('Frontend: Single HTML file, zero JavaScript dependencies')
    pdf.bullet('Charts: Native Canvas API (no Chart.js or D3)')
    pdf.bullet('Communication: WebSocket for real-time serial streaming')
    pdf.bullet('Serial: pyserial with background reader thread')

    # ================================================================
    # 11. PC COMPANION
    # ================================================================
    pdf.section_title('PC Companion Tool', 10)

    pdf.body_text(
        'An optional Python CLI tool that connects to the ESP32 via USB serial '
        'for advanced operations.'
    )

    pdf.sub_title('Modules')
    pdf.table(
        ['Module', 'Purpose'],
        [
            ['connection.py', 'USB serial -- auto-detect port, send commands'],
            ['reporter.py', 'Generate JSON, HTML, plain text, hex dump reports'],
            ['database.py', 'SQLite-backed historical failure storage'],
            ['builder.py', 'PlatformIO build/flash/clean for all targets'],
            ['fixer.py', 'AI-assisted debugging -- root cause, patch suggestions'],
            ['regression.py', 'Replay known failures, detect regressions'],
            ['logic_analyzer.py', 'sigrok-cli integration, VCD parsing, correlation'],
            ['cli.py', 'Interactive CLI with all commands'],
        ],
        [35, 150]
    )

    pdf.sub_title('Usage')
    pdf.code_block(
        'python -m tools.pc_companion.cli connect      # Connect to ESP32\n'
        'python -m tools.pc_companion.cli export        # Export failures\n'
        'python -m tools.pc_companion.cli report        # Generate reports\n'
        'python -m tools.pc_companion.cli history       # View history\n'
        'python -m tools.pc_companion.cli regress       # Regression tests\n'
        'python -m tools.pc_companion.cli fix           # AI analysis\n'
        'python -m tools.pc_companion.cli build stm32   # Build firmware\n'
        'python -m tools.pc_companion.cli flash nano    # Flash target'
    )

    # ================================================================
    # 12. TEST RESULTS
    # ================================================================
    pdf.section_title('Test Results & Reports', 11)

    pdf.add_image_full(
        os.path.join(IMAGES, 'test-result-uart.png'),
        'UART quick test -- serial monitor output showing phase progression and results',
        max_w=130
    )

    pdf.sub_title('Live Test Results')
    pdf.table(
        ['Board', 'Protocol', 'Duration', 'Packets', 'Verdict', 'Score'],
        [
            ['Arduino Nano', 'UART', '30s', '600', 'PASS', '94/100'],
            ['ESP32 Loopback', 'UART', '30s', '600', 'PASS', '92/100'],
            ['STM32 Nucleo', 'UART', '30s', '19', 'FAIL*', '45/100'],
        ],
        [30, 22, 22, 25, 25, 25]
    )
    pdf.set_font('Helvetica', 'I', 8)
    pdf.set_text_color(100, 100, 100)
    pdf.cell(0, 5, '* STM32 heartbeat failure due to MCU identification issue (hardware)', new_x="LMARGIN", new_y="NEXT")
    pdf.ln(3)

    pdf.sub_title('Historical Test Report Summary')
    pdf.table(
        ['Board', 'Total Tests', 'PASS', 'FAIL'],
        [
            ['STM32 Nucleo-F446RE', '15', '13', '2'],
            ['Arduino Nano (CH340)', '18', '13', '5'],
            ['ESP32 DevKit V1', '8', '2', '6'],
            ['TOTAL', '41', '28', '13'],
        ],
        [55, 35, 35, 35]
    )

    pdf.add_image_full(
        os.path.join(IMAGES, 'test-stm32-nucleo.png'),
        'ESP32 connected to STM32 Nucleo-F446RE for UART fuzzing',
        max_w=120
    )

    pdf.add_image_full(
        os.path.join(IMAGES, 'test-arduino-nano.png'),
        'ESP32 connected to Arduino Nano for UART fuzzing via SoftwareSerial',
        max_w=120
    )

    # ================================================================
    # 13. UNIT TESTING
    # ================================================================
    pdf.section_title('Unit Testing', 12)

    pdf.body_text(
        'The project includes 45 host-side unit tests covering all core modules:'
    )

    pdf.table(
        ['Test Suite', 'Tests', 'What Is Tested'],
        [
            ['TestPRNG', '6', 'Determinism, state snapshots, ranges'],
            ['TestChecksum', '3', 'XOR calculation, empty payload, bad CRC'],
            ['TestPacketGeneration', '8', 'All 7 mutation types + deterministic replay'],
            ['TestMutationSelection', '5', 'Phase weight validation'],
            ['TestResponseParser', '6', 'ACK, NACK, sync detection, 16-bit seq'],
            ['TestFailureClassification', '6', 'Timeout, watchdog, power failure types'],
            ['TestScoring', '6', 'Score calculation, weighted average, verdict'],
            ['TestDatabase', '3', 'Save/load sessions, query, empty DB'],
            ['TestEndToEnd', '2', 'Full packet cycle + reproduction cycle'],
        ],
        [40, 20, 125]
    )

    pdf.sub_title('Running Tests')
    pdf.code_block(
        'python -m pytest tests/ -v                           # All 45 tests\n'
        'python -m pytest tests/test_core.py::TestPRNG -v     # PRNG tests\n'
        'python -m pytest tests/test_core.py::TestChecksum -v # Checksum tests'
    )

    # ================================================================
    # 14. ISSUES & SOLUTIONS
    # ================================================================
    pdf.section_title('Issues Faced & Solutions', 13)

    pdf.table(
        ['Issue', 'Cause', 'Solution'],
        [
            ['Nano not flashing', 'CH340 auto-reset broken on clone', 'HUPCL DTR toggle via Python'],
            ['0 ACKs/NACKs from Nano', 'SoftwareSerial unreliable at 115200', 'Reduced to 9600 baud'],
            ['Nano D0/D1 conflict', 'D0/D1 shared with CH340 USB', 'Switched to D2/D3 (SoftwareSerial)'],
            ['Heartbeat spam', 'Floating GPIO25 toggling rapidly', 'Edge debounce + timeout suppression'],
            ['STM32 FAIL.TXT', 'ST-Link cannot identify MCU', 'Hardware issue -- MCU defective/locked'],
            ['OLED freeze after I2C', 'Wire.end() then display.block()', 'OLED sleeps during I2C, wakes properly'],
            ['Buttons dead after test', 'I2C bus stuck holding SDA low', 'Non-blocking OLED wake via state machine'],
        ],
        [40, 50, 95]
    )

    # ================================================================
    # 15. FUTURE WORK
    # ================================================================
    pdf.section_title('Future Work', 14)

    pdf.sub_title('Phase 5-6 (Planned)')
    pdf.bullet('AI source-code analysis with proposed firmware patches (requires human approval)')
    pdf.bullet('Build/flash/verify workflow for automatic fix verification')
    pdf.bullet('Logic analyzer integration for wire-level verification and failure forensics')
    pdf.bullet('Protocol-aware SPI and I2C fuzzing with response parsing')
    pdf.bullet('Custom test duration input via OLED buttons')

    pdf.sub_title('Phase 7 (Future)')
    pdf.bullet('CAN bus fuzzing (requires external CAN transceiver hardware)')
    pdf.bullet('True reinforcement learning architecture (beyond adaptive scheduling)')
    pdf.bullet('Persistent failure storage on SD card')
    pdf.bullet('PC companion GUI (tkinter/PyQt) for visual analysis')
    pdf.bullet('Regression test suite with historical database')
    pdf.bullet('Custom protocol profiles (configurable baud, parity, framing)')

    pdf.sub_title('Research Directions')
    pdf.bullet('Cross-protocol fuzzing (simultaneous UART + SPI + I2C)')
    pdf.bullet('Timing side-channel detection via logic analyzer')
    pdf.bullet('Automated firmware diffing before/after patches')
    pdf.bullet('Integration with existing fuzzing corpora (e.g., Fuzzbuzz, ClusterFuzz)')

    # ================================================================
    # SAVE
    # ================================================================
    output_path = os.path.join(BASE, 'docs', 'AutoFuzzer_Project_Guide.pdf')
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    pdf.output(output_path)
    print(f'PDF generated: {output_path}')
    print(f'Pages: {pdf.pages_count}')
    return output_path


if __name__ == '__main__':
    generate_guide()
