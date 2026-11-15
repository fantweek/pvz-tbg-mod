#!/usr/bin/env python3
"""Снимки меню для README. Рисует настоящий код меню (drawUi из src/ui.cpp) программным растеризатором на Linux:
Windows и игра не нужны. Нужны ziglang и Pillow; шрифт — DejaVu Sans (в игре меню использует Segoe UI, поэтому вид букв немного другой).

  python tools/screenshots/make_screenshots.py            # -> docs/screenshots/*.png
"""
import os, struct, subprocess, sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "docs", "screenshots")
BUILD = os.path.join(ROOT, "build")
FONTS = ["/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
         "/System/Library/Fonts/Supplemental/Arial.ttf"]
# имя файла, вкладка, номер замороженной сборки (-1 нет), прокрутка (пиксели при масштабе 1.5)
SHOTS = [("plants", 0, -1, 0), ("zombies", 1, -1, 0), ("level", 2, -1, 0), ("level-conveyor", 2, -1, 900),
         ("game", 3, -1, 0), ("builds", 4, -1, 430), ("freeze", 0, 4, 0)]


def main():
    font = next((f for f in FONTS if os.path.exists(f)), None)
    if not font:
        sys.exit("Не найден шрифт с кириллицей (DejaVu Sans / Segoe UI)")
    os.makedirs(BUILD, exist_ok=True); os.makedirs(OUT, exist_ok=True)
    exe = os.path.join(BUILD, "shot" + (".exe" if os.name == "nt" else ""))
    imgui = os.path.join(ROOT, "third_party", "imgui")
    cmd = [sys.executable, "-m", "ziglang", "c++", "-target", "x86-linux-musl", "-static", "-std=c++17", "-fno-rtti",
           "-I" + os.path.join(ROOT, "src"), "-I" + os.path.join(ROOT, "tests"), "-I" + imgui,
           os.path.join(HERE, "shot.cpp")] + [os.path.join(imgui, f) for f in ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")] + ["-o", exe]
    subprocess.run(cmd, check=True)
    for name, tab, frozen, scroll in SHOTS:
        raw = os.path.join(BUILD, name + ".rgba")
        subprocess.run([exe, str(tab), str(frozen), str(scroll), raw, font, "1.5"], check=True)
        d = open(raw, "rb").read(); w, h = struct.unpack("ii", d[:8])
        im = Image.frombytes("RGBA", (w, h), d[8:]).convert("RGB")
        im = im.resize((w * 2 // 3, h * 2 // 3), Image.LANCZOS)         # 1120x800 — как окно в игре при масштабе 100%
        im.save(os.path.join(OUT, name + ".png"), optimize=True)
        os.remove(raw); print("готово:", name)


if __name__ == "__main__":
    main()
