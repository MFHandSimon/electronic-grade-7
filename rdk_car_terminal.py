#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
rdk_car_terminal.py —— RDK 前后控制（逐键输入版，无需 pynput）
================================================================
用途：RDK 无显示环境（无 X server），pynput 键盘监听不可用，
      改用逐键输入：输入指令按回车执行。

指令：F 前进   B 后退   X 停止   Q 退出
可以连续输入多个，如 "FB" 会依次前进再后退。

用法（RDK 上）：
    python3 rdk_car_terminal.py

接线（RDK 40PIN → ESP32，已确认）：
    RDK 引脚8  → ESP32 GPIO32 (RX)
    RDK 引脚10 → ESP32 GPIO33 (TX)
    RDK GND    → ESP32 GND

波特率：115200
"""

import serial
import time
import sys

PORT = "/dev/ttyS3"
BAUD = 115200

# 指令名映射（WASD 键位）
CMD_NAME = {
    'W': '前进', 'S': '后退', 'A': '左转', 'D': '右转', 'X': '停止',
}

# WASD 键位 → ESP32 实际动作指令
# ESP32 固件同时兼容 WASD 和 FBLR，这里直接用 WASD 原样下发
KEY_TO_CMD = {
    'W': 'W', 'S': 'S', 'A': 'A', 'D': 'D', 'X': 'X',
}


def open_serial():
    try:
        ser = serial.Serial(PORT, BAUD, timeout=0.5)
        return ser
    except Exception as e:
        print(f"✗ 打不开 {PORT}: {e}")
        sys.exit(1)


def send_cmd(ser, ch, verbose=True):
    """发送单条指令，等 K 回显（等 12 秒），打印 ESP32 所有回显"""
    ser.reset_input_buffer()
    ser.write(ch.encode())
    ser.flush()
    deadline = time.time() + 12.0
    buf = b''
    while time.time() < deadline:
        if ser.in_waiting:
            data = ser.read(ser.in_waiting)
            buf += data
            if verbose:
                try:
                    print(f"   <<ESP32: {data.decode(errors='ignore').strip()}")
                except Exception:
                    pass
        time.sleep(0.02)
    # 只要收到过任何东西（含 ENC_TIMEOUT），都算有响应
    return b'K' in buf


def main():
    ser = open_serial()
    print(f"=== RDK 小车控制 (串口 {PORT}) ===")
    print("指令: W前进360mm S后退 A左转 D右转 X停止 Q退出")
    print("回车后小车会移动几秒，等它走完才回K，请观察小车动作")
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
            cmd = KEY_TO_CMD.get(ch, ch)
            ok = send_cmd(ser, cmd)
            name = CMD_NAME[ch]
            if ok:
                print(f"  ✓ {ch} {name} (ESP32有响应)")
            else:
                print(f"  ? {ch} {name} 无任何回显(超时12s)")

    ser.close()


if __name__ == "__main__":
    main()
