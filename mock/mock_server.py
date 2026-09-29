#!/usr/bin/env python3
"""Эмулятор протокола ESP32-камеры — для разработки без железа.

Видео (VIDEO_PORT): [4 байта длины, big-endian][JPEG].
Control (CONTROL_PORT): JSON-команды по одной на строку, ack в ответ.
"""

import json
import os
import socket
import struct
import threading
import time
import glob

VIDEO_PORT = 3333
CONTROL_PORT = 3334
FPS = 15
SETTINGS = {
    "auto_exposure": 1,
    "exposure": 300,
    "auto_gain": 1,
    "gain": 0,
    "whitebal": 1,
    "jpeg_quality": 12,
}
FRAMES_DIR = os.path.join(os.path.dirname(__file__), "test_frames")


def load_frames():
    paths = sorted(glob.glob(os.path.join(FRAMES_DIR, "*.jpg")))
    frames = []
    for p in paths:
        with open(p, "rb") as f:
            frames.append(f.read())
    if frames:
        print(f"[mock] загружено {len(frames)} кадров из {FRAMES_DIR}")
        return frames

    try:
        from PIL import Image, ImageDraw
        import io

        print("[mock] test_frames пуста, генерирую синтетические кадры")
        frames = []
        w, h = 640, 480
        for i in range(30):
            img = Image.new("RGB", (w, h), (20, 20, 30))
            draw = ImageDraw.Draw(img)
            x = int((i / 30) * (w - 80))
            draw.rectangle([x, h // 2 - 40, x + 80, h // 2 + 40], fill=(220, 80, 60))
            for col in range(0, w, 4):
                shade = int(255 * col / w)
                draw.line([(col, 0), (col, 40)], fill=(shade, shade, shade))
            buf = io.BytesIO()
            img.save(buf, format="JPEG", quality=85)
            frames.append(buf.getvalue())
        return frames
    except ImportError:
        raise SystemExit(
            "[mock] Нужен Pillow для генерации тестовых кадров (или положите свои "
            f".jpg файлы в {FRAMES_DIR}).\n"
            "Установите: pip install pillow"
        )


def video_client_thread(conn, addr, frames):
    print(f"[mock][video] клиент подключился: {addr}")
    idx = 0
    period = 1.0 / FPS
    try:
        while True:
            t0 = time.time()
            frame = frames[idx % len(frames)]
            idx += 1
            header = struct.pack(">I", len(frame))
            conn.sendall(header + frame)
            dt = time.time() - t0
            if dt < period:
                time.sleep(period - dt)
    except (BrokenPipeError, ConnectionResetError):
        print(f"[mock][video] клиент отключился: {addr}")
    finally:
        conn.close()


def control_client_thread(conn, addr):
    print(f"[mock][control] клиент подключился: {addr}")
    buf = b""
    try:
        while True:
            data = conn.recv(4096)
            if not data:
                break
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                if not line.strip():
                    continue
                try:
                    cmd = json.loads(line.decode("utf-8"))
                    print(f"[mock][control] получена команда: {cmd}")
                    if cmd.get("cmd") == "get":
                        reply = json.dumps({"settings": SETTINGS}) + "\n"
                    else:
                        if cmd.get("cmd") == "set" and cmd.get("param") in SETTINGS:
                            SETTINGS[cmd["param"]] = cmd.get("value")
                        reply = json.dumps({"ack": True, "param": cmd.get("param"),
                                            "value": cmd.get("value")}) + "\n"
                except json.JSONDecodeError:
                    reply = json.dumps({"ack": False, "error": "invalid json"}) + "\n"
                conn.sendall(reply.encode("utf-8"))
    except (BrokenPipeError, ConnectionResetError):
        pass
    finally:
        print(f"[mock][control] клиент отключился: {addr}")
        conn.close()


def serve(port, handler, *extra_args):
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", port))
    srv.listen(5)
    print(f"[mock] слушаю порт {port}")
    while True:
        conn, addr = srv.accept()
        threading.Thread(target=handler, args=(conn, addr, *extra_args), daemon=True).start()


def main():
    frames = load_frames()
    t1 = threading.Thread(target=serve, args=(VIDEO_PORT, video_client_thread, frames), daemon=True)
    t2 = threading.Thread(target=serve, args=(CONTROL_PORT, control_client_thread), daemon=True)
    t1.start()
    t2.start()
    print("[mock] Ctrl+C для остановки")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n[mock] остановлено")


if __name__ == "__main__":
    main()
