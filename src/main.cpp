#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "config.h"
#include "display_setup.h"
#include "ui_dashboard.h"
#include "led_controller.h"

// Instâncias globais
LGFX_CYD lcd;
UIDashboard ui(lcd);
LEDController leds;
Preferences prefs;

SystemStats stats;
String serialRxBuffer = "";
unsigned long lastSerialByteMs = 0;
unsigned long lastHttpPollMs = 0;
unsigned long lastTouchCheckMs = 0;
unsigned long touchStartTime = 0;
bool touchWasPressed = false;
bool displayInverted = DEFAULT_COLOR_INVERT;
bool needsRedraw = false;

// =========================================================================
//  PARSER DE JSON (Recebido via Serial USB ou HTTP Wi-Fi)
// =========================================================================

bool parseJsonPayload(const String& jsonStr, bool fromUsb) {
  // Ajusta capacidade do buffer do ArduinoJson v7
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err) {
    return false;
  }

  // Hostname
  if (doc["host"].is<const char*>()) {
    stats.hostName = doc["host"].as<String>();
  }

  // CPU
  if (doc["cpu"].is<JsonObject>()) {
    JsonObject cpu = doc["cpu"];
    if (cpu["name"].is<const char*>()) stats.cpuName = cpu["name"].as<String>();
    if (cpu["usage"].is<float>()) stats.cpuUsage = cpu["usage"].as<float>();
    if (cpu["temp"].is<float>()) stats.cpuTemp = cpu["temp"].as<float>();
    if (cpu["freq"].is<float>()) stats.cpuFreq = cpu["freq"].as<float>();
  }

  // GPU
  if (doc["gpu"].is<JsonObject>()) {
    JsonObject gpu = doc["gpu"];
    if (gpu["name"].is<const char*>()) stats.gpuName = gpu["name"].as<String>();
    if (gpu["usage"].is<int>()) stats.gpuUsage = gpu["usage"].as<int>();
    if (gpu["temp"].is<float>()) stats.gpuTemp = gpu["temp"].as<float>();
  }

  // RAM
  if (doc["ram"].is<JsonObject>()) {
    JsonObject ram = doc["ram"];
    if (ram["used"].is<float>()) stats.ramUsed = ram["used"].as<float>();
    if (ram["total"].is<float>()) stats.ramTotal = ram["total"].as<float>();
    if (ram["pct"].is<float>()) stats.ramPercent = ram["pct"].as<float>();
    else if (ram["percent"].is<float>()) stats.ramPercent = ram["percent"].as<float>();
  }

  // Disco / Armazenamento
  if (doc["disk"].is<JsonObject>()) {
    JsonObject disk = doc["disk"];
    if (disk["used"].is<float>()) stats.diskUsed = disk["used"].as<float>();
    if (disk["total"].is<float>()) stats.diskTotal = disk["total"].as<float>();
    if (disk["pct"].is<float>()) stats.diskPercent = disk["pct"].as<float>();
    if (disk["temp"].is<float>()) stats.ssdTemp = disk["temp"].as<float>();
  }

  // Atualiza metadados
  stats.connected = true;
  stats.isUsb = fromUsb;
  stats.lastUpdateMs = millis();
  needsRedraw = true;

  return true;
}

// =========================================================================
//  LEITURA NÃO-BLOQUEANTE DA SERIAL (USB)
// =========================================================================

void handleSerialInput() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    lastSerialByteMs = millis();

    if (c == '\n' || c == '\r') {
      if (serialRxBuffer.length() > 10) {
        serialRxBuffer.trim();
        if (serialRxBuffer.startsWith("{") && serialRxBuffer.endsWith("}")) {
          parseJsonPayload(serialRxBuffer, true);
        }
      }
      serialRxBuffer = "";
    } else {
      if (serialRxBuffer.length() < 1024) {
        serialRxBuffer += c;
      } else {
        serialRxBuffer = ""; // Overflow guard
      }
    }
  }
}

// =========================================================================
//  CLIENTE HTTP WI-FI (OPCIONAL QUANDO DESCONECTADO DO USB)
// =========================================================================

void handleWiFiPolling() {
  // Se está recebendo dados via USB Serial recentemente, prioriza USB!
  if (millis() - stats.lastUpdateMs < SERIAL_TIMEOUT_MS && stats.isUsb) {
    return;
  }

  if (String(WIFI_SSID).length() == 0) return;

  if (WiFi.status() != WL_CONNECTED) {
    stats.wifiIp = "";
    return;
  }

  stats.wifiIp = WiFi.localIP().toString();
  stats.wifiRssi = WiFi.RSSI();

  unsigned long now = millis();
  if (now - lastHttpPollMs < HTTP_POLL_INTERVAL) return;
  lastHttpPollMs = now;

  HTTPClient http;
  String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + "/api/stats";
  http.begin(url);
  http.setTimeout(1200);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    parseJsonPayload(payload, false);
  } else {
    // Se o HTTP falhar e já passou do timeout, marca como desconectado
    if (now - stats.lastUpdateMs > SERIAL_TIMEOUT_MS + 2000) {
      stats.connected = false;
    }
  }
  http.end();
}

