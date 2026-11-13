#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
s360.py —— 定点 360° 旋转扫描（RDK 侧，串口 /dev/ttyS3）
================================================================
12 步 × 30° = 一圈 360°，每步转到目标航向角后采一次四向测距。
输出 CSV 列：yaw,f,r,b,l

放在 RDK 上运行：
    python3 /app/s360.py
"""
import serial, time, csv
from datetime import datetime

p = serial.Serial("/dev/ttyS3", 115200, timeout=.2)
time.sleep(.2)
p.reset_input_buffer()


def T():
    """单次读取，返回 [f, r, b, l, yaw_x10]，失败返回 None"""
    p.write(b"T")
    t = time.time()
    while time.time() - t < .8:
        l = p.readline().decode(errors="ignore").strip()
        if l.startswith("T:"):
            v = [int(x) for x in l[2:].split()]
            if len(v) >= 5:
                return v
    return None


def TT():
    """重试 6 次"""
    for _ in range(6):
        v = T()
        if v:
            return v
    return None


def spin(ms):
    """右转 ms 毫秒，然后停"""
    p.write(b"D")
    time.sleep(ms / 1000.0)
    p.write(b"X")
    time.sleep(.10)


p.write(b"E")
p.write(b"Z")
time.sleep(.3)
p.reset_input_buffer()

rows = []
for i in range(12):                      # 12 步 = 360°，写 24 会转两圈
    tgt = (i + 1) * 30
    b = 0
    while b < 80:
        v = TT()
        if v and v[4] / 10.0 >= tgt:
            break
        if v is None:
            break
        spin(50)
        b += 1
    v = TT()
    if not v:
        print(i, "FAIL", flush=True)
        continue
    rows.append([v[4] / 10.0, v[0], v[1], v[2], v[3]])
    print(i, "yaw=%.1f" % (v[4] / 10.0), "F", v[0], "R", v[1], "B", v[2],
          "L", v[3], flush=True)

p.write(b"X")
p.write(b"E")
p.close()

n = datetime.now().strftime("%H%M%S")
open("/app/s360_" + n + ".csv", "w").write(
    "yaw,f,r,b,l\n" + "".join("%s,%s,%s,%s,%s\n" % tuple(r) for r in rows))
print("OK", len(rows), "/app/s360_" + n + ".csv")
