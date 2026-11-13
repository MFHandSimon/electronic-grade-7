#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fit_rect2.py —— 任意车位（角落/偏心）下的房间矩形拟合
================================================================
与 fit_rect.py 的区别：不再假设小车在房间中心。
拟合参数共 5 个：旋转角 theta、半宽 a、半深 b、中心偏移 cx、cy。

方法：
  1. 把实测点按 theta 旋转到轴对齐坐标系
  2. 用分位数取包围盒，得到 a,b,cx,cy
  3. 以「点到矩形边界的最小距离」为目标函数，粗搜 theta 再细化

用法：
  python3 fit_rect2.py scan360_corner.csv
  python3 fit_rect2.py scan360_corner.csv --png out.png
"""
import sys
import csv
import math
import zlib
import struct

MAXV = 2000


def load(path):
    rows = []
    with open(path, newline="", encoding="utf-8-sig") as fp:
        for r in csv.DictReader(fp):
            try:
                rows.append((float(r["yaw"]), int(float(r["f"])),
                             int(float(r["r"])), int(float(r["b"])),
                             int(float(r["l"]))))
            except (KeyError, ValueError):
                continue
    return rows


def points(rows):
    pts = []
    for yaw, f, r, b, l in rows:
        for d, off in ((f, 0.0), (r, -90.0), (b, 180.0), (l, 90.0)):
            if 0 < d <= MAXV:
                ang = math.radians(yaw + off)
                pts.append((d * math.cos(ang), d * math.sin(ang)))
    return pts


def cost(pts, theta, a, b, cx, cy):
    """点到矩形边界（内侧）的平均距离，单位 mm"""
    ct, st = math.cos(-theta), math.sin(-theta)
    tot = 0.0
    for x, y in pts:
        u = x * ct - y * st - cx
        v = x * st + y * ct - cy
        du = abs(abs(u) - a)
        dv = abs(abs(v) - b)
        tot += min(du, dv)
    return tot / max(1, len(pts))


def bbox(pts, theta):
    ct, st = math.cos(-theta), math.sin(-theta)
    us = [x * ct - y * st for x, y in pts]
    vs = [x * st + y * ct for x, y in pts]
    us.sort(); vs.sort()
    k = max(1, int(len(us) * 0.03))
    lo_u, hi_u = us[k], us[-1 - k]
    lo_v, hi_v = vs[k], vs[-1 - k]
    return (hi_u - lo_u) / 2, (hi_v - lo_v) / 2, (lo_u + hi_u) / 2, (lo_v + hi_v) / 2


def fit(pts):
    best = None
    # 粗搜旋转角（矩形有 90° 周期，只搜 0~90）
    for deg in range(0, 90, 2):
        th = math.radians(deg)
        a, b, cx, cy = bbox(pts, th)
        e = cost(pts, th, a, b, cx, cy)
        if best is None or e < best[0]:
            best = (e, th, a, b, cx, cy)
    # 细化
    base = best
    for deg in [base[1] + math.radians(d) for d in (-1.5, -1, -0.5, -0.25, 0,
                                                   0.25, 0.5, 1, 1.5)]:
        a, b, cx, cy = bbox(pts, deg)
        e = cost(pts, deg, a, b, cx, cy)
        if e < best[0]:
            best = (e, deg, a, b, cx, cy)
    return best


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return
    pts = points(load(args[0]))
    if not pts:
        print("没有有效点")
        return
    e, th, a, b, cx, cy = fit(pts)
    print(f"有效点 {len(pts)}")
    print(f"矩形朝向 {math.degrees(th):.1f}°（车体坐标系）")
    print(f"尺寸 {2*a:.0f} × {2*b:.0f} mm")
    print(f"中心在车体坐标 ({cx:.0f}, {cy:.0f}) mm  → 车离最近墙约 "
          f"{min(a - abs(cx), b - abs(cy)):.0f} mm")
    print(f"平均残差 {e:.1f} mm")

    if "--png" in args:
        out = args[args.index("--png") + 1]
        draw(pts, th, a, b, cx, cy, out)
        print("已保存", out)


def draw(pts, th, a, b, cx, cy, out, px=900, half=1500.0):
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

    for mm in range(-1500, 1501, 500):
        cx1, _ = to_px(mm, 0)
        cy1, _ = to_px(0, mm)
        for k in range(H):
            if 0 <= cx1 < W:
                buf[(k * W + cx1) * 3:(k * W + cx1) * 3 + 3] = bytes((215, 215, 215))
            if 0 <= cy1 < H:
                buf[(cy1 * W + k) * 3:(cy1 * W + k) * 3 + 3] = bytes((215, 215, 215))

    ct, st = math.cos(th), math.sin(th)
    corners = []
    for su in (-1, 1):
        for sv in (-1, 1):
            u, v = cx + su * a, cy + sv * b
            corners.append((ct * u - st * v, st * u + ct * v))
    corners = [corners[0], corners[1], corners[3], corners[2], corners[0]]
    for i in range(len(corners) - 1):
        x1, y1 = corners[i]
        x2, y2 = corners[i + 1]
        n = int(max(abs(x2 - x1), abs(y2 - y1)) / 3) + 1
        for t in range(n + 1):
            x = x1 + (x2 - x1) * t / n
            y = y1 + (y2 - y1) * t / n
            px_, py_ = to_px(x, y)
            put(px_, py_, (16, 148, 84), 1)

    for x, y in pts:
        cx_, cy_ = to_px(x, y)
        put(cx_, cy_, (43, 108, 176), 3)

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
