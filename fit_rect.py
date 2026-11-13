#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fit_rect.py —— 从 360° 扫描数据拟合房间矩形，并画带矩形轮廓的俯视图
================================================================
假设：小车位于房间几何中心（原点），四路测距分别对应 F/R/B/L。

模型：沿方向 a 到矩形边界的距离
    r(a) = min( a/|cos a| , b/|sin a| )
其中 a,b 是半宽/半深。

用法：
    python3 fit_rect.py scan360_run4.csv
    python3 fit_rect.py scan360_run4.csv --png out.png
"""
import sys
import csv
import math
import zlib
import struct

MAXV = 2000


def load(path):
    out = []
    with open(path, newline="", encoding="utf-8-sig") as fp:
        for r in csv.DictReader(fp):
            try:
                out.append((float(r["yaw"]), int(float(r["f"])),
                            int(float(r["r"])), int(float(r["b"])),
                            int(float(r["l"]))))
            except (KeyError, ValueError):
                continue
    return out


def samples(rows):
    """返回 [(世界方向角, 距离)]，方向角单位度"""
    s = []
    for yaw, f, r, b, l in rows:
        for d, off in ((f, 0.0), (r, -90.0), (b, 180.0), (l, 90.0)):
            if 0 < d <= MAXV:
                s.append((math.radians(yaw + off), d / 1000.0))
    return s


def model(a, b, ang):
    c, sn = abs(math.cos(ang)), abs(math.sin(ang))
    dx = a / c if c > 1e-9 else 1e9
    dy = b / sn if sn > 1e-9 else 1e9
    return min(dx, dy)


def fit(s, astep=0.005):
    best = (1e18, 0.7, 0.7)
    a = 0.40
    while a <= 1.30:
        b = 0.40
        while b <= 1.30:
            e = sum(abs(model(a, b, ang) - d) for ang, d in s)
            if e < best[0]:
                best = (e, a, b)
            b += astep
        a += astep
    return best


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return
    rows = load(args[0])
    s = samples(rows)
    err, a, b = fit(s)
    n = len(s)
    print(f"样本 {n} 个")
    print(f"拟合半宽 a = {a*1000:.0f}mm  半深 b = {b*1000:.0f}mm")
    print(f"即房间约 {2*a*1000:.0f} × {2*b*1000:.0f} mm")
    print(f"平均残差 {err/n*1000:.1f}mm")

    pts = [(d * 1000 * math.cos(ang), d * 1000 * math.sin(ang)) for ang, d in s]
    if "--png" in args:
        out = args[args.index("--png") + 1]
        draw(pts, a * 1000, b * 1000, out)
        print("已保存", out)
    else:
        print(ascii_art(pts, a * 1000, b * 1000))


def ascii_art(pts, a, b, half=1200.0, size=45):
    g = [[" " for _ in range(size)] for _ in range(size)]

    def mark(x, y, ch):
        cx = int((x + half) / (2 * half) * (size - 1))
        cy = int((half - y) / (2 * half) * (size - 1))
        if 0 <= cx < size and 0 <= cy < size:
            g[cy][cx] = ch

    for (x, y) in ((-a, -a), (a, -a), (a, a), (-a, a)):
        pass
    # 画拟合矩形
    step = 2 * half / (size - 1)
    xs = -a
    while xs <= a:
        mark(xs, b, "+"); mark(xs, -b, "+")
        xs += step
    ys = -b
    while ys <= b:
        mark(a, ys, "+"); mark(-a, ys, "+")
        ys += step
    for x, y in pts:
        mark(x, y, "#")
    mark(0, 0, "O")
    lines = ["".join(r) for r in g]
    return ("每格 %.0fmm  + = 拟合矩形  # = 实测点  O = 小车\n" % step) + "\n".join(lines)


def draw(pts, a, b, out, px=900, half=1200.0):
    W = H = px
    buf = bytearray([255, 255, 255] * (W * H))

    def put(x, y, color, r=2):
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                if dx * dx + dy * dy > r * r:
                    continue
                xx, yy = x + dx, y + dy
                if 0 <= xx < W and 0 <= yy < H:
                    i = (yy * W + xx) * 3
                    buf[i:i + 3] = bytes(color)

    def to_px(x, y):
        return (int((x + half) / (2 * half) * (W - 1)),
                int((half - y) / (2 * half) * (H - 1)))

    for mm in range(-1000, 1001, 500):
        cx, _ = to_px(mm, 0)
        cy, _ = to_px(0, mm)
        for k in range(H):
            if 0 <= cx < W:
                buf[(k * W + cx) * 3:(k * W + cx) * 3 + 3] = bytes((215, 215, 215))
            if 0 <= cy < H:
                buf[(cy * W + k) * 3:(cy * W + k) * 3 + 3] = bytes((215, 215, 215))

    # 拟合矩形（绿线）
    k = 4
    for t in range(int(-a), int(a), k):
        x1, y1 = to_px(t, b); put(x1, y1, (16, 148, 84), 1)
        x2, y2 = to_px(t, -b); put(x2, y2, (16, 148, 84), 1)
    for t in range(int(-b), int(b), k):
        x1, y1 = to_px(a, t); put(x1, y1, (16, 148, 84), 1)
        x2, y2 = to_px(-a, t); put(x2, y2, (16, 148, 84), 1)

    for x, y in pts:
        cx, cy = to_px(x, y)
        put(cx, cy, (43, 108, 176), 3)

    c, _ = to_px(0, 0)
    put(c, c, (197, 48, 48), 5)

    raw = bytearray()
    for y in range(H):
        raw.append(0)
        raw.extend(buf[y * W * 3:(y + 1) * W * 3])

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(bytes(raw), 6))
           + chunk(b"IEND", b""))
    open(out, "wb").write(png)


if __name__ == "__main__":
    main()
