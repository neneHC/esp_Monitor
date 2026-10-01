#!/usr/bin/env python3
"""
Monitor Linux - Agente para ESP32 CYD (Cheap Yellow Display)
Transmite métricas do sistema em tempo real via Cabo USB (Serial) e Servidor HTTP (Wi-Fi).
Compatível com AMD Ryzen, AMD Radeon, Intel, NVIDIA e sensores NVMe.
"""

import os
import sys
import time
import glob
import json
import socket
import shutil
import argparse
import threading
import subprocess
from http.server import HTTPServer, BaseHTTPRequestHandler

# Importações de terceiros (psutil e serial)
try:
    import psutil
except ImportError:
    print("[ERRO] Biblioteca 'psutil' não encontrada! Instale com: pip install psutil")
    sys.exit(1)

try:
    import serial
    import serial.tools.list_ports
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False
    print("[AVISO] Biblioteca 'pyserial' não encontrada. Modo serial desabilitado.")

# =========================================================================
#  COLETA DE MÉTRICAS DO SISTEMA
# =========================================================================

class SystemMetricsCollector:
    def __init__(self):
        self.cached_cpu_name = None
        self.cached_gpu_name = None
        # Inicializa o cálculo de CPU para evitar 0.0% na primeira chamada
        psutil.cpu_percent(interval=None)

    def get_cpu_name(self):
        if self.cached_cpu_name:
            return self.cached_cpu_name
        try:
            with open('/proc/cpuinfo', 'r') as f:
                for line in f:
                    if 'model name' in line:
                        full = line.split(':')[1].strip()
                        # Limpa sufixos redundantes para caber no display
                        clean = full.replace('AMD ', '').replace('Intel(R) Core(TM) ', '')
                        clean = clean.replace(' with Radeon Graphics', '').replace(' Processor', '')
                        self.cached_cpu_name = clean
                        return clean
        except Exception:
            pass
        self.cached_cpu_name = "CPU Linux"
        return self.cached_cpu_name

    def get_cpu_temp(self):
        try:
            temps = psutil.sensors_temperatures()
            for key in ['k10temp', 'coretemp', 'zenpower', 'cpu_thermal']:
                if key in temps and temps[key]:
                    return round(temps[key][0].current, 1)
        except Exception:
            pass
        return 0.0

    def get_cpu_freq_ghz(self):
        try:
            freq = psutil.cpu_freq()
            if freq and freq.current:
                return round(freq.current / 1000.0, 2)
        except Exception:
            pass
        return 0.0

    def get_gpu_name(self):
        if self.cached_gpu_name:
            return self.cached_gpu_name
        try:
            res = subprocess.run(['lspci'], capture_output=True, text=True, timeout=2)
            for line in res.stdout.splitlines():
                if any(x in line for x in ['VGA compatible controller', '3D controller', 'Display controller']):
                    parts = line.split(': ')
                    if len(parts) > 1:
                        full = parts[1].strip()
                        # Extrai o nome limpo entre colchetes caso exista
                        if '[' in full and ']' in full:
                            clean = full.split('[')[-1].split(']')[0]
                        else:
                            clean = full
                        # Encurta modelos com barras longas (ex: Radeon RX 6600/6600 XT/6600M -> Radeon RX 6600)
                        if '/' in clean:
                            clean = clean.split('/')[0]
                        self.cached_gpu_name = clean
                        return clean
        except Exception:
            pass
        self.cached_gpu_name = "GPU"
        return self.cached_gpu_name

    def get_gpu_info(self):
        gpu_name = self.get_gpu_name()
        usage = 0
        temp = 0.0

        # 1. Estratégia NVIDIA
        if shutil.which('nvidia-smi'):
            try:
                out = subprocess.check_output(
                    ['nvidia-smi', '--query-gpu=utilization.gpu,temperature.gpu', '--format=csv,noheader,nounits'],
                    encoding='utf-8', timeout=1
                )
                u, t = out.strip().split(', ')
                return {'name': gpu_name, 'usage': int(u), 'temp': float(t)}
            except Exception:
                pass

        # 2. Estratégia AMD / Intel DRM (lendo de /sys/class/drm)
        for path in glob.glob('/sys/class/drm/card*/device/gpu_busy_percent'):
            try:
                with open(path, 'r') as f:
                    val = int(f.read().strip())
                    if val > usage:
                        usage = val
            except Exception:
                continue

        # Temperatura AMD GPU
        try:
            temps = psutil.sensors_temperatures()
            if 'amdgpu' in temps and temps['amdgpu']:
                for entry in temps['amdgpu']:
                    if entry.current > temp:
                        temp = round(entry.current, 1)
        except Exception:
            pass

        return {'name': gpu_name, 'usage': usage, 'temp': temp}

    def get_ram_info(self):
        vm = psutil.virtual_memory()
        return {
            'used': round(vm.used / (1024**3), 1),
            'total': round(vm.total / (1024**3), 1),
            'pct': round(vm.percent, 1)
        }

    def get_disk_info(self):
        try:
            root = psutil.disk_usage('/')
            used_gb = round(root.used / (1024**3), 1)
            total_gb = round(root.total / (1024**3), 1)
            pct = round(root.percent, 1)
        except Exception:
            used_gb, total_gb, pct = 0.0, 0.0, 0.0

        ssd_temp = 0.0
        try:
            temps = psutil.sensors_temperatures()
            for key, entries in temps.items():
                if any(x in key.lower() for x in ['nvme', 'composite', 'drivetemp']):
                    if entries:
                        ssd_temp = round(entries[0].current, 1)
                        break
        except Exception:
            pass

        return {
            'used': used_gb,
            'total': total_gb,
            'pct': pct,
            'temp': ssd_temp
        }

    def collect(self):
        cpu_usage = round(psutil.cpu_percent(interval=None), 1)
        gpu = self.get_gpu_info()
        ram = self.get_ram_info()
        disk = self.get_disk_info()

        return {
            'host': socket.gethostname(),
            'cpu': {
                'name': self.get_cpu_name(),
                'usage': cpu_usage,
                'temp': self.get_cpu_temp(),
                'freq': self.get_cpu_freq_ghz()
            },
            'gpu': gpu,
            'ram': ram,
            'disk': disk
        }

