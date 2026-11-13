#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
plot_scan.py —— 把 360° 扫描 CSV 画成俯视图（字符图 / PNG）
================================================================
输入 CSV 列：yaw,f,r,b,l   （yaw 单位度，距离单位 mm，无效值 <=0 或 >2000 会被丢弃）

坐标约定（车体坐标系，yaw 为航向角，左转为正）：
    F 前 → yaw + 0
    R 右 → yaw - 90
    B 后 → yaw + 180
    L 左 → yaw + 90
    x = d * cos(angle), y = d * sin(angle)

用法：
    python3 plot_scan.py scan360_test.csv            # 字符图
    python3 plot_scan.py scan360_test.csv --png out.png
    python3 plot_scan.py scan360_test.csv --fr       # 只用 F/R 两路（B/L 俯角大，量地面）
    python3 plot_scan.py scan360_test.csv --min 300  # 丢弃 300mm 内的地面假点
"""

import sys
import csv
import math

MAX_VALID_MM = 2000
MIN_VALID_MM = 0
USE_FR_ONLY = False
SIZE = 45


def load(path):
    rows = []
    with open(path, newline="", encoding="utf-8-sig") as fp:
        rd = csv.DictReader(fp)
        for r in rd:
            try:
                yaw = float(r["yaw"])
                f = int(float(r["f"]))
                rr = int(float(r["r"]))
                b = int(float(r["b"]))
                l = int(float(r["l"]))
            except (KeyError, ValueError):
                continue
            rows.append((yaw, f, rr, b, l))
    return rows


def to_points(rows):
    pts = []
    for yaw, f, r, b, l in rows:
        pairs = ((f, 0.0), (r, -90.0)) if USE_FR_ONLY else \
                ((f, 0.0), (r, -90.0), (b, 180.0), (l, 90.0))
        for d, off in pairs:
            if d <= 0 or d > MAX_VALID_MM or d < MIN_VALID_MM:
                continue
            a = math.radians(yaw + off)
            pts.append((d * math.cos(a), d * math.sin(a), d))
    return pts


def ascii_map(pts, half=1200.0, size=SIZE):
    grid = [[" " for _ in range(size)] for _ in range(size)]
    for x, y, _d in pts:
        cx = int((x + half) / (2 * half) * (size - 1))
        cy = int((half - y) / (2 * half) * (size - 1))
        if 0 <= cx < size and 0 <= cy < size:
            grid[cy][cx] = "#"
    grid[size // 2][size // 2] = "O"
    scale = 2 * half / (size - 1)
    print(f"俯视图  每格 {scale:.0f}mm  视野 ±{half:.0f}mm  O=小车  #=障碍点")
    for row in grid:
        print("".join(row))


def png_map(pts, out):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        return raster_png(pts, out)
    fig, ax = plt.subplots(figsize=(7, 7))
    ax.scatter([p[0] for p in pts], [p[1] for p in pts], s=12, c="#2b6cb0")
    ax.scatter([0], [0], s=120, c="#c53030", marker="^")
    ax.set_aspect("equal")
    ax.grid(alpha=0.3)
    ax.set_xlabel("x (mm)")
    ax.set_ylabel("y (mm)")
    ax.set_title("360 scan")
    ax.set_xlim(-1500, 1500)
    ax.set_ylim(-1500, 1500)
    fig.savefig(out, dpi=140, bbox_inches="tight")
    print("已保存", out)
    return True


def raster_png(pts, out, px=900, half=1500.0):
    """不依赖 matplotlib，用 zlib 手写 PNG 俯视图"""
    import zlib
    import struct

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
        cx = int((x + half) / (2 * half) * (W - 1))
        cy = int((half - y) / (2 * half) * (H - 1))
        return cx, cy

    # 网格线：每 500mm 一条浅灰线
    for mm in range(-1500, 1501, 500):
        cx, _ = to_px(mm, 0)
        cy_line, _ = to_px(0, mm)
        for k in range(H):
            i = (k * W + max(0, min(W - 1, cx))) * 3
            buf[i:i + 3] = bytes((205, 205, 205))
            i2 = (max(0, min(H - 1, cy_line)) * W + k) * 3
            buf[i2:i2 + 3] = bytes((205, 205, 205))

    for x, y, _d in pts:
        if abs(x) > half or abs(y) > half:
            continue
        cx, cy = to_px(x, y)
        put(cx, cy, (43, 108, 176), 2)

    # 中心标记（小车）
    c0, _ = to_px(0, 0)
    put(c0, c0, (197, 48, 48), 5)

    raw = bytearray()
    for y in range(H):
        raw.append(0)                     # 每行 filter type 0
        raw.extend(buf[y * W * 3:(y + 1) * W * 3])

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        c += struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        return c

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(bytes(raw), 6))
           + chunk(b"IEND", b""))
    with open(out, "wb") as fp:
        fp.write(png)
    print("已保存", out, f"（{W}×{H}，视野 ±{half:.0f}mm，每格 500mm 网格）")
    return True


def main():
    global MIN_VALID_MM, USE_FR_ONLY
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return
    path = args[0]
    if "--fr" in args:
        USE_FR_ONLY = True
    if "--min" in args:
        idx = args.index("--min")
        MIN_VALID_MM = int(args[idx + 1]) if len(args) > idx + 1 else 0

    rows = load(path)
    print(f"读入 {len(rows)} 行"
          f"  传感器 {'F/R' if USE_FR_ONLY else 'F/R/B/L'}"
          f"  丢弃 <{MIN_VALID_MM}mm")
    pts = to_points(rows)
    print(f"有效点 {len(pts)} 个")

    if "--png" in args:
        idx = args.index("--png")
        out = args[idx + 1] if len(args) > idx + 1 else "scan_map.png"
        png_map(pts, out)
    else:
        ascii_map(pts)


if __name__ == "__main__":
    main()
