#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
step_scan.py —— 开到离墙 550mm 停下，再原地扫描一圈（RDK 侧）
================================================================
第一步：慢速脉冲前进，直到 F 读数 < 550mm 停下
第二步：定点 12 步 × 30° 扫描
输出 CSV 列：yaw,f,r,b,l

注意：try/finally 兜底发 X；超时 10 秒强制停。
"""
import serial, time, csv
from datetime import datetime

p = serial.Serial("/dev/ttyS3", 115200, timeout=.1)
time.sleep(.2)
p.reset_input_buffer()

rows = []


def T():
    p.write(b"T")
    t = time.time()
    while time.time() - t < 0.8:
        l = p.readline().decode(errors="ignore").strip()
        if l.startswith("T:"):
            v = [int(x) for x in l[2:].split()]
            if len(v) >= 5:
                return v
    return None


def TT():
    for _ in range(6):
        v = T()
        if v:
            return v
    return None


def spin(ms):
    p.write(b"D")
    time.sleep(ms / 1000.0)
    p.write(b"X")
    time.sleep(0.10)


try:
    p.write(b"E")
    time.sleep(0.2)
    p.write(b"Z")
    p.write(b"C")

    # 1) 慢速开到离墙约 550mm
    last = None
    t0 = time.time()
    v = None
    while time.time() - t0 < 10:
        on = (time.time() - t0) % 0.45 < 0.30
        if on != last:
            p.write(b"W" if on else b"X")
            last = on
        l = p.readline().decode(errors="ignore").strip()
        if l.startswith("D:"):
            v = [int(x) for x in l[2:].split()]
            if len(v) >= 4 and v[0] > 0 and v[0] < 550:
                break
    p.write(b"X")
    time.sleep(0.3)
    print("到位 F=", (v[0] if v else "?"), flush=True)

    # 2) 定点 12 步一圈
    for i in range(12):
        tgt = (i + 1) * 30
        b = 0
        while b < 80:
            v = TT()
            if v is None:
                break
            if v[4] / 10.0 >= tgt:
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
except KeyboardInterrupt:
    print("手动停止", flush=True)
finally:
    p.write(b"X")
    p.write(b"E")
    p.close()

n = datetime.now().strftime("%H%M%S")
open("/app/stepscan_" + n + ".csv", "w").write(
    "yaw,f,r,b,l\n" + "".join("%s,%s,%s,%s,%s\n" % tuple(r) for r in rows))
print("OK", len(rows), "/app/stepscan_" + n + ".csv")
