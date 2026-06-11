/**
 * Open Battery Information - ESP32-C3 Firmware
 *
 * FUNCTIONAL REQUIREMENTS:
 * 1. Serial bridge mode: Communicate with PC via USB serial using OBI protocol
 * 2. Web server mode: Browser-based interface for standalone diagnostics
 * 3. Support Makita LXT 18V battery protocol via modified OneWire
 * 4. WiFi configuration via captive portal on first boot
 *
 * HARDWARE CONFIGURATION:
 * - ESP32-C3 Super Mini
 * - GPIO3: OneWire data line (4.7kΩ pull-up to 3.3V)
 * - GPIO4: Enable pin (4.7kΩ pull-up to 3.3V)
 * - Battery Pin 2: OneWire data
 * - Battery Pin 6: Enable (must be HIGH during communication)
 *
 * MODES:
 * - Build with -DENABLE_WEB_SERVER=1 for standalone web interface
 * - Default build provides serial bridge compatible with Python GUI
 *
 * PROTOCOL (Serial Bridge):
 * Request:  [0x01][data_len][rsp_len][cmd][data...]
 * Response: [cmd][rsp_len][data...]
 *
 * AI-generated on 2025-12-16
 */

#include "OneWire2.h"
#include <Arduino.h>

#ifdef ENABLE_DISPLAY
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Wire.h>

// Bật cờ này để giả lập dữ liệu pin (tiện cho việc test UI)
// #define SIMULATE_BATTERY 1

TFT_eSPI tft = TFT_eSPI();
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_UPDATE_INTERVAL = 1000;

#define FT6336_ADDR 0x38
TFT_eSPI_Button btnRead;
TFT_eSPI_Button btnClear;
bool uiDrawn = false;
#endif

#ifdef ENABLE_WEB_SERVER
#include "web_interface.h"
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <WiFi.h>
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#endif

// Version
#define OBI_VERSION_MAJOR 1
#define OBI_VERSION_MINOR 0
#define OBI_VERSION_PATCH 0

// Pin definitions (can be overridden via build flags)
#ifndef ONEWIRE_PIN
#define ONEWIRE_PIN 3
#endif

#ifndef ENABLE_PIN
#define ENABLE_PIN 4
#endif

// WiFi credentials (for web server mode)
#ifndef WIFI_SSID
#define WIFI_SSID "YourSSID"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "YourPassword"
#endif

// Nibble swap helper
#define SWAP_NIBBLES(x) (((x) & 0x0F) << 4 | ((x) & 0xF0) >> 4)

// Instantiate OneWire with template pin
OneWire<ONEWIRE_PIN> makita;

#ifdef ENABLE_WEB_SERVER
WebServer server(80);
#endif

// Battery data structure
struct BatteryData {
  bool valid;
  char model[16];
  bool locked;
  uint16_t chargeCount;
  char mfgDate[16];
  float capacity;
  uint8_t errorCode;
  uint8_t romId[8];
  float packVoltage;
  float cellVoltage[5];
  float cellDiff;
  float tempCell;
  float tempMosfet;
};

BatteryData batteryData;

// Forward declarations
void processSerialCommand();
bool cmdAndRead33(byte *cmd, uint8_t cmd_len, byte *rsp, uint8_t rsp_len);
bool cmdAndReadCC(byte *cmd, uint8_t cmd_len, byte *rsp, uint8_t rsp_len);
void sendUSB(byte *rsp, byte rsp_len);
void setEnable(bool high);
void triggerPower();
bool readBatteryInfo();
bool readBatteryVoltages();
bool readBatteryModel();

#ifdef ENABLE_WEB_SERVER
void setupWebServer();
void setupOTA();
#endif

#ifdef ENABLE_DISPLAY
void setupDisplay();
void drawStaticUI();
void updateDynamicUI();
void handleDisplay();
bool getTouch(uint16_t *x, uint16_t *y);
#endif
void clearBatteryErrors();

// ------------------------------------------------------------------
// Setup
// ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Wait for serial connection (USB CDC)
  while (!Serial && millis() < 3000) {
    delay(10);
  }

#ifdef ENABLE_DISPLAY
  setupDisplay();
