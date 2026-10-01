# 🖥️ Monitor Linux para ESP32 CYD (Cheap Yellow Display)

Sistema de monitoramento de hardware em tempo real para placas **ESP32 CYD (ESP32-2432S028 / ESP32-2432S028R)** com tela colorida de 2.8" (320x240) e touchscreen, inspirado e compatível com o [Monitor Linux](https://github.com/neneHC/esp_Monitor).

O sistema monitora CPU, GPU (AMD/NVIDIA/Intel), Memória RAM, Armazenamento e Temperaturas NVMe/SSD, transmitindo os dados em tempo real para a tela do microcontrolador com taxa de atualização de 1 segundo e 0% de cintilação (flicker).

---

## ✨ Características Principais

* **Visual Dark Theme Moderno:** Interface escura estilizada com bordas sutis e 4 cartões de métricas (layout 2x2):
  * **CPU:** Modelo do processador (AMD Ryzen / Intel Core), Carga em %, Clock em GHz e Temperatura em °C.
  * **GPU:** Modelo da placa de vídeo (AMD Radeon / NVIDIA / Intel), Uso da GPU em % e Temperatura em °C.
  * **RAM:** Memória usada e total em GB, e percentual ocupado.
  * **DISCO / SSD:** Ocupação da partição raiz em GB e %, além da temperatura do sensor NVMe / SSD.
* **Barras de Progresso Dinâmicas com Transição de Cores:**
  * 🟢 **Verde** (< 60%): Operação normal
  * 🟠 **Laranja / Âmbar** (60% - 85%): Carga média / Atenção
  * 🔴 **Vermelho** (> 85%): Carga alta / Alerta
* **Zero Flicker (60 FPS):** Renderização via sprites double-buffered com a biblioteca de alto desempenho `LovyanGFX`.
* **Interatividade via Touchscreen:**
  * **Toque na metade superior:** Alterna entre o **Dashboard 2x2** e a tela de **Detalhes Técnicos do Sistema**.
  * **Toque na metade inferior:** Cicla o brilho da tela (100% ➔ 70% ➔ 35% ➔ 10%).
  * **Toque longo (3 segundos):** Alterna a inversão de cores (útil caso seu lote do CYD seja ST7789/BGR) e salva na memória permanente NVS (Flash).
* **LED RGB Onboard Integrado:**
  * O LED multicolorido da placa CYD reflete a saúde do computador em tempo real:
    * 🟢 Verde: Carga normal
    * 🟡 Amarelo/Laranja: Uso moderado
    * 🔴 Vermelho: Uso intenso ou temperatura elevada
    * 🔵 Azul pulsante: Aguardando conexão com o PC
* **Modo Duplo de Conexão:**
  * **Cabo USB (Serial 115200 baud):** Plug-and-play imediato, sem latência de rede e sem precisar de Wi-Fi.
  * **Wi-Fi (HTTP Polling):** Permite usar a placa como display de mesa alimentado por qualquer carregador USB.

---

## 📁 Estrutura do Projeto

```
esp_monitor/
├── platformio.ini           # Configuração do PlatformIO (LovyanGFX + ArduinoJson)
├── build_and_upload.sh      # Script para compilar e gravar na placa
├── run_agent.sh             # Script para rodar o agente Python
├── include/
│   ├── config.h             # Configurações de Wi-Fi, pinagem e brilho
│   ├── display_setup.h      # Driver e barramentos SPI do CYD (ST7789/ILI9341)
│   ├── led_controller.h     # Controle do LED RGB onboard
│   └── ui_dashboard.h       # Engine gráfica de renderização dos cards
├── src/
│   └── main.cpp             # Loop principal, parsing JSON e touch
└── agent/
    ├── monitor_agent.py     # Agente de coleta Linux (Serial + HTTP)
    ├── requirements.txt     # Dependências (psutil, pyserial)
    ├── install_service.sh   # Instalador do serviço de boot (systemd)
    └── esp-monitor.service  # Arquivo de unidade systemd
```

