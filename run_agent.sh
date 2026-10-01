#!/bin/bash
# =========================================================================
#  Script de inicialização do Monitor Linux para ESP32 CYD
# =========================================================================

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd "$DIR"

# Verifica ambiente virtual
if [ ! -d "venv" ]; then
    echo "⚙️  Criando ambiente virtual Python..."
    python3 -m venv venv
    venv/bin/pip install --upgrade pip
    venv/bin/pip install -r agent/requirements.txt
fi

echo "🚀 Iniciando Monitor Linux Agent..."
exec ./venv/bin/python3 agent/monitor_agent.py "$@"
