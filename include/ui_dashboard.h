#pragma once
#include <Arduino.h>
#include "display_setup.h"

// =========================================================================
//  ESTRUTURA DE DADOS DOS SENSORES
// =========================================================================

struct SystemStats {
  String hostName;
  
  // CPU
  String cpuName;
  float cpuUsage;
  float cpuTemp;
  float cpuFreq;

  // GPU
  String gpuName;
  int gpuUsage;
  float gpuTemp;

  // RAM
  float ramUsed;
  float ramTotal;
  float ramPercent;

  // Storage / SSD
  float diskUsed;
  float diskTotal;
  float diskPercent;
  float ssdTemp;

  // Metadados da conexão
  bool connected;
  bool isUsb;         // true = USB Serial, false = Wi-Fi
  String wifiIp;
  int wifiRssi;
  unsigned long lastUpdateMs;
};

// =========================================================================
//  CORES DO TEMA DARK (RGB565)
// =========================================================================

#define COLOR_BG            0x0862  // #0b0e14 - Fundo geral do monitor
#define COLOR_HEADER_BG     0x10A4  // #13171f - Fundo da barra de status
#define COLOR_CARD_BG       0x18E5  // #161b22 - Fundo dos cartões
#define COLOR_CARD_BORDER   0x2967  // #2b3340 - Borda sutil dos cartões
#define COLOR_BAR_TRACK     0x2125  // #202632 - Trilho de fundo das barras

#define COLOR_TEXT_WHITE    0xFFFF  // #ffffff - Texto principal
#define COLOR_TEXT_MUTED    0x9CF3  // #94a3b8 - Texto secundário/rótulos
#define COLOR_TEXT_DARK     0x52AA  // #525a66 - Texto desbotado

#define COLOR_ACCENT_CPU    0x3DEF  // #38bdf8 - Ciano suave
#define COLOR_ACCENT_GPU    0xB2DF  // #a855f7 - Roxo vibrante
#define COLOR_ACCENT_RAM    0x36F3  // #34d399 - Verde Esmeralda
#define COLOR_ACCENT_DISK   0xFDE4  // #fbbf24 - Âmbar/Dourado

#define COLOR_LEVEL_GREEN   0x07E0  // #00ff88 - Normal (< 60%)
#define COLOR_LEVEL_ORANGE  0xFDA0  // #ffaa00 - Atenção (60% - 85%)
#define COLOR_LEVEL_RED     0xF9A6  // #ff4444 - Alerta (> 85%)

// =========================================================================
//  CLASSE DE RENDERIZAÇÃO DA INTERFACE
// =========================================================================

class UIDashboard {
private:
  LGFX_CYD& _lcd;
  LGFX_Sprite _cardSprite;
  LGFX_Sprite _headerSprite;
  uint8_t _currentView; // 0 = Dashboard 2x2, 1 = Detalhes do Hardware
  uint8_t _brightnessLevel; // 0 = Dim, 1 = Low, 2 = Med, 3 = High
  const uint8_t _brightnessValues[4] = { BRIGHTNESS_DIM, BRIGHTNESS_LOW, BRIGHTNESS_MED, BRIGHTNESS_HIGH };

  // Retorna a cor da barra com base no percentual (idêntico ao Monitor Linux)
  uint16_t getLoadColor(float percent) {
    if (percent < 60.0f) return COLOR_LEVEL_GREEN;
    if (percent < 85.0f) return COLOR_LEVEL_ORANGE;
    return COLOR_LEVEL_RED;
  }

  // Retorna cor com base na temperatura (°C)
  uint16_t getTempColor(float temp) {
    if (temp <= 0.0f) return COLOR_TEXT_MUTED;
    if (temp < 65.0f) return COLOR_LEVEL_GREEN;
    if (temp < 80.0f) return COLOR_LEVEL_ORANGE;
    return COLOR_LEVEL_RED;
  }

  // Desenha uma barra de progresso arredondada dentro do sprite
  void drawProgressBar(LGFX_Sprite& sp, int x, int y, int w, int h, float percent, uint16_t fillColor) {
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;
    
    // Fundo do trilho
    sp.fillRoundRect(x, y, w, h, h / 2, COLOR_BAR_TRACK);

    // Preenchimento proporcional
    int fillW = (int)((w * percent) / 100.0f);
    if (fillW > 0) {
      if (fillW < h) fillW = h; // Para manter o arredondamento perfeito
      sp.fillRoundRect(x, y, fillW, h, h / 2, fillColor);
    }
  }

public:
  UIDashboard(LGFX_CYD& lcd) 
    : _lcd(lcd), _cardSprite(&lcd), _headerSprite(&lcd), _currentView(0), _brightnessLevel(3) {}