// =========================================================================
//  GERENCIAMENTO DO TOUCHSCREEN E TOQUE LONGO
// =========================================================================

void handleTouch() {
  uint16_t x, y;
  bool isTouched = lcd.getTouch(&x, &y);
  unsigned long now = millis();

  if (isTouched) {
    if (!touchWasPressed) {
      touchWasPressed = true;
      touchStartTime = now;
    } else {
      // Toque longo (> 3 segundos) alterna a inversão de cores e salva na memória NVS
      if (touchStartTime > 0 && (now - touchStartTime > 3000)) {
        displayInverted = !displayInverted;
        lcd.invertDisplay(displayInverted);

        prefs.begin("cyd_v2", false);
        prefs.putBool("invert", displayInverted);
        prefs.end();

        // Feedback visual
        lcd.fillScreen(COLOR_BG);
        touchStartTime = 0; // Evita repetição imediata
      }
    }
  } else {
    if (touchWasPressed) {
      unsigned long duration = now - touchStartTime;
      touchWasPressed = false;
      touchStartTime = 0;

      if (duration >= 50 && duration < 800) {
        // Toque curto:
        // Metade superior da tela = Alterna a Vista (Dashboard / Detalhes)
        // Metade inferior da tela = Cicla o Brilho da Tela (100% -> 70% -> 35% -> 10%)
        if (y < 120) {
          ui.toggleView();
          needsRedraw = true;
        } else {
          ui.cycleBrightness();
          needsRedraw = true;
        }
      }
    }
  }
}

// =========================================================================
//  CONFIGURAÇÃO INICIAL (SETUP)
// =========================================================================

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println("\n[CYD] Iniciando Monitor Linux...");

  // Inicializa LED onboard
  leds.begin();

  // Carrega preferências da NVS
  prefs.begin("cyd_v2", true);
  displayInverted = prefs.getBool("invert", DEFAULT_COLOR_INVERT);
  prefs.end();

  // Inicializa Display e UI
  ui.begin();
  lcd.invertDisplay(displayInverted);

  // Inicializa dados com valores padrão
  stats.hostName = "Linux PC";
  stats.cpuName = "CPU";
  stats.gpuName = "GPU";
  stats.connected = false;
  stats.isUsb = true;
  stats.lastUpdateMs = 0;

  // Mostra tela de espera inicial
  ui.drawWaitingScreen("Aguardando conexao...");

  // Inicia Wi-Fi caso configurado
  if (String(WIFI_SSID).length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

// =========================================================================
//  LOOP PRINCIPAL
// =========================================================================

void loop() {
  // 1. Processa dados da Serial USB
  handleSerialInput();

  // 2. Processa Wi-Fi HTTP caso USB esteja inativo
  handleWiFiPolling();

  // 3. Processa interação por toque
  handleTouch();

  // 4. Verifica timeout de conexão
  unsigned long now = millis();
  if (stats.connected && (now - stats.lastUpdateMs > SERIAL_TIMEOUT_MS)) {
    stats.connected = false;
    ui.drawWaitingScreen("Conexao perdida. Reconectando...");
    needsRedraw = false;
  }

  // 5. Atualização do LED RGB (atualizado a cada 200ms para suavidade/piscar)
  static unsigned long lastLedMs = 0;
  if (now - lastLedMs >= 200) {
    lastLedMs = now;
    float maxLoad = max(stats.cpuUsage, max((float)stats.gpuUsage, stats.ramPercent));
    float maxTemp = max(stats.cpuTemp, max(stats.gpuTemp, stats.ssdTemp));
    leds.update(maxLoad, maxTemp, stats.connected);
  }

  // 6. Atualização visual da tela (somente quando novos dados chegam ou na interação)
  static unsigned long lastRenderMs = 0;
  if (needsRedraw || (stats.connected && (now - lastRenderMs >= 1000))) {
    lastRenderMs = now;
    needsRedraw = false;

    if (stats.connected) {
      if (ui.getView() == 0) {
        ui.drawDashboard(stats);
      } else {
        ui.drawDetailsView(stats);
      }
    }
  }

  delay(5);
}
