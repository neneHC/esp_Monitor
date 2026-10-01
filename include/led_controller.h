#pragma once
#include <Arduino.h>
#include "config.h"

class LEDController {
private:
  bool _enabled;
  uint8_t _current_r;
  uint8_t _current_g;
  uint8_t _current_b;
  unsigned long _last_blink;
  bool _blink_state;

public:
  LEDController() : _enabled(true), _current_r(0), _current_g(0), _current_b(0), _last_blink(0), _blink_state(false) {}

  void begin() {
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);
    off();
  }

  void setEnabled(bool en) {
    _enabled = en;
    if (!_enabled) off();
  }

  bool isEnabled() const {
    return _enabled;
  }

  void toggle() {
    setEnabled(!_enabled);
  }

  void setColor(bool r, bool g, bool b) {
    if (!_enabled) {
      digitalWrite(PIN_LED_RED, HIGH);
      digitalWrite(PIN_LED_GREEN, HIGH);
      digitalWrite(PIN_LED_BLUE, HIGH);
      return;
    }
    // Active LOW: LOW acende, HIGH apaga
    digitalWrite(PIN_LED_RED, r ? LOW : HIGH);
    digitalWrite(PIN_LED_GREEN, g ? LOW : HIGH);
    digitalWrite(PIN_LED_BLUE, b ? LOW : HIGH);
  }

  void off() {
    digitalWrite(PIN_LED_RED, HIGH);
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_BLUE, HIGH);
  }

  void update(float max_load, float max_temp, bool connected) {
    if (!_enabled) return;

    if (!connected) {
      // Pisca em azul suave quando desconectado
      unsigned long now = millis();
      if (now - _last_blink > 500) {
        _last_blink = now;
        _blink_state = !_blink_state;
        setColor(false, false, _blink_state);
      }
      return;
    }

    // Se conectado, a cor reflete a saúde do sistema Linux
    if (max_load >= 85.0f || max_temp >= 80.0f) {
      // Vermelho (Alerta / Carga alta)
      setColor(true, false, false);
    } else if (max_load >= 60.0f || max_temp >= 70.0f) {
      // Amarelo / Âmbar (Vermelho + Verde)
      setColor(true, true, false);
    } else {
      // Verde (Normal / Tranquilo)
      setColor(false, true, false);
    }
  }
};