  void begin() {
    _lcd.init();
    _lcd.setRotation(SCREEN_ROTATION);
    _lcd.setBrightness(_brightnessValues[_brightnessLevel]);
    _lcd.fillScreen(COLOR_BG);

    // Cria os buffers de sprite para renderização sem flicker
    _cardSprite.setColorDepth(16);
    _cardSprite.createSprite(151, 98);

    _headerSprite.setColorDepth(16);
    _headerSprite.createSprite(320, 24);
  }

  void cycleBrightness() {
    _brightnessLevel = (_brightnessLevel + 1) % 4;
    _lcd.setBrightness(_brightnessValues[_brightnessLevel]);
  }

  void setBrightnessLevel(uint8_t lvl) {
    if (lvl < 4) {
      _brightnessLevel = lvl;
      _lcd.setBrightness(_brightnessValues[_brightnessLevel]);
    }
  }

  uint8_t getBrightnessPercent() const {
    switch (_brightnessLevel) {
      case 0: return 10;
      case 1: return 35;
      case 2: return 70;
      default: return 100;
    }
  }

  void toggleView() {
    _currentView = (_currentView == 0) ? 1 : 0;
    _lcd.fillScreen(COLOR_BG);
  }

  uint8_t getView() const {
    return _currentView;
  }

  // =======================================================================
  //  BARRA SUPERIOR (HEADER)
  // =======================================================================
  void drawHeader(const SystemStats& stats) {
    _headerSprite.fillSprite(COLOR_HEADER_BG);
    _headerSprite.drawFastHLine(0, 23, 320, COLOR_CARD_BORDER);

    // Ícone indicador (verde = conectado, azul = aguardando)
    uint16_t statusDotColor = stats.connected ? COLOR_LEVEL_GREEN : COLOR_ACCENT_CPU;
    _headerSprite.fillCircle(12, 11, 4, statusDotColor);

    // Título Principal
    _headerSprite.setTextColor(COLOR_TEXT_WHITE, COLOR_HEADER_BG);
    _headerSprite.setFont(&fonts::Font0);
    _headerSprite.setTextSize(1);
    _headerSprite.drawString("MONITOR LINUX", 22, 7);

    // Nome da Máquina / Host
    if (stats.hostName.length() > 0) {
      _headerSprite.setTextColor(COLOR_TEXT_MUTED, COLOR_HEADER_BG);
      String hostDisp = stats.hostName;
      if (hostDisp.length() > 14) hostDisp = hostDisp.substring(0, 12) + "..";
      _headerSprite.drawCenterString(hostDisp.c_str(), 160, 7);
    }

    // Badge de Conexão no canto direito
    if (stats.connected) {
      if (stats.isUsb) {
        _headerSprite.fillRoundRect(260, 3, 52, 17, 4, 0x03E0);
        _headerSprite.setTextColor(COLOR_TEXT_WHITE, 0x03E0);
        _headerSprite.drawCenterString("USB", 286, 7);
      } else {
        _headerSprite.fillRoundRect(254, 3, 58, 17, 4, 0x1A7A);
        _headerSprite.setTextColor(COLOR_TEXT_WHITE, 0x1A7A);
        _headerSprite.drawCenterString("Wi-Fi", 283, 7);
      }
    } else {
      _headerSprite.fillRoundRect(248, 3, 64, 17, 4, 0x4208);
      _headerSprite.setTextColor(COLOR_TEXT_MUTED, 0x4208);
      _headerSprite.drawCenterString("OFFLINE", 280, 7);
    }

    _headerSprite.pushSprite(&_lcd, 0, 0);
  }

