#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
slowrun.py —— 慢速遍历（RDK 侧）
================================================================
脉冲驱动：走 0.3s 停 0.15s，平均约 0.2m/s。
边跑边读流式测距，落盘 /app/slow_*.csv，列为 t,f,r,b,l

安全要点（重要）：
    - try/finally 兜底发 X，脚本任何异常都不会让车继续冲
    - DUR 限时，F < 220mm 自动停
"""
import serial, time, csv
from datetime import datetime

p = serial.Serial("/dev/ttyS3", 115200, timeout=.1)
time.sleep(.2)
p.reset_input_buffer()

rows = []
t0 = time.time()
DUR = 25.0
last = None
print("慢速遍历 25 秒（走0.3停0.15），Ctrl+C 停", flush=True)

try:
    p.write(b"E")
    time.sleep(.2)
    p.write(b"Z")
    p.write(b"C")
    while time.time() - t0 < DUR:
        on = (time.time() - t0) % 0.45 < 0.30
        if on != last:
            p.write(b"W" if on else b"X")
            last = on
        l = p.readline().decode(errors="ignore").strip()
        if not l.startswith("D:"):
            continue
        v = [int(x) for x in l[2:].split()]
        if len(v) < 4:
            continue
        f, r, b, ch = v[0], v[1], v[2], v[3]
        rows.append([round(time.time() - t0, 2), f, r, b, ch])
        print("%5.2fs F%5d R%5d B%5d L%5d" % (time.time() - t0, f, r, b, ch),
              flush=True)
        if f != -1 and 0 < f < 220:
            print("前方很近，停", flush=True)
            break
except KeyboardInterrupt:
    print("手动停止", flush=True)
finally:
    p.write(b"X")
    p.write(b"E")
    p.close()

n = datetime.now().strftime("%H%M%S")
open("/app/slow_" + n + ".csv", "w").write(
    "t,f,r,b,l\n" + "".join("%s,%s,%s,%s,%s\n" % tuple(r) for r in rows))
print("OK", len(rows), "/app/slow_" + n + ".csv")
