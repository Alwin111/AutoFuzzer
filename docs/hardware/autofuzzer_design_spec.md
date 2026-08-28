# AutoFuzzer Hardware Design Specification

![AutoFuzzer Battery and Analyzer Dot Board Layout](./autofuzzer_battery_analyzer_schematic_1787298769386.jpg)

This document provides the complete hardware specification, electrical schematic mapping, and layout instructions for the **AutoFuzzer** standalone dot board prototype. This specification serves as a direct technical guide for manual assembly (soldering) and a reference for generating schematics and PCB designs.


---

## 1. Project Concept
AutoFuzzer is a standalone hardware unit designed to test the communication robustness of external target microcontrollers (Device Under Test or DUT). It connects to targets via exposed pin headers and runs automated, mutated packet injection over UART, SPI, I2C, and CAN. It monitors the target's liveness using a physical GPIO heartbeat pin and alerts the operator via status LEDs, an OLED screen, and a buzzer if the target locks up or crashes.

---

## 2. Component Listing and Specifications

### 2.1 Main Controller
* **ESP32 NodeMCU Development Board (38-Pin)**
  * Microcontroller: ESP-WROOM-32
  * Pitch: 0.1 inch (2.54 mm) standard pin spacing
  * Pins: 19 pins on the left side, 19 pins on the right side

### 2.2 Power Supply & Battery Management
* **TP4056 Lithium Battery Charger Module**
  * Input: 5V via USB-C or micro-USB port
  * Output Pins: `OUT+` (5V boost/system power), `OUT-` (System Ground)
  * Battery Connection: `BAT+`, `BAT-`
  * Protection: Integrated over-charge, over-discharge, and over-current protection
* **Lithium Polymer (Li-Po) Battery**
  * Nominal Voltage: 3.7V
  * Capacity: 800 mAh to 1200 mAh (pouch type)

### 2.3 User Interface (UI)
* **0.96" I2C OLED Display Module**
  * Driver: SSD1306
  * Resolution: 128x64 pixels
  * Communication: I2C (4 pins: `VCC`, `GND`, `SCL`, `SDA`)
* **Tactile Push Buttons**
  * Type: 6x6mm momentary push buttons (4 pins, normally open)
  * Quantity: 4 buttons (labeled `POWER`, `FUZZ`, `UP`, `DOWN`)
* **Piezo Buzzer (LS1)**
  * Type: 5V Active Piezo Buzzer (magnetic/cylindrical)
* **Status LEDs**
  * 3x 5mm LEDs: Red (FAIL), Green (PASS), Yellow or Blue (ACTIVE)
  * 3x 220 Ohm Resistors (current-limiting for LEDs)

### 2.4 Diagnostic Interfaces
* **J1: 8-Pin Logic Analyzer Header (LA1)**
  * Type: 1x8 pin male header strip
  * Purpose: Connects an external logic analyzer in parallel to trace fuzzer outputs.
* **Target Interface Header**
  * Type: 1x12 pin female header strip
  * Purpose: Exposed connection port to run jumper wires to the target board.

---

## 3. Comprehensive Electrical Netlist (Connections)

### 3.1 Power Netlist
| Source Pin | Destination Pins | Description |
| :--- | :--- | :--- |
| **TP4056 OUT+** | ESP32 `VIN` (5V), OLED `VCC`, Target Header Pin 3 | Main Power Rail (5V VCC) |
| **TP4056 OUT-** | ESP32 `GND`, OLED `GND`, CAN `GND`, Logic Analyzer Pin 1, LED Cathodes, Button Ground Pins, Buzzer (-), Target Header Pin 1 | System Ground Rail (GND) |
| **ESP32 3V3** | Target Header Pin 2 | Regulated 3.3V out to low-power targets |
| **Li-Po Battery (+)**| TP4056 `BAT+` | Battery Positive Terminal |
| **Li-Po Battery (-)**| TP4056 `BAT-` | Battery Negative Terminal |

### 3.2 Display Netlist (I2C)
* **ESP32 GPIO 21 (SDA)** $\longleftrightarrow$ **OLED SDA** $\longleftrightarrow$ **Target Header Pin 10**
* **ESP32 GPIO 22 (SCL)** $\longleftrightarrow$ **OLED SCL** $\longleftrightarrow$ **Target Header Pin 11**