  // =======================================================================
  //  TELA DE ESPERA / AGUARDANDO PC
  // =======================================================================
  void drawWaitingScreen(const String& infoMsg) {
    drawHeader({ "", "", 0, 0, 0, "", 0, 0, 0, 0, 0, 0, 0, 0, 0, false, false, "", 0, 0 });

    _lcd.fillRect(10, 34, 300, 196, COLOR_BG);
    
    // Caixa central estilizada
    _lcd.fillRoundRect(25, 45, 270, 175, 10, COLOR_CARD_BG);
    _lcd.drawRoundRect(25, 45, 270, 175, 10, COLOR_CARD_BORDER);

    // Ícone de computador
    _lcd.drawRoundRect(130, 60, 60, 42, 4, COLOR_ACCENT_CPU);
    _lcd.fillRect(133, 63, 54, 36, COLOR_BG);
    _lcd.drawFastHLine(148, 106, 24, COLOR_TEXT_MUTED);
    _lcd.drawFastHLine(142, 110, 36, COLOR_TEXT_MUTED);

    _lcd.setTextColor(COLOR_TEXT_WHITE, COLOR_CARD_BG);
    _lcd.setFont(&fonts::Font2);
    _lcd.drawCenterString("Aguardando Conexao", 160, 120);

    _lcd.setFont(&fonts::Font0);
    _lcd.setTextSize(1);
    _lcd.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
    _lcd.drawCenterString("Conecte o cabo USB ou inicie o agente:", 160, 145);

    _lcd.setTextColor(COLOR_LEVEL_GREEN, COLOR_CARD_BG);
    _lcd.drawCenterString("./run_agent.sh", 160, 162);

    if (infoMsg.length() > 0) {
      _lcd.setTextColor(COLOR_TEXT_DARK, COLOR_CARD_BG);
      _lcd.drawCenterString(infoMsg.c_str(), 160, 185);
    }
  }

  // =======================================================================
  //  CARTÃO: CPU
  // =======================================================================
  void renderCpuCard(const SystemStats& stats, int posX, int posY) {
    _cardSprite.fillSprite(COLOR_CARD_BG);
    _cardSprite.drawRoundRect(0, 0, 151, 98, 6, COLOR_CARD_BORDER);

    // Top Header com etiqueta colorida
    _cardSprite.fillRoundRect(8, 7, 34, 14, 3, COLOR_ACCENT_CPU);
    _cardSprite.setTextColor(COLOR_BG, COLOR_ACCENT_CPU);
    _cardSprite.setFont(&fonts::Font0);
    _cardSprite.setTextSize(1);
    _cardSprite.drawString("CPU", 14, 10);

    // Nome resumido da CPU
    String name = stats.cpuName;
    if (name.length() > 14) name = name.substring(0, 13) + ".";
    _cardSprite.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
    _cardSprite.drawString(name.c_str(), 48, 10);

    // Percentual em destaque
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f%%", stats.cpuUsage);
    _cardSprite.setFont(&fonts::Font4);
    _cardSprite.setTextColor(COLOR_TEXT_WHITE, COLOR_CARD_BG);
    _cardSprite.drawString(buf, 8, 27);

    // Temperatura
    if (stats.cpuTemp > 0.0f) {
      snprintf(buf, sizeof(buf), "%.0f'C", stats.cpuTemp);
      _cardSprite.setFont(&fonts::Font2);
      _cardSprite.setTextColor(getTempColor(stats.cpuTemp), COLOR_CARD_BG);
      _cardSprite.drawRightString(buf, 143, 30);
    }

    // Frequência (GHz)
    _cardSprite.setFont(&fonts::Font0);
    _cardSprite.setTextSize(1);
    _cardSprite.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
    if (stats.cpuFreq > 0.0f) {
      snprintf(buf, sizeof(buf), "Clock: %.2f GHz", stats.cpuFreq);
      _cardSprite.drawString(buf, 8, 58);
    } else {
      _cardSprite.drawString("Uso do Processador", 8, 58);
    }

    // Barra de progresso dinâmica
    drawProgressBar(_cardSprite, 8, 76, 135, 10, stats.cpuUsage, getLoadColor(stats.cpuUsage));

    _cardSprite.pushSprite(&_lcd, posX, posY);
  }

