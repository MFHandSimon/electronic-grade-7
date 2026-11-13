#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
rdk_field_scan.py —— 3×3 场地实时扫描采集（RDK 端）
================================================================
配合固件 esp32_scan_field.ino（v1.0.0）使用。
ESP32 开启流式测距后，以约 10Hz 输出 "D:f r b l"（前 右 后 左，mm，-1=无效）。
本脚本负责：发指令、收数据、落 CSV、实时打印、防撞急停。

用法（RDK 上）：
    python3 rdk_field_scan.py stand     # 原地测距，小车不动，先验证四个方向读数
    python3 rdk_field_scan.py go        # 边前进边采数（带前向防撞急停）

参数：
    --guard 200    前进模式下前方小于该毫米数就急停（默认 200mm）
    --dur   30     采集多少秒后自动停车（默认 30s）

输出：
    scan_log_YYYYMMDD_HHMMSS.csv   （时间戳, 前, 右, 后, 左）
"""

import serial
import sys
import time
import csv
import os
from datetime import datetime

PORT = "/dev/ttyS3"
BAUD = 115200


def main():
    mode = "stand"
    guard_mm = 200
    duration = 30.0

    args = sys.argv[1:]
    if args and args[0] in ("stand", "go"):
        mode = args[0]
        args = args[1:]
    i = 0
    while i < len(args):
        if args[i] == "--guard" and i + 1 < len(args):
            guard_mm = int(args[i + 1]); i += 2
        elif args[i] == "--dur" and i + 1 < len(args):
            duration = float(args[i + 1]); i += 2
        else:
            i += 1

    try:
        ser = serial.Serial(PORT, BAUD, timeout=0.1)
    except Exception as e:
        print(f"✗ 打不开 {PORT}: {e}")
        sys.exit(1)

    time.sleep(0.2)
    ser.reset_input_buffer()

    if mode == "go":
        print(f"=== 场地扫描 · 前进采集（防撞 {guard_mm}mm，{duration:.0f}s）===")
    else:
        print("=== 场地扫描 · 原地测距（小车不动）===")
    print("Ctrl+C 结束并保存")

    # 开流式测距
    ser.write(b'C')
    ser.flush()
    time.sleep(0.3)

    if mode == "go":
        ser.write(b'W')
        ser.flush()

    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_dir = os.path.dirname(os.path.abspath(__file__))
    csv_path = os.path.join(out_dir, f"scan_log_{stamp}.csv")

    rows = []
    t0 = time.time()
    stopped = False
    print("time      F(mm)  R(mm)  B(mm)  L(mm)")
    print("-" * 44)

    try:
        while True:
            elapsed = time.time() - t0
            if elapsed > duration:
                break

            line = ser.readline().decode(errors="ignore").strip()
            if not line:
                continue

            if line.startswith("D:"):
                parts = line[2:].split()
                if len(parts) != 4:
                    continue
                try:
                    f, r, b, l = [int(p) for p in parts]
                except ValueError:
                    continue

                ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                rows.append([ts, f, r, b, l])
                print(f"{ts[3:]:>10}  {f:>5}  {r:>5}  {b:>5}  {l:>5}")

                # 前进模式：前向防撞
                if mode == "go" and f != -1 and f < guard_mm:
                    ser.write(b'X')
                    ser.flush()
                    stopped = True
                    print(f"⚠ 前方 {f}mm < {guard_mm}mm，已急停")
                    break

            elif line.startswith("T:"):
                print(f"  单次读数: {line[2:]}")
            elif line in ("K", "READY") or line.startswith("VL "):
                if line.startswith("VL "):
                    print(f"  [传感器] {line}")
    except KeyboardInterrupt:
        print("\n用户中断")

    # 收尾：停车 + 关流式
    try:
        ser.write(b'X')
        ser.write(b'E')
        ser.flush()
        time.sleep(0.2)
    except Exception:
        pass
    ser.close()

    with open(csv_path, "w", newline="", encoding="utf-8-sig") as fp:
        w = csv.writer(fp)
        w.writerow(["time", "front_mm", "right_mm", "back_mm", "left_mm"])
        w.writerows(rows)

    print("-" * 44)
    print(f"共 {len(rows)} 帧，已保存：{csv_path}")
    if stopped:
        print("（本次在前向防撞处提前结束）")


if __name__ == "__main__":
    main()