#endif

  // Configure pins
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW);

  // Initialise battery data
  memset(&batteryData, 0, sizeof(batteryData));

  Serial.println("=================================");
  Serial.println("OBI ESP32-C3 - Open Battery Info");
  Serial.println("=================================");
  Serial.printf("Version: %d.%d.%d\n", OBI_VERSION_MAJOR, OBI_VERSION_MINOR,
                OBI_VERSION_PATCH);
  Serial.printf("OneWire Pin: GPIO%d\n", ONEWIRE_PIN);
  Serial.printf("Enable Pin: GPIO%d\n", ENABLE_PIN);

#ifdef ENABLE_WEB_SERVER
  Serial.println("Mode: Web Server + Serial Bridge");
  Serial.println("Connecting to WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
    setupOTA();
    setupWebServer();
  } else {
    Serial.println();
    Serial.println("WiFi failed - Serial bridge only");
  }
#else
  Serial.println("Mode: Serial Bridge Only");
#endif

  Serial.println("Ready.");
}

// ------------------------------------------------------------------
// Main Loop
// ------------------------------------------------------------------
void loop() {
#ifdef ENABLE_WEB_SERVER
  ArduinoOTA.handle();
  server.handleClient();
#endif
#ifdef ENABLE_DISPLAY
  handleDisplay();
#endif
  processSerialCommand();
}

// ------------------------------------------------------------------
// Enable pin control
// ------------------------------------------------------------------
void setEnable(bool high) { digitalWrite(ENABLE_PIN, high ? HIGH : LOW); }

void triggerPower() {
  setEnable(false);
  delay(200);
  setEnable(true);
  delay(500);
}

// ------------------------------------------------------------------
// OneWire command functions
// ------------------------------------------------------------------

bool cmdAndRead33(byte *cmd, uint8_t cmd_len, byte *rsp, uint8_t rsp_len) {
  int i;

  for (int retry = 0; retry < 3; retry++) {
    if (!makita.reset()) {
      triggerPower();
      continue;
    }

    delayMicroseconds(310);
    makita.write(0x33);

    // Read 8-byte ROM ID
    for (i = 0; i < 8; i++) {
      delayMicroseconds(90);
      rsp[i] = makita.read();
    }

    // Write command
    for (i = 0; i < cmd_len; i++) {
      delayMicroseconds(90);
      makita.write(cmd[i]);
    }

    // Read response
    for (i = 8; i < rsp_len + 8; i++) {
      delayMicroseconds(90);
      rsp[i] = makita.read();
    }

    // Check if valid (not all 0xFF)
    bool valid = false;
    for (i = 0; i < rsp_len + 8; i++) {
      if (rsp[i] != 0xFF) {
        valid = true;
        break;
      }
    }
    if (valid)
      return true;
  }

  memset(rsp, 0xFF, rsp_len + 8);
  return false;
}

bool cmdAndReadCC(byte *cmd, uint8_t cmd_len, byte *rsp, uint8_t rsp_len) {
  int i;

  for (int retry = 0; retry < 3; retry++) {
    if (!makita.reset()) {
      triggerPower();
      continue;
    }

    delayMicroseconds(310);
    makita.write(0xCC);

    // Write command
    for (i = 0; i < cmd_len; i++) {
      delayMicroseconds(90);
      makita.write(cmd[i]);
    }

    // Read response
    for (i = 0; i < rsp_len; i++) {
      delayMicroseconds(90);
      rsp[i] = makita.read();
    }

    // Check if valid
    bool valid = false;
    for (i = 0; i < rsp_len; i++) {
      if (rsp[i] != 0xFF) {
        valid = true;
        break;
      }
    }
    if (valid)
      return true;
  }

  memset(rsp, 0xFF, rsp_len);
  return false;
}

// ------------------------------------------------------------------
// High-level battery functions
// ------------------------------------------------------------------

