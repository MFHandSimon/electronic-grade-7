#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
rdk_car_forward_back.py —— RDK 实时前后控制（键盘 W/S）
=========================================================
用途：RDK 通过 UART 实时控制小车前进/后退。
      按住 W 前进，按住 S 后退，松开即停。

指令协议（RDK → ESP32）：
    F  前进        B  后退        X  停止

用法（RDK 上，需装 pynput）：
    pip3 install pynput
    python3 rdk_car_forward_back.py

    按住 W → 前进    松开 W → 停止
    按住 S → 后退    松开 S → 停止
    Ctrl+C 退出

接线（RDK 40PIN → ESP32，已确认）：
    RDK 引脚8  → ESP32 GPIO32 (RX)
    RDK 引脚10 → ESP32 GPIO33 (TX)
    RDK GND    → ESP32 GND

波特率：115200
"""

import serial
import time
import sys
import threading

try:
    from pynput import keyboard
except ImportError:
    print("缺少 pynput，请安装: pip3 install pynput")
    sys.exit(1)

PORT = "/dev/ttyS3"
BAUD = 115200

ser = None
current_state = "X"   # 当前应处于的运动状态
lock = threading.Lock()


def send(ch):
    """向 ESP32 发送指令"""
    global ser
    try:
        ser.reset_input_buffer()
        ser.write(ch.encode())
        ser.flush()
        # 等待回 K
        deadline = time.time() + 0.5
        while time.time() < deadline:
            if ser.in_waiting:
                ack = ser.read(ser.in_waiting)
                if b'K' in ack:
                    return True
            time.sleep(0.02)
        return False
    except Exception as e:
        print(f"\n串口错误: {e}")
        return False


def set_state(state):
    """切换运动状态（只在变化时发送）"""
    global current_state
    with lock:
        if state == current_state:
            return
        current_state = state
    ok = send(state)
    name = {'F': '前进', 'B': '后退', 'X': '停止'}.get(state, state)
    if ok:
        print(f"\r▶ {name}   (ESP32 回 K)")
    else:
        print(f"\r? {name} 发送但未确认")


def on_press(key):
    try:
        if key == keyboard.KeyCode.from_char('w') or key == keyboard.KeyCode.from_char('W'):
            set_state('F')   # 前进
        elif key == keyboard.KeyCode.from_char('s') or key == keyboard.KeyCode.from_char('S'):
            set_state('B')   # 后退
        elif key == keyboard.KeyCode.from_char('q') or key == keyboard.KeyCode.from_char('Q'):
            print("\n退出")
            return False
    except Exception:
        pass


def on_release(key):
    # 松开 W 或 S → 停止
    try:
        if key == keyboard.KeyCode.from_char('w') or key == keyboard.KeyCode.from_char('W') or \
           key == keyboard.KeyCode.from_char('s') or key == keyboard.KeyCode.from_char('S'):
            set_state('X')
    except Exception:
        pass


def main():
    global ser
    try:
        ser = serial.Serial(PORT, BAUD, timeout=0.5)
    except Exception as e:
        print(f"✗ 打不开 {PORT}: {e}")
        sys.exit(1)

    print(f"=== RDK 前后控制 (串口 {PORT}) ===")
    print("按住 W 前进, 按住 S 后退, 松开即停, Q 退出")

    # 启动监听
    listener = keyboard.Listener(on_press=on_press, on_release=on_release)
    listener.start()

    try:
        while listener.is_alive():
            time.sleep(0.1)
    except KeyboardInterrupt:
        print("\n退出")
    finally:
        send('X')   # 确保停车
        ser.close()
        listener.stop()


if __name__ == "__main__":
    main()