  // =======================================================================
  //  CARTÃO: GPU
  // =======================================================================
  void renderGpuCard(const SystemStats& stats, int posX, int posY) {
    _cardSprite.fillSprite(COLOR_CARD_BG);
    _cardSprite.drawRoundRect(0, 0, 151, 98, 6, COLOR_CARD_BORDER);

    // Top Header com etiqueta colorida
    _cardSprite.fillRoundRect(8, 7, 34, 14, 3, COLOR_ACCENT_GPU);
    _cardSprite.setTextColor(COLOR_BG, COLOR_ACCENT_GPU);
    _cardSprite.setFont(&fonts::Font0);
    _cardSprite.setTextSize(1);
    _cardSprite.drawString("GPU", 14, 10);

    // Nome resumido da GPU
    String name = stats.gpuName;
    if (name.length() > 14) name = name.substring(0, 13) + ".";
    _cardSprite.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
    _cardSprite.drawString(name.c_str(), 48, 10);

    // Percentual de uso
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", stats.gpuUsage);
    _cardSprite.setFont(&fonts::Font4);
    _cardSprite.setTextColor(COLOR_TEXT_WHITE, COLOR_CARD_BG);
    _cardSprite.drawString(buf, 8, 27);

    // Temperatura
    if (stats.gpuTemp > 0.0f) {
      snprintf(buf, sizeof(buf), "%.0f'C", stats.gpuTemp);
      _cardSprite.setFont(&fonts::Font2);
      _cardSprite.setTextColor(getTempColor(stats.gpuTemp), COLOR_CARD_BG);
      _cardSprite.drawRightString(buf, 143, 30);
    }

    // Informação secundária
    _cardSprite.setFont(&fonts::Font0);
    _cardSprite.setTextSize(1);
    _cardSprite.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
    _cardSprite.drawString("Placa Grafica", 8, 58);

    // Barra de progresso
    drawProgressBar(_cardSprite, 8, 76, 135, 10, stats.gpuUsage, getLoadColor(stats.gpuUsage));

    _cardSprite.pushSprite(&_lcd, posX, posY);
  }

  // =======================================================================
  //  CARTÃO: MEMÓRIA RAM
  // =======================================================================
  void renderRamCard(const SystemStats& stats, int posX, int posY) {
    _cardSprite.fillSprite(COLOR_CARD_BG);
    _cardSprite.drawRoundRect(0, 0, 151, 98, 6, COLOR_CARD_BORDER);

    // Top Header com etiqueta colorida
    _cardSprite.fillRoundRect(8, 7, 34, 14, 3, COLOR_ACCENT_RAM);
    _cardSprite.setTextColor(COLOR_BG, COLOR_ACCENT_RAM);
    _cardSprite.setFont(&fonts::Font0);
    _cardSprite.setTextSize(1);
    _cardSprite.drawString("RAM", 14, 10);

    _cardSprite.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
    _cardSprite.drawString("Memoria", 48, 10);

    // Percentual
    char buf[20];
    snprintf(buf, sizeof(buf), "%.0f%%", stats.ramPercent);
    _cardSprite.setFont(&fonts::Font4);
    _cardSprite.setTextColor(COLOR_TEXT_WHITE, COLOR_CARD_BG);
    _cardSprite.drawString(buf, 8, 27);

    // Usado / Total (GB)
    snprintf(buf, sizeof(buf), "%.1f/%.1fGB", stats.ramUsed, stats.ramTotal);
    _cardSprite.setFont(&fonts::Font0);
    _cardSprite.setTextSize(1);
    _cardSprite.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
    _cardSprite.drawRightString(buf, 143, 36);

    _cardSprite.drawString("Em Uso / Total", 8, 58);

    // Barra de progresso
    drawProgressBar(_cardSprite, 8, 76, 135, 10, stats.ramPercent, getLoadColor(stats.ramPercent));

    _cardSprite.pushSprite(&_lcd, posX, posY);
  }

