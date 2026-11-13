#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
rdk_corridor_test.py —— RDK 走廊直线测试（画走廊图 + 判断走姿）
================================================================
用途：让小车沿窄走廊走 3 米直线，边走边回传左右测距，
      RDK 端实时打印距离，走完画一张走廊俯视 ASCII 图，
      直观看出小车轨迹在走廊里的位置和走姿。

用法（RDK 上）：
    python3 rdk_corridor_test.py
    发送 'T' 后小车自动走 3 米，实时回传 L/R 距离。

接线：RDK 引脚8→ESP32 GPIO32, 10→GPIO33, GND→GND（波特率115200）
"""

import serial
import time
import argparse

PORT = "/dev/ttyS3"
BAUD = 115200
CORRIDOR_W = 1080.0   # 走廊宽 mm (1.08m)
SAMPLES = []          # 收集 (距离_左mm, 距离_右mm)


def draw_corridor(samples, corridor_w):
    """画走廊俯视 ASCII 图：左右墙 + 小车轨迹"""
    if not samples:
        print("无数据")
        return
    # 画布宽度（字符列数）
    width = 60
    # 每字符代表的 mm
    mm_per_col = corridor_w / width

    # 用左右距离求小车中心在走廊里的位置
    # 中心距左墙 = 左距离, 中心距右墙 = 右距离
    # 走廊宽 = 左距离 + 车宽 + 右距离，这里简化用中心=左右均值偏移
    # 车中心离左墙 = 左测距；但测距是"传感器到墙"，车宽需要扣除，这里用近似
    print("\n=== 走廊俯视图 (宽 %.2fcm, 长 %.1fm) ===" % (corridor_w/10, len(samples)/10.0))
    print("    '='=左墙   '#'=小车轨迹   '='=右墙")
    # 画顶墙
    print(" " + "=" * width)

    # 逐采样点画小车位置
    for i, (ld, rd) in enumerate(samples):
        # 简化：小车中心位置 = 左墙 + 左距离（传感器假设在车中心附近）
        if ld < 0 or rd < 0:
            row = [" "] * width
            pos = width // 2
        else:
            center_mm = ld
            pos = int(center_mm / mm_per_col)
            pos = max(1, min(width - 2, pos))
            row = [" "] * width
            # 左右墙大致位置
            lw = 0
            rw = width - 1
            row[lw] = "="
            row[rw] = "="
        row[pos] = "#"
        line = "".join(row)
        # 标记起点
        if i == 0:
            line = "S" + line[1:]
        print(line)
    print(" " + "=" * width)

    # 统计
    print("\n=== 走姿分析 ===")
    l_ok = sum(1 for l, _ in samples if 0 < l)
    r_ok = sum(1 for _, r in samples if 0 < r)
    print(f"有效采样点: {len(samples)}")
    if l_ok:
        print(f"左侧测距有效: {l_ok}/{len(samples)}")
    if r_ok:
        print(f"右侧测距有效: {r_ok}/{len(samples)}")

    # 判断是否居中（用左右距离）
    mids = []
    for l, r in samples:
        if l > 0 and r > 0:
            mids.append(l - r)
    if mids:
        avg = sum(mids) / len(mids)
        print(f"左右测距差均值: {avg:.0f}mm ({'偏左' if avg>20 else ('偏右' if avg<-20 else '居中')})")
        print(f"左右测距差范围: {min(mids):.0f} ~ {max(mids):.0f}mm")


def main():
    ap = argparse.ArgumentParser(description="RDK 走廊直线测试")
    ap.add_argument("--port", default=PORT)
    ap.add_argument("--baud", type=int, default=BAUD)
    args = ap.parse_args()

    try:
        ser = serial.Serial(args.port, args.baud, timeout=1.0)
    except Exception as e:
        print(f"✗ 打不开 {args.port}: {e}")
        return

    print(f"=== RDK 走廊测试 (串口 {args.port}) ===")
    print("小车将走 3 米直线，实时回传左右距离...")
    print("按 Ctrl+C 随时中断（会先发停止指令）\n")

    ser.reset_input_buffer()
    ser.write(b'T')   # 启动走廊测试
    ser.flush()

    start = time.time()
    try:
        while True:
            line = ser.readline().decode(errors='ignore').strip()
            if not line:
                # 超时保护
                if time.time() - start > 120:
                    print("超时(120s)")
                    break
                continue
            if line == 'K':
                print("✓ 小车走完 3 米")
                break
            if line.startswith('L:') and 'R:' in line:
                parts = line.split()
                try:
                    ld = int(parts[0].split(':')[1])
                    rd = int(parts[1].split(':')[1])
                    SAMPLES.append((ld, rd))
                    print(f"  [{len(SAMPLES):3d}] L:{ld:4d}mm  R:{rd:4d}mm")
                except (ValueError, IndexError):
                    print(f"  原始: {line}")
            else:
                print(f"  {line}")
    except KeyboardInterrupt:
        print("\n中断，发送停止...")
        ser.write(b'X')
        ser.flush()
    finally:
        ser.close()

    draw_corridor(SAMPLES, CORRIDOR_W)


if __name__ == "__main__":
    main()