---

## 🚀 Como Usar

### 1. Pré-requisitos no Linux

Certifique-se de ter o Python 3 e o PlatformIO instalados:

```bash
# Permissão para acessar a porta serial sem root
sudo usermod -aG dialout $USER

# Instalação do PlatformIO Core (caso não tenha)
pip install -U platformio
```

---

### 2. Gravar o Firmware na Placa ESP32 CYD

Conecte a placa CYD à porta USB do seu computador e execute:

```bash
./build_and_upload.sh
```

O script cuidará de pausar qualquer serviço serial ativo, compilar o código e gravar o firmware em alta velocidade (921600 baud) na porta `/dev/ttyUSB0`.

---

### 3. Iniciar o Envio de Dados do PC

Para testar e rodar o agente manualmente no terminal:

```bash
./run_agent.sh
```

O script criará o ambiente virtual Python automaticamente, instalará as dependências (`psutil`, `pyserial`), detectará a porta serial da placa e iniciará o envio das métricas a cada 1 segundo.

---

### 4. Iniciar Automaticamente no Boot do Linux (Serviço em Segundo Plano)

Se desejar que o monitor funcione silenciosamente em segundo plano sempre que você ligar o computador ou fizer login:

```bash
./agent/install_service.sh
```

Comandos úteis para gerenciar o serviço:
* **Ver status:** `systemctl --user status esp-monitor`
* **Ver logs em tempo real:** `journalctl --user -u esp-monitor -f`
* **Parar o serviço:** `systemctl --user stop esp-monitor`
* **Reiniciar o serviço:** `systemctl --user restart esp-monitor`

---

## 📡 Configuração Wi-Fi (Opcional)

Se você preferir usar a placa como um display sem fio (ligada a uma fonte USB na mesa, sem conexão direta com o PC):

1. Abra o arquivo [`include/config.h`](include/config.h).
2. Configure seu Wi-Fi e o IP do seu computador na rede local:
   ```cpp
   #define WIFI_SSID       "SuaRedeWiFi"
   #define WIFI_PASSWORD   "SuaSenha"
   #define SERVER_HOST     "192.168.1.100"  // IP do seu PC Linux
   #define SERVER_PORT     5005
   ```
3. Regrave o firmware executando `./build_and_upload.sh`.
4. O agente `monitor_agent.py` já disponibiliza o endpoint HTTP `http://0.0.0.0:5005/api/stats` automaticamente.

---

## 🎯 Controles Touch do Display

| Gesto / Ação | Função |
| :--- | :--- |
| **Toque na metade superior** | Alterna entre o **Dashboard (2x2)** e a tela de **Detalhes Técnicos** |
| **Toque na metade inferior** | Ajusta o brilho da tela (100% ➔ 70% ➔ 35% ➔ 10%) |
| **Toque longo (3 segundos)** | Alterna e salva a inversão de cores na memória flash NVS |

---

## 🛠️ Variantes de Hardware CYD (Resolução de Problemas)

As placas "Cheap Yellow Display" possuem diferentes lotes de fabricação:

1. **Painel ST7789 (Padrão configurado neste projeto):**
   * Típico em placas revisadas (Dual-USB: USB-C + Micro-USB).
   * Se sua tela tiver bordas de ruído ou imagem espelhada ao usar ILI9341, o driver correto é o `Panel_ST7789`.
2. **Painel ILI9341 (Modelos mais antigos):**
   * Se o seu display usar o controlador ILI9341, altere em [`include/display_setup.h`](include/display_setup.h):
     ```cpp
     lgfx::Panel_ILI9341 _panel_instance;
     ```
3. **Cores Invertidas:**
   * Basta tocar e segurar o dedo na tela por 3 segundos para inverter as cores na hora sem precisar recompilar.

---

## 📄 Licença

Distribuído sob a licença MIT. Sinta-se livre para modificar e customizar para o seu setup!