### 3.3 Control Inputs (Tactile Buttons)
All buttons connect between their respective ESP32 GPIO pin and the **GND** rail. The ESP32 utilizes internal pull-up resistors (`INPUT_PULLUP`).
* **B1 (Power/Select)**: ESP32 GPIO 12 $\longleftrightarrow$ Button B1 Terminal $\longleftrightarrow$ GND
* **B2 (Fuzz/Start)**: ESP32 GPIO 13 $\longleftrightarrow$ Button B2 Terminal $\longleftrightarrow$ GND
* **B3 (Up)**: ESP32 GPIO 14 $\longleftrightarrow$ Button B3 Terminal $\longleftrightarrow$ GND
* **B4 (Down)**: ESP32 GPIO 27 $\longleftrightarrow$ Button B4 Terminal $\longleftrightarrow$ GND

### 3.4 Alerts & Status Indicators
* **Buzzer (LS1)**: ESP32 GPIO 32 $\longleftrightarrow$ Buzzer (+) ; Buzzer (-) $\longleftrightarrow$ GND
* **Green LED (PASS)**: ESP32 GPIO 2 $\longleftrightarrow$ 220 Ohm Resistor $\longleftrightarrow$ Green LED Anode (+); Cathode (-) $\longleftrightarrow$ GND
* **Red LED (FAIL)**: ESP32 GPIO 4 $\longleftrightarrow$ 220 Ohm Resistor $\longleftrightarrow$ Red LED Anode (+); Cathode (-) $\longleftrightarrow$ GND
* **Yellow LED (ACTIVE)**: ESP32 GPIO 15 $\longleftrightarrow$ 220 Ohm Resistor $\longleftrightarrow$ Yellow LED Anode (+); Cathode (-) $\longleftrightarrow$ GND

### 3.5 Logic Analyzer (J1 / LA1) & Target Headers
The Logic Analyzer Header splits fuzzer signals in parallel before they reach the main target header.

| Logic Analyzer (J1) Pin | Associated Signal | ESP32 Connection Pin | Target Header Pin |
| :---: | :--- | :--- | :---: |
| **Pin 1** | Ground (GND) | GND | **Pin 1** |
| **Pin 2** | Target RX (Fuzzer Input) | RX2 (GPIO 16) | **Pin 5** |
| **Pin 3** | Target TX (Fuzzer Output) | TX2 (GPIO 17) | **Pin 4** |
| **Pin 4** | Target CS (SPI Chip Select) | GPIO 5 | **Pin 9** |
| **Pin 5** | Target SCK (SPI Clock) | GPIO 18 | **Pin 6** |
| **Pin 6** | Target MISO (SPI Input) | GPIO 19 | **Pin 7** |
| **Pin 7** | Target MOSI (SPI Output) | GPIO 23 | **Pin 8** |
| **Pin 8** | Heartbeat (Target Alive Input) | GPIO 25 | **Pin 12** |

---

## 4. Physical Layout & Solder Trace Routing (For Dot Board)

### 4.1 Layout Overview
1. **Left Half**:
   * ESP32 mounted vertically in the center.
   * Buttons (B1, B2, B3, B4) stacked vertically to the left of the ESP32.
   * Active Piezo Buzzer positioned below the buttons.
   * TP4056 charger module mounted flat at the bottom-left corner.
2. **Right Half**:
   * OLED display mounted on the top-right corner.
   * 3x LEDs with resistors arranged in a vertical line below the OLED.
   * J1 (8-pin Logic Analyzer header) placed vertically in the center-right.
   * Target interface female headers placed on the right edge.
3. **Bottom**:
   * Lithium pouch battery secured flat below the ESP32 board.

### 4.2 Solder Tracing Instructions
* **Ground Bus**: Run a thick bus wire (or solder bridge path) around the perimeter of the board. Connect OUT- from the TP4056 charger to this wire. All ground pins from buttons, LEDs, OLED, and buzzer connect to this common trace.
* **Power Bus (5V)**: Connect OUT+ from the TP4056 directly to the ESP32 `VIN` pin using 24 AWG wire. Route the same line to the OLED `VCC` pin.
* **Signal Lines**: Use thin wrapping wire (30 AWG) to connect ESP32 pins to the OLED, buttons, LEDs, and buzzer on the solder side. Avoid crossing signal lines directly to minimize I2C noise.
* **Logic Analyzer Parallel Tap**: Solder a direct line from the ESP32 pins to the logic analyzer pins first, then continue the trace to the corresponding target header pin. This ensures the analyzer tap is in parallel.