bool readBatteryInfo() {
  byte rsp[48];
  byte cmd[] = {0xAA, 0x00};

  setEnable(true);
  delay(400);

  bool success = cmdAndRead33(cmd, 2, rsp, 40);

  if (success) {
    // Copy ROM ID
    memcpy(batteryData.romId, rsp, 8);

    // Parse message data (offset by 8 for ROM ID)
    byte *msg = &rsp[8];

    // Manufacturing date is in ROM ID bytes - use ISO 8601 (YYYY-MM-DD)
    // romId[0] = year, romId[1] = month, romId[2] = day
    snprintf(batteryData.mfgDate, sizeof(batteryData.mfgDate),
             "20%02d-%02d-%02d",
             batteryData.romId[0],  // year
             batteryData.romId[1],  // month
             batteryData.romId[2]); // day

    // Charge count
    uint16_t rawCount = ((uint16_t)SWAP_NIBBLES(msg[29])) |
                        (((uint16_t)SWAP_NIBBLES(msg[28])) << 8);
    batteryData.chargeCount = rawCount & 0x0FFF;

    // Lock status
    batteryData.locked = (msg[22] & 0x0F) > 0;

    // Error code
    batteryData.errorCode = msg[21] & 0x0F;

    // Capacity
    batteryData.capacity = SWAP_NIBBLES(msg[18]) / 10.0f;

    batteryData.valid = true;
  }

  setEnable(false);
  return success;
}

bool readBatteryModel() {
  byte rsp[16];
  byte cmd[] = {0xDC, 0x0C};

  setEnable(true);
  delay(400);

  bool success = cmdAndReadCC(cmd, 2, rsp, 10);

  if (success && rsp[0] != 0xFF) {
    // Copy model string (null-terminate)
    memcpy(batteryData.model, rsp, 7);
    batteryData.model[7] = '\0';
  } else {
    // Try F0513 method for older batteries
    makita.reset();
    delayMicroseconds(400);
    makita.write(0xCC);
    delayMicroseconds(90);
    makita.write(0x99);
    delay(400);
    makita.reset();
    delayMicroseconds(400);
    makita.write(0x31);
    delayMicroseconds(90);
    byte b1 = makita.read();
    delayMicroseconds(90);
    byte b0 = makita.read();

    if (b0 != 0xFF && b1 != 0xFF) {
      snprintf(batteryData.model, sizeof(batteryData.model), "BL%02X%02X", b1,
               b0);
      success = true;
    }
  }

  setEnable(false);
  return success;
}

bool readBatteryVoltages() {
  byte rsp[32];
  byte cmd[] = {0xD7, 0x00, 0x00, 0xFF};

  setEnable(true);
  delay(400);

  bool success = cmdAndReadCC(cmd, 4, rsp, 29);

  if (success && rsp[0] != 0xFF) {
    batteryData.packVoltage =
        ((uint16_t)rsp[0] | ((uint16_t)rsp[1] << 8)) / 1000.0f;

    float maxV = 0, minV = 5;
    for (int i = 0; i < 5; i++) {
      float v = ((uint16_t)rsp[2 + i * 2] | ((uint16_t)rsp[3 + i * 2] << 8)) /
                1000.0f;
      batteryData.cellVoltage[i] = v;
      if (v > maxV)
        maxV = v;
      if (v < minV)
        minV = v;
    }
    batteryData.cellDiff = maxV - minV;

    // Cell temperature (offset 14-15)
    int16_t tempRaw = (int16_t)rsp[14] | ((int16_t)rsp[15] << 8);
    batteryData.tempCell = tempRaw / 100.0f;

    // MOSFET temperature (offset 16-17)
    int16_t tempMosfetRaw = (int16_t)rsp[16] | ((int16_t)rsp[17] << 8);
    batteryData.tempMosfet = tempMosfetRaw / 100.0f;
  } else {
    // Try F0513 method
    byte vcmd[1];
    bool f0513_ok = true;

    for (int i = 0; i < 5 && f0513_ok; i++) {
      vcmd[0] = 0x31 + i;
      if (cmdAndReadCC(vcmd, 1, rsp, 2)) {
        batteryData.cellVoltage[i] =
            ((uint16_t)rsp[0] | ((uint16_t)rsp[1] << 8)) / 1000.0f;
      } else {
        f0513_ok = false;
      }
    }

    if (f0513_ok) {
      // Calculate pack voltage and diff
      float sum = 0, maxV = 0, minV = 5;
      for (int i = 0; i < 5; i++) {
        sum += batteryData.cellVoltage[i];
        if (batteryData.cellVoltage[i] > maxV)
          maxV = batteryData.cellVoltage[i];
        if (batteryData.cellVoltage[i] < minV)
          minV = batteryData.cellVoltage[i];
      }
      batteryData.packVoltage = sum;
      batteryData.cellDiff = maxV - minV;

      // Temperature (F0513 only has cell temp, no MOSFET temp)
      vcmd[0] = 0x52;
      if (cmdAndReadCC(vcmd, 1, rsp, 2)) {
        batteryData.tempCell =
            ((uint16_t)rsp[0] | ((uint16_t)rsp[1] << 8)) / 100.0f;
        batteryData.tempMosfet = 0; // Not available on F0513
      }
      success = true;
    }
  }

  setEnable(false);
  return success;
}

