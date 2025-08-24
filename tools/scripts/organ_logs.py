#!/usr/bin/env python3
import os
import shutil
from datetime import datetime
from tools.lib.logreader import LogReader

BASE_DIR = "/home/sato/commalogs/realdata"

def get_route_key(dirname):
    """Extrai o route_id e car_id do nome da pasta"""
    parts = dirname.split("--")
    if len(parts) != 3:
        return None
    return f"{parts[0]}--{parts[1]}"  # route_id + car_id

def get_first_segment(logs_in_route):
    """Retorna o nome do primeiro segmento de uma rota"""
    # Segmentos são sempre numerados no final do nome da pasta
    return sorted(logs_in_route, key=lambda x: int(x.split("--")[2]))[0]

# Agrupa diretórios por rota
routes = {}
for dirname in sorted(os.listdir(BASE_DIR)):
    dirpath = os.path.join(BASE_DIR, dirname)
    if not os.path.isdir(dirpath):
        continue
    if dirname in ("boot", "crash"):
        continue

    route_key = get_route_key(dirname)
    if route_key is None:
        continue

    routes.setdefault(route_key, []).append(dirname)

# Processa cada rota encontrada
for route_key, segments in routes.items():
    first_segment = get_first_segment(segments)
    rlog_path = os.path.join(BASE_DIR, first_segment, "rlog.zst")

    if not os.path.exists(rlog_path):
        print(f"⚠️  rlog.zst não encontrado: {first_segment}")
        continue

    # Lê o primeiro gpsLocationExternal do primeiro segmento
    try:
        log = LogReader(rlog_path)
        gps_wall_time = None
        for m in log:
            if m.which() == "gpsLocationExternal" and m.gpsLocationExternal.flags % 2 == 1:
                gps_wall_time = m.gpsLocationExternal.unixTimestampMillis
                break

        if gps_wall_time is None:
            print(f"⚠️  Nenhum GPS encontrado em {first_segment}, pulando...")
            continue

        # Monta nome final do diretório
        dt = datetime.fromtimestamp(gps_wall_time / 1000)
        route_folder_name = f"{dt.strftime('%Y-%m-%d__%H-%M')}__{route_key}"
        target_dir = os.path.join(BASE_DIR, route_folder_name)
        os.makedirs(target_dir, exist_ok=True)

        # Move todos os segmentos dessa rota para o diretório final
        for seg in segments:
            old_path = os.path.join(BASE_DIR, seg)
            new_path = os.path.join(target_dir, seg)
            if not os.path.exists(new_path):
                shutil.move(old_path, new_path)
                print(f"✅ Movido: {seg} → {route_folder_name}")
            else:
                print(f"⚠️  Já existe: {seg}, pulando...")

    except Exception as e:
        print(f"❌ Erro ao processar {first_segment}: {e}")
