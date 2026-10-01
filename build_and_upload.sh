#!/bin/bash
# =========================================================================
#  Script para compilar e gravar (flash) o firmware na placa ESP32 CYD
# =========================================================================

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd "$DIR"

echo "============================================================"
echo "   Compilando e Gravando Firmware na Placa ESP32 CYD"
echo "============================================================"

# Se o serviço estiver rodando, pausa temporariamente para liberar /dev/ttyUSB0
WAS_RUNNING=0
if systemctl --user is-active --quiet esp-monitor 2>/dev/null; then
    echo "⏸️  Pausando serviço esp-monitor para liberar a porta serial..."
    systemctl --user stop esp-monitor
    WAS_RUNNING=1
fi

# Procura o comando pio ou platformio
PIO_CMD=""
if command -v pio &> /dev/null; then
    PIO_CMD="pio"
elif command -v platformio &> /dev/null; then
    PIO_CMD="platformio"
elif [ -f "$HOME/.local/bin/pio" ]; then
    PIO_CMD="$HOME/.local/bin/pio"
else
    echo "❌ Erro: PlatformIO não encontrado no PATH!"
    exit 1
fi

echo "📦 Utilizando PlatformIO: $PIO_CMD"
$PIO_CMD run -t upload "$@"

echo ""
echo "✅ Firmware gravado com sucesso na placa CYD!"

# Reinicia o serviço caso estivesse rodando
if [ "$WAS_RUNNING" -eq 1 ]; then
    echo "▶️  Reiniciando serviço esp-monitor..."
    systemctl --user start esp-monitor
    echo "🚀 Serviço reiniciado e transmitindo dados!"
else
    echo "👉 Execute './run_agent.sh' para iniciar o envio de dados via USB!"
fi