  // =======================================================================
  //  CARTÃO: STORAGE / DISCO / SSD
  // =======================================================================
  void renderStorageCard(const SystemStats& stats, int posX, int posY) {
    _cardSprite.fillSprite(COLOR_CARD_BG);
    _cardSprite.drawRoundRect(0, 0, 151, 98, 6, COLOR_CARD_BORDER);

    // Top Header com etiqueta colorida
    _cardSprite.fillRoundRect(8, 7, 44, 14, 3, COLOR_ACCENT_DISK);
    _cardSprite.setTextColor(COLOR_BG, COLOR_ACCENT_DISK);
    _cardSprite.setFont(&fonts::Font0);
    _cardSprite.setTextSize(1);
    _cardSprite.drawString("SSD/DISCO", 11, 10);

    // Percentual de ocupação
    char buf[20];
    snprintf(buf, sizeof(buf), "%.0f%%", stats.diskPercent);
    _cardSprite.setFont(&fonts::Font4);
    _cardSprite.setTextColor(COLOR_TEXT_WHITE, COLOR_CARD_BG);
    _cardSprite.drawString(buf, 8, 27);

    // Temperatura do SSD NVMe
    if (stats.ssdTemp > 0.0f) {
      snprintf(buf, sizeof(buf), "%.0f'C", stats.ssdTemp);
      _cardSprite.setFont(&fonts::Font2);
      _cardSprite.setTextColor(getTempColor(stats.ssdTemp), COLOR_CARD_BG);
      _cardSprite.drawRightString(buf, 143, 30);
    }

    // Usado / Total em GB
    _cardSprite.setFont(&fonts::Font0);
    _cardSprite.setTextSize(1);
    _cardSprite.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
    snprintf(buf, sizeof(buf), "%.0f / %.0f GB", stats.diskUsed, stats.diskTotal);
    _cardSprite.drawString(buf, 8, 58);

    // Barra de progresso
    drawProgressBar(_cardSprite, 8, 76, 135, 10, stats.diskPercent, getLoadColor(stats.diskPercent));

    _cardSprite.pushSprite(&_lcd, posX, posY);
  }

  // =======================================================================
  //  DESENHO DO PAINEL PRINCIPAL (DASHBOARD 2x2)
  // =======================================================================
  void drawDashboard(const SystemStats& stats) {
    drawHeader(stats);

    // Posições dos 4 cartões
    const int col0 = 6;
    const int col1 = 163;
    const int row0 = 28;
    const int row1 = 132;

    renderCpuCard(stats, col0, row0);
    renderGpuCard(stats, col1, row0);
    renderRamCard(stats, col0, row1);
    renderStorageCard(stats, col1, row1);
  }

  // =======================================================================
  //  TELA DE DETALHES (VISTA ALTERNATIVA)
  // =======================================================================
  void drawDetailsView(const SystemStats& stats) {
    drawHeader(stats);

    _lcd.fillRoundRect(10, 30, 300, 202, 8, COLOR_CARD_BG);
    _lcd.drawRoundRect(10, 30, 300, 202, 8, COLOR_CARD_BORDER);

    _lcd.setTextColor(COLOR_TEXT_WHITE, COLOR_CARD_BG);
    _lcd.setFont(&fonts::Font2);
    _lcd.drawString("Informacoes do Sistema", 20, 38);

    _lcd.setFont(&fonts::Font0);
    _lcd.setTextSize(1);
    _lcd.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
    _lcd.drawString("Toque na tela para voltar ao Dashboard", 20, 56);
    _lcd.drawFastHLine(20, 68, 280, COLOR_CARD_BORDER);

    int y = 76;
    auto drawDetailLine = [&](const char* label, const String& val, uint16_t valColor = COLOR_TEXT_WHITE) {
      _lcd.setTextColor(COLOR_TEXT_MUTED, COLOR_CARD_BG);
      _lcd.drawString(label, 20, y);
      _lcd.setTextColor(valColor, COLOR_CARD_BG);
      _lcd.drawRightString(val.c_str(), 300, y);
      y += 18;
    };

    drawDetailLine("Processador (CPU):", stats.cpuName);
    drawDetailLine("Carga CPU / Temp:", String(stats.cpuUsage, 1) + "%  |  " + String(stats.cpuTemp, 1) + "'C", getLoadColor(stats.cpuUsage));
    drawDetailLine("Placa de Video (GPU):", stats.gpuName);
    drawDetailLine("Carga GPU / Temp:", String(stats.gpuUsage) + "%  |  " + String(stats.gpuTemp, 1) + "'C", getLoadColor(stats.gpuUsage));
    drawDetailLine("Memoria RAM:", String(stats.ramUsed, 1) + " GB / " + String(stats.ramTotal, 1) + " GB (" + String(stats.ramPercent, 0) + "%)");
    drawDetailLine("Armazenamento:", String(stats.diskUsed, 0) + " GB / " + String(stats.diskTotal, 0) + " GB  |  SSD: " + String(stats.ssdTemp, 0) + "'C");
    drawDetailLine("Conexao:", stats.isUsb ? "USB Serial (115200)" : ("Wi-Fi IP: " + stats.wifiIp));
    drawDetailLine("Brilho da Tela:", String(getBrightnessPercent()) + "% (Toque p/ alternar)");
  }
};