// ------------------------------------------------------------------
// Serial communication (OBI Protocol)
// ------------------------------------------------------------------

void sendUSB(byte *rsp, byte rsp_len) {
  for (int i = 0; i < rsp_len; i++) {
    Serial.write(rsp[i]);
  }
}

void processSerialCommand() {
  if (Serial.available() >= 4) {
    byte start = Serial.read();
    byte len;
    byte rsp_len;
    byte cmd;
    byte data[255];
    byte rsp[255];

    if (start != 0x01) {
      return;
    }

    len = Serial.read();
    rsp_len = Serial.read();
    cmd = Serial.read();

    if (len > 0) {
      for (int i = 0; i < len; i++) {
        while (Serial.available() < 1) {
        }
        data[i] = Serial.read();
      }
    }

    setEnable(true);
    delay(400);

    switch (cmd) {
    case 0x01:
      rsp[0] = 0x01;
      rsp[1] = rsp_len;
      rsp[2] = OBI_VERSION_MAJOR;
      rsp[3] = OBI_VERSION_MINOR;
      rsp[4] = OBI_VERSION_PATCH;
      break;

    case 0x31:
    case 0x32: {
      makita.reset();
      delayMicroseconds(400);
      makita.write(0xCC);
      delayMicroseconds(90);
      makita.write(0x99);
      delay(400);
      makita.reset();
      delayMicroseconds(400);
      makita.write(cmd);
      delayMicroseconds(90);
      rsp[3] = makita.read();
      delayMicroseconds(90);
      rsp[2] = makita.read();
      break;
    }

    case 0x33:
      cmdAndRead33(data, len, &rsp[2], rsp_len);
      break;

    case 0xCC:
      cmdAndReadCC(data, len, &rsp[2], rsp_len);
      break;

    default:
      rsp_len = 0;
      break;
    }

    rsp[0] = cmd;
    rsp[1] = rsp_len;
    sendUSB(rsp, rsp_len + 2);

    setEnable(false);
  }
}

// ------------------------------------------------------------------
// OTA Updates
// ------------------------------------------------------------------