# =========================================================================
#  TRANSMISSÃO SERIAL USB
# =========================================================================

def auto_detect_serial_port():
    if not SERIAL_AVAILABLE:
        return None
    ports = serial.tools.list_ports.comports()
    for p in ports:
        # CYD costuma usar CH340 (1a86:7523) ou CP2102
        if any(x in p.description.lower() for x in ['ch340', 'cp210', 'usb-serial', 'uart', 'esp32']):
            return p.device
        if 'ttyUSB' in p.device or 'ttyACM' in p.device:
            return p.device
    return None

class SerialSenderThread(threading.Thread):
    def __init__(self, collector, port=None, baud=115200, interval=1.0):
        super().__init__(daemon=True)
        self.collector = collector
        self.preferred_port = port
        self.baud = baud
        self.interval = interval
        self.running = True
        self.current_serial = None

    def run(self):
        print(f"[SERIAL] Iniciando transmissor Serial...")
        while self.running:
            target_port = self.preferred_port or auto_detect_serial_port()

            if not target_port:
                # Nenhum dispositivo encontrado
                time.sleep(2)
                continue

            try:
                print(f"[SERIAL] Conectando a {target_port} ({self.baud} baud)...")
                self.current_serial = serial.Serial(target_port, self.baud, timeout=1)
                print(f"[SERIAL] Conexao estabelecida com sucesso em {target_port}!")

                while self.running:
                    data = self.collector.collect()
                    line = json.dumps(data, separators=(',', ':')) + '\n'
                    self.current_serial.write(line.encode('utf-8'))
                    self.current_serial.flush()
                    time.sleep(self.interval)

            except serial.SerialException as e:
                print(f"[SERIAL] Desconectado ou erro na porta ({e}). Tentando reconectar...")
                if self.current_serial:
                    try:
                        self.current_serial.close()
                    except Exception:
                        pass
                    self.current_serial = None
                time.sleep(2)
            except Exception as e:
                print(f"[SERIAL] Erro inesperado: {e}")
                time.sleep(2)

# =========================================================================
#  SERVIDOR HTTP (WI-FI)
# =========================================================================

class MetricsHttpHandler(BaseHTTPRequestHandler):
    collector = None

    def do_GET(self):
        if self.path == '/api/stats':
            data = self.collector.collect()
            body = json.dumps(data).encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(body)))
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(body)
        elif self.path == '/api/ping':
            body = b'{"status":"ok"}'
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(body)
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        # Silencia logs repetitivos de polling
        pass

def start_http_server(collector, host='0.0.0.0', port=5005):
    MetricsHttpHandler.collector = collector
    server = HTTPServer((host, port), MetricsHttpHandler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    print(f"[HTTP] Servidor de métricas ativo em http://{host}:{port}/api/stats")
    return server

# =========================================================================
#  PONTO DE ENTRADA PRINCIPAL
# =========================================================================

def main():
    parser = argparse.ArgumentParser(description="Monitor Linux - Agente para ESP32 CYD")
    parser.add_argument('--port', type=str, default=None, help="Porta serial manual (ex: /dev/ttyUSB0)")
    parser.add_argument('--baud', type=int, default=115200, help="Taxa de transmissao serial (padrao: 115200)")
    parser.add_argument('--interval', type=float, default=1.0, help="Intervalo de atualizacao em segundos (padrao: 1.0)")
    parser.add_argument('--http-port', type=int, default=5005, help="Porta do servidor HTTP (padrao: 5005, 0 para desabilitar)")
    parser.add_argument('--no-serial', action='store_true', help="Desabilita envio via Serial")
    args = parser.parse_args()

    collector = SystemMetricsCollector()

    print("=" * 60)
    print("   MONITOR LINUX - AGENTE ESP32 CYD")
    print("=" * 60)
    print(f" Máquina: {socket.gethostname()}")
    print(f" Processador: {collector.get_cpu_name()}")
    print(f" Placa de Vídeo: {collector.get_gpu_name()}")
    print("=" * 60)

    # Inicia Servidor HTTP se habilitado
    if args.http_port > 0:
        start_http_server(collector, port=args.http_port)

    # Inicia Transmissor Serial se habilitado
    if not args.no_serial and SERIAL_AVAILABLE:
        serial_thread = SerialSenderThread(
            collector,
            port=args.port,
            baud=args.baud,
            interval=args.interval
        )
        serial_thread.start()

    print("\n[OK] Agente em execucao! Pressione Ctrl+C para encerrar.\n")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n[INFO] Encerrando agente...")
        sys.exit(0)

if __name__ == '__main__':
    main()
