#!/usr/bin/env python3
import sys
import datetime
import bisect
import subprocess
from openpilot.tools.lib.logreader import LogReader

BRT = datetime.timezone(datetime.timedelta(hours=-3))

def to_srt_time(seconds: float) -> str:
    """Converte segundos float -> formato SRT hh:mm:ss,ms"""
    td = datetime.timedelta(seconds=seconds)
    h, rem = divmod(td.seconds, 3600)
    m, s = divmod(rem, 60)
    ms = int(td.microseconds / 1000)
    return f"{h:02}:{m:02}:{s:02},{ms:03}"

def get_fps_and_duration(video_path: str):
    if video_path.endswith(".hevc"):
        fps = 20.0   # câmeras do comma: ~20 fps
        # descobre duração decodificando frames
        nframes_str = subprocess.check_output([
            "ffprobe", "-v", "0",
            "-count_frames",
            "-select_streams", "v:0",
            "-show_entries", "stream=nb_read_frames",
            "-of", "csv=p=0",
            video_path
        ]).decode().strip()
        nframes = int(nframes_str)
        duration = nframes / fps
        return fps, duration

    # fallback para arquivos com metadados normais (.ts, .mp4)
    fps_str = subprocess.check_output([
        "ffprobe", "-v", "0", "-select_streams", "v:0",
        "-show_entries", "stream=r_frame_rate",
        "-of", "csv=p=0", video_path
    ]).decode().strip()
    fps_line = [l for l in fps_str.splitlines() if l.strip()][0]
    num, den = map(int, fps_line.split('/'))
    fps = num / den if den != 0 else float(num)

    duration_str = subprocess.check_output([
        "ffprobe", "-v", "0",
        "-show_entries", "format=duration",
        "-of", "csv=p=0", video_path
    ]).decode().strip()
    duration = float(duration_str)
    return fps, duration

def main(rlog_path, video_path, srt_path):
    # --- ler CarState e GPS do rlog ---
    car_times, vEgos = [], []
    gps_times, gps_data = [], []

    for evt in LogReader(rlog_path):
        if evt.which() == "carState":
            car_times.append(evt.logMonoTime)
            vEgos.append(evt.carState.vEgo)
        elif evt.which() == "gpsLocationExternal":
            gps_times.append(evt.logMonoTime)
            gps_data.append((
                evt.gpsLocationExternal.latitude,
                evt.gpsLocationExternal.longitude,
                evt.gpsLocationExternal.unixTimestampMillis
            ))

    if not car_times:
        print("Nenhum CarState encontrado no rlog!")
        return

    first_mono = car_times[0]
    fps, duration = get_fps_and_duration(video_path)
    nframes = int(duration * fps)

    srt_lines = []
    idx = 1

    for i in range(nframes):
        ts = first_mono + int(i * (1e9 / fps))

        # --- pegar vEgo ---
        j = bisect.bisect_left(car_times, ts)
        if j == 0:
            vEgo = vEgos[0]
        elif j >= len(car_times):
            vEgo = vEgos[-1]
        else:
            if abs(car_times[j] - ts) < abs(car_times[j-1] - ts):
                vEgo = vEgos[j]
            else:
                vEgo = vEgos[j-1]

        # --- pegar GPS ---
        if gps_times:
            k = bisect.bisect_left(gps_times, ts)
            if k == 0:
                lat, lon, uts = gps_data[0]
            elif k >= len(gps_times):
                lat, lon, uts = gps_data[-1]
            else:
                if abs(gps_times[k] - ts) < abs(gps_times[k-1] - ts):
                    lat, lon, uts = gps_data[k]
                else:
                    lat, lon, uts = gps_data[k-1]
            # converter timestamp Unix (ms) -> datetime UTC
            dt = datetime.datetime.fromtimestamp(uts / 1e3, tz=datetime.timezone.utc)
            dt = dt.astimezone(BRT)
            dt_str = dt.strftime("%Y-%m-%d %H:%M:%S")
        else:
            lat, lon, dt_str = None, None, ""

        # --- tempo relativo no vídeo ---
        rel_time = i / fps
        start = to_srt_time(rel_time)
        end = to_srt_time(rel_time + 1/fps)

        # --- montar legenda ---
        srt_lines.append(f"{idx}")
        srt_lines.append(f"{start} --> {end}")
        if dt_str:
            line = (f"<font color=\"yellow\">{dt_str}   {vEgo*3.6:.1f}km/h   Lat: {lat:.6f} Lon: {lon:.6f}")
        else:
            line = f"vEgo: {vEgo*3.6:.1f} km/h"

        srt_lines.append(line)
        srt_lines.append("")
        idx += 1

    with open(srt_path, "w", encoding="utf-8") as f:
        f.write("\n".join(srt_lines))

    print(f"[OK] Legenda gerada: {srt_path}")
    print("\nExemplo de comando para adicionar a legenda:\nffmpeg -i qcamera.ts -vf subtitles=vego.srt -c:a copy out.mp4\n")
    print("Exemplo de comando para adicionar áudio no fcamera.hevc:")
    print("ffmpeg -i fcamera.mp4 -i qcamera.ts -map 0:v:0 -map 1:a:0 -c:v libx264 -crf 28 -preset veryfast -c:a aac -b:a 96k final_comprimido.mp4\n")

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Uso: python3 rlog_to_srt.py <rlog.zst> <qcamera.ts> <out.srt>")
        sys.exit(1)
    main(sys.argv[1], sys.argv[2], sys.argv[3])
