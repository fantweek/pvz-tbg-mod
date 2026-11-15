#!/usr/bin/env python3
"""Рисует иконку и баннер проекта (оригинальный рисунок, без графики игры). Нужен Pillow: pip install pillow.
Результат: assets/icon.ico, assets/icon-256.png, assets/banner.png.
"""
import math, os, sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets")
BG1, BG2 = (14, 22, 32), (28, 44, 62)
GREEN, GREEN_D, GREEN_L = (111, 207, 74), (58, 140, 46), (170, 240, 120)
SUN = (255, 205, 70)
WHITE = (236, 242, 247)

def font(size, bold=True):
    for p in ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" if bold else "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
              "C:/Windows/Fonts/segoeuib.ttf" if bold else "C:/Windows/Fonts/segoeui.ttf",
              "/System/Library/Fonts/Supplemental/Arial Bold.ttf"):
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()

def vgrad(w, h, c1, c2):
    im = Image.new("RGB", (w, h), c1)
    px = im.load()
    for y in range(h):
        t = y / max(1, h - 1)
        col = tuple(int(c1[i] + (c2[i] - c1[i]) * t) for i in range(3))
        for x in range(w):
            px[x, y] = col
    return im

def bezier(p0, p1, p2, n=40):
    return [((1-t)**2*p0[0] + 2*(1-t)*t*p1[0] + t*t*p2[0], (1-t)**2*p0[1] + 2*(1-t)*t*p1[1] + t*t*p2[1]) for t in [i/n for i in range(n+1)]]

def leaf(d, base, tip, width, fill, vein):
    """Лист-«линза» от base до tip; ширина width. Рисуется двумя дугами."""
    bx, by = base; tx, ty = tip
    dx, dy = tx - bx, ty - by
    L = math.hypot(dx, dy); nx, ny = -dy / L, dx / L
    up, down = [], []
    for i in range(41):
        t = i / 40
        w = math.sin(math.pi * t) ** 0.85 * width / 2
        cx, cy = bx + dx * t, by + dy * t
        up.append((cx + nx * w, cy + ny * w)); down.append((cx - nx * w, cy - ny * w))
    d.polygon(up + down[::-1], fill=fill)
    d.line([base, tip], fill=vein, width=max(1, int(width * 0.07)))

def icon(size, with_text):
    S = 4
    N = size * S
    base = vgrad(N, N, BG1, BG2).convert("RGBA")
    mask = Image.new("L", (N, N), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, N - 1, N - 1], radius=int(N * 0.22), fill=255)
    lay = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    d = ImageDraw.Draw(lay)
    k = N / 256.0
    # тонкое кольцо-«солнце» за ростком
    cy_off = -14 if with_text else 0
    ring_c = (128 * k, (112 + cy_off) * k); rr = 78 * k
    d.ellipse([ring_c[0]-rr, ring_c[1]-rr, ring_c[0]+rr, ring_c[1]+rr], outline=SUN + (255,), width=int(9 * k))
    # стебель и листья
    stem = bezier((128 * k, (186 + cy_off) * k), (122 * k, (140 + cy_off) * k), (130 * k, (84 + cy_off) * k))
    d.line(stem, fill=GREEN_D + (255,), width=int(12 * k), joint="curve")
    for p in (stem[0], stem[-1]):
        d.ellipse([p[0]-6*k, p[1]-6*k, p[0]+6*k, p[1]+6*k], fill=GREEN_D + (255,))
    leaf(d, (127 * k, (128 + cy_off) * k), (60 * k, (84 + cy_off) * k), 54 * k, GREEN + (255,), GREEN_D + (255,))
    leaf(d, (128 * k, (112 + cy_off) * k), (200 * k, (60 + cy_off) * k), 60 * k, GREEN_L + (255,), GREEN + (255,))
    # земля
    g0 = (186 + cy_off) * k
    d.rounded_rectangle([70 * k, g0, 186 * k, g0 + 14 * k], radius=int(7 * k), fill=(92, 66, 46, 255))
    if with_text:
        f = font(int(46 * k))
        tw = d.textlength("TBG", font=f)
        d.text(((256 * k - tw) / 2, 200 * k), "TBG", font=f, fill=WHITE + (255,))
    base.alpha_composite(lay)
    out = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    out.paste(base, (0, 0), mask)
    return out.resize((size, size), Image.LANCZOS)

def banner():
    W, H = 1280, 640
    im = vgrad(W, H, BG1, (24, 38, 54)).convert("RGBA")
    ic = icon(300, True)
    im.alpha_composite(ic, (90, 170))
    d = ImageDraw.Draw(im)
    d.text((430, 185), "PvZ TBG Mod", font=font(84), fill=WHITE)
    d.text((434, 298), "Мод-меню для Plants vs. Zombies GOTY (Steam)", font=font(28, False), fill=(170, 185, 200))
    chips = ["Растения", "Зомби", "Уровень", "Игра", "Сборки", "Конвейер"]
    x = 434
    for c in chips:
        f = font(22, False); w = d.textlength(c, font=f) + 32
        d.rounded_rectangle([x, 366, x + w, 410], radius=22, fill=(32, 52, 72), outline=GREEN, width=2)
        d.text((x + 16, 374), c, font=f, fill=GREEN_L)
        x += w + 12
    d.text((434, 450), "Tab — открыть меню  ·  F6 / F7 / F8 — быстрые действия", font=font(24, False), fill=(130, 150, 168))
    d.text((434, 560), "Неофициальный фанатский мод для одиночной игры. Не связан с PopCap / EA.", font=font(20, False), fill=(110, 128, 146))
    return im.convert("RGB")

def main():
    os.makedirs(OUT, exist_ok=True)
    sizes = [16, 24, 32, 48, 64, 128, 256]
    imgs = [icon(s, s >= 64) for s in sizes]
    imgs[-1].save(os.path.join(OUT, "icon-256.png"))
    imgs[-1].save(os.path.join(OUT, "icon.ico"), format="ICO", sizes=[(s, s) for s in sizes], append_images=imgs[:-1])
    banner().save(os.path.join(OUT, "banner.png"))
    print("assets готовы:", os.path.abspath(OUT))

if __name__ == "__main__":
    sys.exit(main())
