#pragma once
#include <Arduino.h>

// =========================================================================
//  CONFIGURAÇÕES DE CONEXÃO
// =========================================================================

// Configurações de Wi-Fi (Opcional - deixe vazio se usar apenas via cabo USB)
// Se preenchido, o monitor tentará conectar no Wi-Fi caso o cabo USB não envie dados.
#define WIFI_SSID           ""
#define WIFI_PASSWORD       ""

// Endereço IP e porta do PC Linux onde o monitor_agent.py está rodando
#define SERVER_HOST         "192.168.1.100"
#define SERVER_PORT         5005

// Timeout em milissegundos sem dados da Serial antes de tentar Wi-Fi (se configurado)
#define SERIAL_TIMEOUT_MS   3500

// Intervalo de requisição HTTP no modo Wi-Fi (em milissegundos)
#define HTTP_POLL_INTERVAL  1000

// Taxa de comunicação Serial (USB)
#define SERIAL_BAUD_RATE    115200

// =========================================================================
//  CONFIGURAÇÕES DE HARDWARE CYD (ESP32-2432S028)
// =========================================================================

// Pinos do LED RGB onboard (ativo em nível BAIXO / LOW)
#define PIN_LED_RED         4
#define PIN_LED_GREEN       16
#define PIN_LED_BLUE        17
#define LED_ACTIVE_LOW      true

// Pino do sensor de luminosidade LDR (analógico)
#define PIN_LDR             34

// Configurações do Display
#define SCREEN_WIDTH        320
#define SCREEN_HEIGHT       240
#define SCREEN_ROTATION     1   // 1 ou 3 para modo Paisagem (Landscape)

// Inversão e ordem de cores padrão (pode ser alternado dinamicamente com toque longo)
#define DEFAULT_COLOR_INVERT true
#define DEFAULT_RGB_ORDER    false

// Níveis de Brilho do Backlight (PWM 0 a 255)
#define BRIGHTNESS_HIGH     255
#define BRIGHTNESS_MED      170
#define BRIGHTNESS_LOW      80
#define BRIGHTNESS_DIM      25
