#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
rdk_car_realtime.py —— RDK 实时小车控制（发送后立即返回，不阻塞）
================================================================
用途：连续快速控制小车。每输入一个指令立即发送，不等 K 回显，
      因此可以连续输入 W/S/A/D 无延迟。

指令：W前进 S后退 A左转 D右转 X停止 Q退出
支持连输（如 "WA"）。

用法（RDK 上）：
    python3 rdk_car_realtime.py

接线：RDK 引脚8→ESP32 GPIO32, 10→GPIO33, GND→GND（波特率115200）
"""

import serial
import sys
import threading

PORT = "/dev/ttyS3"
BAUD = 115200

CMD_NAME = {
    'W': '前进', 'S': '后退', 'A': '左转', 'D': '右转', 'X': '停止',
    'T': '读距离',
}

ser = None
stop_thread = False


def ack_reader():
    """后台线程：持续读取 ESP32 回显（不阻塞主输入）"""
    global ser, stop_thread
    while not stop_thread:
        try:
            if ser and ser.in_waiting:
                data = ser.read(ser.in_waiting)
                for ch in data.decode(errors='ignore'):
                    if ch == 'K':
                        pass  # 忽略 K，不做任何等待
                print(f"  <<ESP32: {data.decode(errors='ignore').strip()}", flush=True)
            else:
                import time
                time.sleep(0.02)
        except Exception:
            break


def main():
    global ser, stop_thread
    try:
        ser = serial.Serial(PORT, BAUD, timeout=0.05)
    except Exception as e:
        print(f"✗ 打不开 {PORT}: {e}")
        sys.exit(1)

    # 启动后台读线程
    t = threading.Thread(target=ack_reader, daemon=True)
    t.start()

    print(f"=== RDK 实时小车控制 (串口 {PORT}) ===")
    print("W前进 S后退 A左转 D右转 X停止 Q退出")
    print("输入后回车立即发送，可连输如 WA，无需等待")
    print()

    while True:
        try:
            line = input("> ").strip().upper()
        except (EOFError, KeyboardInterrupt):
            print("\n退出")
            break
        if not line:
            continue
        if line == 'Q':
            break
        for ch in line:
            if ch not in CMD_NAME:
                print(f"  ? 未知指令: {ch}")
                continue
            ser.write(ch.encode())
            ser.flush()
            print(f"  → {ch} {CMD_NAME[ch]}")

    stop_thread = True
    try:
        ser.write(b'X')   # 退出前停车
        ser.flush()
    except Exception:
        pass
    ser.close()


if __name__ == "__main__":
    main()
