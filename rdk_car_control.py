#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
rdk_car_control.py —— RDK 上位机：通过 UART 控制 ESP32 小车运动
================================================================
用途：RDK X3 通过 GPIO 串口（/dev/ttyS3）给 ESP32 发运动指令，
      并读取 ESP32 回传的 'K'（确认执行）。

指令协议（RDK → ESP32）：
    F  前进        B  后退
    L  左转        R  右转
    X  停止

ESP32 收到指令执行完成后回传 'K'。

用法（RDK 上）：
    # 发单条指令
    python3 rdk_car_control.py F
    python3 rdk_car_control.py L

    # 交互模式：输入指令按回车（q 退出）
    python3 rdk_car_control.py --interactive

接线（RDK 40PIN → ESP32，已确认的 8/10 组合）：
    RDK 物理引脚 8  → ESP32 GPIO32 (RX)
    RDK 物理引脚 10 → ESP32 GPIO33 (TX)
    RDK GND          → ESP32 GND
"""

import serial
import argparse
import time
import sys


def send_cmd(port, baud, cmd, wait_ack=0.8):
    """发送单条指令，等待 ESP32 回 K"""
    try:
        ser = serial.Serial(port, baud, timeout=wait_ack)
    except Exception as e:
        print(f"✗ 打不开 {port}: {e}")
        return

    try:
        ser.reset_input_buffer()
        ser.write(cmd.encode())
        ser.flush()

        # 等回显 K
        deadline = time.time() + wait_ack
        ack = b''
        while time.time() < deadline:
            if ser.in_waiting:
                ack += ser.read(ser.in_waiting)
                if b'K' in ack:
                    break
            time.sleep(0.02)

        if b'K' in ack:
            print(f"✓ 指令 {cmd} 已执行 (ESP32 回 K)")
        else:
            print(f"? 指令 {cmd} 发送成功但未收到 K (收到: {ack!r})")
    finally:
        ser.close()


def interactive(port, baud):
    """交互模式：输入指令按回车"""
    print(f"=== 交互控制 (串口 {port}) ===")
    print("指令: F前进 B后退 L左转 R右转 X停止, q=退出")
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
            if ch in 'FBLRX':
                send_cmd(port, baud, ch)


def main():
    ap = argparse.ArgumentParser(description="RDK 控制 ESP32 小车运动")
    ap.add_argument("cmd", nargs="?", default=None,
                    help="单条指令: F/B/L/R/X")
    ap.add_argument("--port", default="/dev/ttyS3", help="串口设备(默认 /dev/ttyS3)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--interactive", action="store_true", help="交互模式")
    args = ap.parse_args()

    if args.interactive:
        interactive(args.port, args.baud)
    elif args.cmd:
        send_cmd(args.port, args.baud, args.cmd.upper())
    else:
        ap.print_help()


if __name__ == "__main__":
    main()
