#include <Arduino.h>

// ============================================
// AutoFuzzer DUT — Nucleo F446RE (STM32F446RE)
// ============================================
// Heartbeat output: PB5 (documented pin) + mirrored on on-board LED (PA5)
//   PB5 -> ESP32 GPIO26 via jumper, common GND required.
// UART protocol to ESP32: Serial1 (PA9=TX, PA10=RX) @ 9600 baud
//   (matches the Nano DUT firmware baud and the ESP32 UART profile)
// Protocol: 0xA5 SYNC packets; responses 0x5A SEQ_LO SEQ_HI STATUS
// ============================================

constexpr uint32_t HEARTBEAT_MS = 100;
constexpr uint32_t UART_BAUD    = 9600;
constexpr uint8_t  HB_PIN       = PB5;   // documented heartbeat output
constexpr uint8_t  LED_PIN      = LED_BUILTIN; // PA5 mirror

static uint32_t lastHeartbeat = 0;

// ---- Protocol constants (must match ESP32) ----
constexpr uint8_t  PKT_SYNC      = 0xA5;
constexpr uint8_t  RSP_SYNC      = 0x5A;
constexpr size_t   MAX_PAYLOAD   = 32;
constexpr uint8_t  RSP_ACK          = 0x00;
constexpr uint8_t  RSP_OVERLENGTH   = 0x02;
constexpr uint8_t  RSP_CHECKSUM_ERR = 0x03;

// Simple packet receive state machine on Serial1
static uint8_t  rxBuf[64];
static size_t   rxLen = 0;
static bool     rxHavePkt = false;

static void try_parse_packet(void) {
  // Look for SYNC at start
  while (rxLen >= 7) {                       // minimum packet: sync,cmd,len,seqL,seqH,payload(0),cksum
    if (rxBuf[0] != PKT_SYNC) {
      // shift left until a sync byte is at the front
      memmove(rxBuf, rxBuf + 1, rxLen - 1);
      rxLen--;
      continue;
    }
    uint8_t cmd     = rxBuf[1];
    uint8_t plen    = rxBuf[2];
    size_t  total   = 6 + plen;              // 5 header bytes + payload + checksum
    if (rxLen < total) return;               // wait for more bytes

    uint8_t cks = cmd;
    cks ^= plen;
    cks ^= rxBuf[3];
    cks ^= rxBuf[4];
    for (uint8_t i = 0; i < plen; i++) cks ^= rxBuf[5 + i];

    uint8_t status;
    if (plen > MAX_PAYLOAD)      status = RSP_OVERLENGTH;
    else if (cks != rxBuf[total - 1]) status = RSP_CHECKSUM_ERR;
    else                          status = RSP_ACK;

    // Send response: 0x5A SEQ_LO SEQ_HI STATUS
    Serial1.write(RSP_SYNC);
    Serial1.write(rxBuf[3]);
    Serial1.write(rxBuf[4]);
    Serial1.write(status);

    // consume packet
    memmove(rxBuf, rxBuf + total, rxLen - total);
    rxLen -= total;
  }
}

void setup() {
  pinMode(HB_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(HB_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);        // debug over STLink VCP
  Serial1.begin(UART_BAUD);    // protocol UART to ESP32 (PA9 TX -> ESP32 RX GPIO16)

  delay(500);
  for (int i = 0; i < 4; ++i) {
    Serial.println("Nucleo F446RE DUT ready");
    Serial.println("Heartbeat -> PB5 (+ PA5 LED mirror)");
    Serial.println("UART to ESP32 -> Serial1 (PA9/PA10) @ 9600");
    delay(50);
  }
}

void loop() {
  // Heartbeat toggle every 100ms
  uint32_t now = millis();
  if (now - lastHeartbeat >= HEARTBEAT_MS) {
    bool newLevel = !digitalRead(HB_PIN);
    digitalWrite(HB_PIN, newLevel ? HIGH : LOW);
    digitalWrite(LED_PIN, newLevel ? HIGH : LOW);   // mirror for visibility
    lastHeartbeat = now;
  }

  // Receive and respond to UART fuzz packets
  while (Serial1.available()) {
    if (rxLen < sizeof(rxBuf)) {
      rxBuf[rxLen++] = Serial1.read();
    } else {
      // overflow: drop oldest
      memmove(rxBuf, rxBuf + 1, sizeof(rxBuf) - 1);
      rxBuf[sizeof(rxBuf) - 1] = Serial1.read();
    }
    try_parse_packet();
  }
}
