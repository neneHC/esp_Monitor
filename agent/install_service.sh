#!/bin/bash
# =========================================================================
#  Instala o agente como um serviço systemd de usuário (inicia no boot)
# =========================================================================

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
SERVICE_DIR="$HOME/.config/systemd/user"

mkdir -p "$SERVICE_DIR"
cp "$DIR/esp-monitor.service" "$SERVICE_DIR/esp-monitor.service"

systemctl --user daemon-reload
systemctl --user enable esp-monitor.service
systemctl --user restart esp-monitor.service

echo "============================================================"
echo "✅ Serviço instalado e iniciado com sucesso!"
echo "Status do serviço:"
systemctl --user status esp-monitor.service --no-pager
echo "============================================================"
echo "Comandos úteis:"
echo "  Parar serviço:      systemctl --user stop esp-monitor"
echo "  Reiniciar serviço:  systemctl --user restart esp-monitor"
echo "  Ver logs em tempo real: journalctl --user -u esp-monitor -f"
