#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
rdk_scan360.py —— 定点 360° 旋转扫描（3×3 场地建图第一步）
================================================================
配合 esp32_scan_field.ino v1.2.0+。车不动位置，原地自转一圈，
每转 30° 采一组四向距离，得到该点的极坐标障碍轮廓。

为什么这么设计：小车无编码器，无法可靠推算位移，但陀螺仪航向角可信（实测 2.6% 误差）。
所以先做"定点环视"，纯靠角度建图，不依赖里程。

4 路传感器指向（车体坐标系，heading=0 朝前）：
    F 前 = heading         R 右 = heading - 90
    B 后 = heading + 180   L 左 = heading + 90

用法（RDK）：
    python3 rdk_scan360.py            # 扫一圈，输出 CSV + 字符地图
    python3 rdk_scan360.py 15         # 每 15° 采一次（更密，慢一倍）
"""

import serial
import sys
import time
import math
import csv
from datetime import datetime

PORT = "/dev/ttyS3"
BAUD = 115200

STEP_DEG = float(sys.argv[1]) if len(sys.argv) > 1 else 30.0


def main():
    p = serial.Serial(PORT, BAUD, timeout=0.2)
    time.sleep(0.2)
    p.reset_input_buffer()

    def readT():
        """发 T，返回 [f, r, b, l, yaw×10]，读不到返回 None"""
        p.write(b'T')
        t = time.time()
        while time.time() - t < 0.6:
            line = p.readline().decode(errors="ignore").strip()
            if line.startswith("T:"):
                try:
                    v = [int(x) for x in line[2:].split()]
                except ValueError:
                    continue
                if len(v) >= 5:
                    return v
        return None

    def spin_ms(ms):
        p.write(b'D')
        time.sleep(ms / 1000.0)
        p.write(b'X')
        time.sleep(0.10)

    # 归零航向
    p.write(b'Z')
    time.sleep(0.3)
    p.reset_input_buffer()

    n_steps = int(round(360.0 / STEP_DEG))
    rows = []
    prev_yaw = 0.0

    print(f"=== 360° 扫描：{n_steps} 步，每步 {STEP_DEG:.0f}° ===")
    print("步  角度    F      R      B      L     yaw")
    print("-" * 52)

    for i in range(n_steps):
        target = (i + 1) * STEP_DEG       # 相对起点的累计目标角

        # 一小段一小段地转，每段结束都核对航向，转到目标角为止
        guard = 0
        while guard < 60:
            v = readT()
            if v is None:
                break
            if v[4] / 10.0 >= target:
                break
            spin_ms(45)
            guard += 1

        v = readT()
        if v is None:
            print(f"{i:3d}  读数失败，跳过")
            continue

        yaw = v[4] / 10.0
        f, r, b, l = v[0], v[1], v[2], v[3]
        rows.append([yaw, f, r, b, l])
        print(f"{i:3d}  {yaw:6.1f}  {f:>5}  {r:>5}  {b:>5}  {l:>5}  {yaw:6.1f}")

    p.write(b'X')
    p.write(b'E')
    p.close()

    # ---- 存 CSV ----
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    csv_path = f"/app/scan360_{stamp}.csv"
    with open(csv_path, "w", newline="", encoding="utf-8-sig") as fp:
        w = csv.writer(fp)
        w.writerow(["yaw_deg", "front_mm", "right_mm", "back_mm", "left_mm"])
        w.writerows(rows)
    print("-" * 52)
    print(f"已保存：{csv_path}")

    # ---- 转成极坐标点，打字符地图 ----
    # 车体坐标系→世界坐标系：F 沿 yaw，R 在 yaw-90，B 在 yaw+180，L 在 yaw+90
    pts = []
    for yaw, f, r, b, l in rows:
        for dist, ang_off in ((f, 0), (r, -90), (b, 180), (l, 90)):
            if dist <= 0 or dist > 2000:
                continue
            a = math.radians(yaw + ang_off)
            x = dist * math.cos(a)
            y = dist * math.sin(a)
            pts.append((x, y))

    print(f"有效点 {len(pts)} 个")

    SIZE = 41          # 41×41 字符格
    HALF = 1000.0      # 视野 ±1000mm
    grid = [[" " for _ in range(SIZE)] for _ in range(SIZE)]

    def to_cell(x, y):
        cx = int((x + HALF) / (2 * HALF) * (SIZE - 1))
        cy = int((HALF - y) / (2 * HALF) * (SIZE - 1))   # y 向上为正
        if 0 <= cx < SIZE and 0 <= cy < SIZE:
            return cx, cy
        return None

    for x, y in pts:
        c = to_cell(x, y)
        if c:
            grid[c[1]][c[0]] = "#"

    # 车本身在中心
    grid[SIZE // 2][SIZE // 2] = "O"

    print(f"\n俯视图（每格 {2 * HALF / (SIZE - 1):.0f}mm，O=小车，#=障碍点，视野±{HALF:.0f}mm）")
    for row in grid:
        print("".join(row))


if __name__ == "__main__":
    main()