#ifdef ENABLE_WEB_SERVER
void setupOTA() {
  ArduinoOTA.setHostname("obi-esp32");

  ArduinoOTA.onStart([]() {
    String type =
        (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("OTA Start: " + type);
  });

  ArduinoOTA.onEnd([]() { Serial.println("\nOTA End"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}
#endif

// ------------------------------------------------------------------
// Web Server (Phase 2)
// ------------------------------------------------------------------

#ifdef ENABLE_WEB_SERVER
void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

void handleApiRead() {
  readBatteryInfo();
  readBatteryModel();
  readBatteryVoltages();

  JsonDocument doc;
  doc["success"] = batteryData.valid;
  doc["model"] = batteryData.model;
  doc["locked"] = batteryData.locked;
  doc["chargeCount"] = batteryData.chargeCount;
  doc["mfgDate"] = batteryData.mfgDate;
  doc["capacity"] = batteryData.capacity;
  doc["errorCode"] = batteryData.errorCode;
  doc["packVoltage"] = batteryData.packVoltage;
  doc["cell1"] = batteryData.cellVoltage[0];
  doc["cell2"] = batteryData.cellVoltage[1];
  doc["cell3"] = batteryData.cellVoltage[2];
  doc["cell4"] = batteryData.cellVoltage[3];
  doc["cell5"] = batteryData.cellVoltage[4];
  doc["cellDiff"] = batteryData.cellDiff;
  doc["tempCell"] = batteryData.tempCell;
  doc["tempMosfet"] = batteryData.tempMosfet;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleApiVoltages() {
  bool success = readBatteryVoltages();

  JsonDocument doc;
  doc["success"] = success;
  doc["packVoltage"] = batteryData.packVoltage;
  doc["cell1"] = batteryData.cellVoltage[0];
  doc["cell2"] = batteryData.cellVoltage[1];
  doc["cell3"] = batteryData.cellVoltage[2];
  doc["cell4"] = batteryData.cellVoltage[3];
  doc["cell5"] = batteryData.cellVoltage[4];
  doc["cellDiff"] = batteryData.cellDiff;
  doc["tempCell"] = batteryData.tempCell;
  doc["tempMosfet"] = batteryData.tempMosfet;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleApiLeds() {
  bool state = server.hasArg("state") && server.arg("state") == "1";

  setEnable(true);
  delay(400);

  // Test mode command
  byte cmd1[] = {0xD9, 0x96, 0xA5};
  byte rsp[32];
  cmdAndRead33(cmd1, 3, rsp, 9);

  // LED command
  byte cmd2[] = {0xDA, (byte)(state ? 0x31 : 0x34)};
  cmdAndRead33(cmd2, 2, rsp, 9);

  setEnable(false);

  server.send(200, "application/json", "{\"success\":true}");
}

void handleApiReset() {
  setEnable(true);
  delay(400);

  // Test mode
  byte cmd1[] = {0xD9, 0x96, 0xA5};
  byte rsp[32];
  cmdAndRead33(cmd1, 3, rsp, 9);

  // Reset error
  byte cmd2[] = {0xDA, 0x04};
  cmdAndRead33(cmd2, 2, rsp, 9);

  setEnable(false);

  server.send(200, "application/json", "{\"success\":true}");
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/read", HTTP_GET, handleApiRead);
  server.on("/api/voltages", HTTP_GET, handleApiVoltages);
  server.on("/api/leds", HTTP_GET, handleApiLeds);
  server.on("/api/reset", HTTP_GET, handleApiReset);

  server.begin();
  Serial.println("Web server started on port 80");
}
#endif

void clearBatteryErrors() {
  setEnable(true);
  delay(400);

  // Test mode
  byte cmd1[] = {0xD9, 0x96, 0xA5};
  byte rsp[32];
  cmdAndRead33(cmd1, 3, rsp, 9);

  // Reset error
  byte cmd2[] = {0xDA, 0x04};
  cmdAndRead33(cmd2, 2, rsp, 9);

  setEnable(false);
}

// ------------------------------------------------------------------
// Display
// ------------------------------------------------------------------

#ifdef ENABLE_DISPLAY
bool getTouch(uint16_t *x, uint16_t *y) {
  Wire.beginTransmission(FT6336_ADDR);
  Wire.write(0x02); // TD_STATUS
  if (Wire.endTransmission() != 0)
    return false;

  Wire.requestFrom((uint16_t)FT6336_ADDR, (uint8_t)5);
  if (Wire.available() < 5)
    return false;

  uint8_t touches = Wire.read();
  if (touches == 0 || touches > 2) {
    // clear buffer
    while (Wire.available())
      Wire.read();
    return false;
  }

  uint8_t x1 = Wire.read();
  uint8_t x2 = Wire.read();
  uint8_t y1 = Wire.read();
  uint8_t y2 = Wire.read();

  uint16_t touchX = ((x1 & 0x0F) << 8) | x2;
  uint16_t touchY = ((y1 & 0x0F) << 8) | y2;

  // Rotation 1 (Landscape) on most 320x240 FT6336 displays mapping
  *x = touchY;
  *y = 240 - touchX;

  return true;
}

#ifdef SIMULATE_BATTERY
void loadMockBatteryData() {
  batteryData.valid = true;
  strcpy(batteryData.model, "BL1850B");
  batteryData.packVoltage = 18.5;
  batteryData.capacity = 5.0;
  batteryData.chargeCount = 120;
  batteryData.cellVoltage[0] = 3.71;
  batteryData.cellVoltage[1] = 3.69;
  batteryData.cellVoltage[2] = 3.70;
  batteryData.cellVoltage[3] = 3.72;
  batteryData.cellVoltage[4] = 3.68;
  batteryData.cellDiff = 0.04;
  batteryData.tempCell = 32.5;
  batteryData.tempMosfet = 35.0;
  batteryData.errorCode = 0;
}
#endif

#define COLOR_BG 0x10A2
#define COLOR_CARD 0x2124
#define COLOR_TEXT 0xFFFF
#define COLOR_MUTED 0xAD55
#define COLOR_ACCENT 0x055F
#define COLOR_GOOD 0x072E
#define COLOR_WARN 0xFD00
#define COLOR_ERROR 0xF9E0

void drawBatteryIcon(int x, int y, float voltage, float capacity_percent) {
  tft.drawRoundRect(x, y + 2, 40, 20, 3, COLOR_TEXT);
  tft.fillRect(x + 40, y + 6, 3, 12, COLOR_TEXT);

  int fillWidth = (int)((capacity_percent / 100.0f) * 36);
  if (fillWidth > 36)
    fillWidth = 36;
  if (fillWidth < 0)
    fillWidth = 0;

  uint16_t fillColor = COLOR_GOOD;
  if (capacity_percent < 40)
    fillColor = COLOR_WARN;
  if (capacity_percent < 15)
    fillColor = COLOR_ERROR;

  tft.fillRect(x + 2, y + 4, 36, 16, COLOR_CARD);
  if (fillWidth > 0)
    tft.fillRect(x + 2, y + 4, fillWidth, 16, fillColor);
}

void drawThermometerIcon(int x, int y, float temp) {
  tft.drawRoundRect(x + 4, y, 6, 16, 3, COLOR_MUTED);
  tft.drawCircle(x + 7, y + 18, 5, COLOR_MUTED);

  int fillH = (int)((temp / 60.0f) * 12);
  if (fillH > 12)
    fillH = 12;
  if (fillH < 0)
    fillH = 0;

  uint16_t color =
      (temp > 45) ? COLOR_ERROR : ((temp > 35) ? COLOR_WARN : COLOR_ACCENT);

  tft.fillCircle(x + 7, y + 18, 3, color);
  tft.fillRect(x + 5, y + 14 - fillH, 4, fillH + 2, color);
}

void drawCellBarChart(int x, int y, float *cells) {
  int width = 135;
  int height = 80;

  tft.drawFastHLine(x, y, width, COLOR_BG);
  tft.drawFastHLine(x, y + height / 2, width, COLOR_BG);
  tft.drawFastHLine(x, y + height, width, COLOR_BG);

  tft.setTextSize(1);

  for (int i = 0; i < 5; i++) {
    int barX = x + 2 + (i * 26);
    float v = cells[i];
    if (v > 4.2f)
      v = 4.2f;
    if (v < 3.0f)
      v = 3.0f;

    int barH = (int)(((v - 3.0f) / 1.2f) * height);
    int barY = y + height - barH;

    uint16_t color = COLOR_GOOD;
    if (v < 3.3f)
      color = COLOR_WARN;
    if (v < 3.0f)
      color = COLOR_ERROR;

    tft.fillRect(barX, y, 18, height - barH, COLOR_CARD);
    tft.fillRect(barX, barY, 18, barH, color);

    tft.setTextColor(COLOR_TEXT, COLOR_CARD);
    tft.setCursor(barX + 6, y + height + 5);
    tft.print(i + 1);

    tft.setCursor(barX - 4, barY - 12);
    tft.printf("%.2f", cells[i]);
  }
}

void setupDisplay() {
  Serial.println("setupDisplay: Bắt đầu khởi tạo màn hình...");
#ifdef TFT_BL
  analogWrite(TFT_BL, 150);
#endif
  delay(100);

  tft.init();
  tft.invertDisplay(true);
  tft.setRotation(1);

  Wire.begin(TOUCH_SDA, TOUCH_SCL);

  // Khởi tạo nút bấm mới (màu xám nhạt, viền ẩn)
  btnRead.initButton(&tft, 80, 215, 140, 36, COLOR_BG, COLOR_CARD, COLOR_ACCENT,
                     (char *)"DOC PIN", 2);
  btnClear.initButton(&tft, 240, 215, 140, 36, COLOR_BG, COLOR_CARD,
                      COLOR_ERROR, (char *)"XOA LOI", 2);

#ifdef RGB_LED_PIN
  pinMode(RGB_LED_PIN, OUTPUT);
  digitalWrite(RGB_LED_PIN, LOW);
#endif

#ifdef SIMULATE_BATTERY
  loadMockBatteryData();
#endif

  drawStaticUI();
  uiDrawn = true;
  updateDynamicUI();
}

void drawStaticUI() {
  tft.fillScreen(COLOR_BG);

  // Top Bar
  tft.fillRect(0, 0, 320, 25, COLOR_BG);
  tft.setTextColor(COLOR_ACCENT, COLOR_BG);
  tft.setTextSize(2);
  tft.setCursor(5, 5);
  tft.print("Makita Analyzer");

  // Left Card (Cells)
  tft.fillRoundRect(5, 30, 150, 160, 8, COLOR_CARD);
  tft.setTextColor(COLOR_MUTED, COLOR_CARD);
  tft.setTextSize(1);
  tft.setCursor(15, 38);
  tft.print("CELL VOLTAGES");

  // Right Card (Info)
  tft.fillRoundRect(165, 30, 150, 160, 8, COLOR_CARD);
  tft.setTextColor(COLOR_MUTED, COLOR_CARD);
  tft.setCursor(175, 38);
  tft.print("BATTERY INFO");

  // Nút bấm
  btnRead.drawButton();
  btnClear.drawButton();
}

void updateDynamicUI() {
  tft.setTextSize(1);
  tft.setCursor(215, 8);
  if (batteryData.valid) {
    tft.fillCircle(205, 11, 4, COLOR_GOOD);
    tft.setTextColor(COLOR_GOOD, COLOR_BG);
    tft.print("CONNECTED   ");
  } else {
    tft.fillCircle(205, 11, 4, COLOR_ERROR);
    tft.setTextColor(COLOR_ERROR, COLOR_BG);
    tft.print("NO BATTERY  ");
  }

  static bool wasValid = true; // Lưu trạng thái trước đó

  if (!batteryData.valid) {
    if (wasValid) { // Chỉ xóa và vẽ chữ NO DATA nếu vừa mất kết nối
      tft.fillRoundRect(5, 30, 150, 160, 8, COLOR_CARD);
      tft.setTextColor(COLOR_MUTED, COLOR_CARD);
      tft.setCursor(15, 38);
      tft.print("CELL VOLTAGES");

      tft.fillRoundRect(165, 30, 150, 160, 8, COLOR_CARD);
      tft.setTextColor(COLOR_MUTED, COLOR_CARD);
      tft.setCursor(175, 38);
      tft.print("BATTERY INFO");

      tft.setTextColor(COLOR_MUTED, COLOR_CARD);
      tft.setTextSize(2);
      tft.setCursor(35, 100);
      tft.print("NO DATA");
      tft.setCursor(195, 100);
      tft.print("NO DATA");
      wasValid = false;
    }
    return;
  }

  if (!wasValid) {
    // Nếu vừa có kết nối lại, xóa sạch card 1 lần để bay chữ NO DATA
    tft.fillRoundRect(5, 30, 150, 160, 8, COLOR_CARD);
    tft.setTextColor(COLOR_MUTED, COLOR_CARD);
    tft.setTextSize(1);
    tft.setCursor(15, 38);
    tft.print("CELL VOLTAGES");

    tft.fillRoundRect(165, 30, 150, 160, 8, COLOR_CARD);
    tft.setTextColor(COLOR_MUTED, COLOR_CARD);
    tft.setCursor(175, 38);
    tft.print("BATTERY INFO");
    wasValid = true;
  }

  // KHÔNG DÙNG fillRect xóa toàn bộ ruột card ở mỗi chu kỳ nữa để chống chớp
  // (Flicker) Các hàm vẽ bar chart và text nền đã tự đè lên pixel cũ

  // Left Area: Bar Chart & Diff
  drawCellBarChart(10, 65, batteryData.cellVoltage);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_WARN, COLOR_CARD);
  tft.setCursor(15, 175);
  // Thêm khoảng trắng phía sau để ghi đè số cũ nếu chuỗi mới ngắn hơn
  tft.printf("Max Diff: %-5.2fV  ", batteryData.cellDiff);

  // Right Area: Battery Icon & Info
  float percent = (batteryData.packVoltage - 15.0f) / 6.0f * 100.0f;
  if (percent > 100)
    percent = 100;
  if (percent < 0)
    percent = 0;

  drawBatteryIcon(175, 55, batteryData.packVoltage, percent);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_CARD);
  tft.setCursor(225, 58);
  tft.printf("%-5.1fV ", batteryData.packVoltage);

  // Info Lines
  tft.setTextSize(1);
  int infoY = 85;
  tft.setTextColor(COLOR_MUTED, COLOR_CARD);
  tft.setCursor(175, infoY);
  tft.print("Model:");
  tft.setTextColor(COLOR_TEXT, COLOR_CARD);
  tft.setCursor(220, infoY);
  tft.printf("%-10s", batteryData.model);

  infoY += 20;
  tft.setTextColor(COLOR_MUTED, COLOR_CARD);
  tft.setCursor(175, infoY);
  tft.print("Cap:");
  tft.setTextColor(COLOR_TEXT, COLOR_CARD);
  tft.setCursor(210, infoY);
  tft.printf("%-4.1f Ah ", batteryData.capacity);

  infoY += 20;
  tft.setTextColor(COLOR_MUTED, COLOR_CARD);
  tft.setCursor(175, infoY);
  tft.print("Cycl:");
  tft.setTextColor(COLOR_TEXT, COLOR_CARD);
  tft.setCursor(215, infoY);
  tft.printf("%-5d ", batteryData.chargeCount);

  infoY += 20;
  drawThermometerIcon(175, infoY - 5, batteryData.tempCell);
  tft.setTextColor(COLOR_TEXT, COLOR_CARD);
  tft.setCursor(195, infoY);
  tft.printf("%-4.1fC ", batteryData.tempCell);
  tft.setTextColor(COLOR_MUTED, COLOR_CARD);
  tft.setCursor(235, infoY);
  tft.printf("(%-2.0fM) ", batteryData.tempMosfet);

  infoY += 20;
  tft.setTextColor(COLOR_MUTED, COLOR_CARD);
  tft.setCursor(175, infoY);
  tft.print("Err:");
  if (batteryData.errorCode > 0) {
    tft.setTextColor(COLOR_ERROR, COLOR_CARD);
    tft.setCursor(205, infoY);
    tft.printf("CODE %-4d", batteryData.errorCode);
  } else {
    tft.setTextColor(COLOR_GOOD, COLOR_CARD);
    tft.setCursor(205, infoY);
    tft.print("NONE      ");
  }
}

void handleDisplay() {
  uint16_t t_x = 0, t_y = 0;
  bool pressed = getTouch(&t_x, &t_y);

  if (pressed) {
    btnRead.press(btnRead.contains(t_x, t_y));
    btnClear.press(btnClear.contains(t_x, t_y));
  } else {
    btnRead.press(false);
    btnClear.press(false);
  }

  if (btnRead.justReleased())
    btnRead.drawButton();
  if (btnClear.justReleased())
    btnClear.drawButton();

  if (btnRead.justPressed()) {
    btnRead.drawButton(true);
    Serial.println("Touch: Nút ĐỌC PIN được nhấn");
#ifdef SIMULATE_BATTERY
    loadMockBatteryData();
    batteryData.chargeCount++; // Tăng nhẹ để thấy UI có thay đổi
#else
    setEnable(true);
    delay(50);
    bool present = makita.reset();
    setEnable(false);

    if (present) {
      readBatteryInfo();
      readBatteryModel();
      readBatteryVoltages();
    } else {
      batteryData.valid = false;
    }
#endif
    updateDynamicUI();
  }

  if (btnClear.justPressed()) {
    btnClear.drawButton(true);
    Serial.println("Touch: Nút XÓA LỖI được nhấn");
#ifdef SIMULATE_BATTERY
    batteryData.errorCode = 0;
    batteryData.chargeCount = 0;
#else
    clearBatteryErrors();

    // Đọc lại sau khi xóa
    setEnable(true);
    delay(50);
    if (makita.reset()) {
      setEnable(false);
      readBatteryInfo();
      readBatteryModel();
      readBatteryVoltages();
    } else {
      setEnable(false);
      batteryData.valid = false;
    }
#endif
    updateDynamicUI();
  }

  if (millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
    lastDisplayUpdate = millis();
    // Cập nhật lại màn hình nếu không có lỗi, để lấy trạng thái mới nhất
    if (batteryData.valid) {
      readBatteryVoltages();
      updateDynamicUI();
    }
  }
}
#endif
