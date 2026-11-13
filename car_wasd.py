#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
car_wasd.py —— WASD 遥控小车（RDK X3 上位机）
=================================================
在 RDK X3 上运行，读取键盘 WASD 控制指令，通过串口发给 ESP32 下位机，
ESP32 解析后驱动电机，实现小车前进/后退/左转/右转/停止。

控制键：
    W  前进
    S  后退
    A  原地左转
    D  原地右转
    空格  停止
    Q  退出程序

运行：
    python3 car_wasd.py --port /dev/ttyS1     # 你实测 RDK 能用的串口（推荐）
    python3 car_wasd.py --port /dev/ttyS3     # RDK 40PIN 默认 UART3
    python3 car_wasd.py --port /dev/ttyUSB0   # USB 转串口

依赖：pynput（键盘监听）。RDK 上安装：
    pip3 install pynput
（若 RDK 无法安装 pynput，本脚本提供备用的 input() 逐键模式，见 README）
"""

import argparse
import time
import serial  # pyserial


# ============ 指令定义（与 ESP32 固件约定）============
# 每个指令发送一个字符 + 换行，ESP32 收到后执行
CMD = {
    "forward":  "F",   # 前进
    "backward": "B",   # 后退
    "turn_left": "L",  # 原地左转
    "turn_right": "R", # 原地右转
    "stop":     "S",   # 停止
}


def open_serial(port, baud=115200):
    try:
        ser = serial.Serial(port, baud, timeout=0.1)
        time.sleep(0.5)
        print(f"✅ 串口已打开: {port} @ {baud}")
        return ser
    except Exception as e:
        print(f"❌ 无法打开串口 {port}: {e}")
        print("   请检查：ESP32 是否已连接、设备名是否正确（ls /dev/ttyUSB*）")
        return None


def send_cmd(ser, cmd):
    if ser is None:
        return
    try:
        ser.write((cmd + "\n").encode())
        print(f"→ 发送: {cmd}")
    except Exception as e:
        print(f"⚠️ 发送失败: {e}")


def read_back(ser):
    """读 ESP32 的反馈（可选，用于调试）。"""
    if ser is None:
        return
    try:
        data = ser.read(ser.in_waiting or 1)
        if data:
            print(f"← ESP32: {data.decode(errors='ignore').strip()}")
    except Exception:
        pass


def mode_terminal(ser):
    """终端逐键模式（pynput 不可用时用）。每行输一个指令字母。"""
    print("\n=== 终端控制模式 ===")
    print("输入 F(前进) B(后退) L(左转) R(右转) S(停止) Q(退出)")
    while True:
        try:
            k = input("> ").strip().upper()
        except (EOFError, KeyboardInterrupt):
            break
        if k == "Q":
            break
        if k in ("F", "B", "L", "R", "S"):
            send_cmd(ser, k)
            read_back(ser)
        else:
            print("无效指令，请输入 F/B/L/R/S/Q")


def mode_keyboard(ser):
    """pynput 键盘模式：按住 W/A/S/D 控制，松开发送停止。"""
    try:
        from pynput import keyboard
    except ImportError:
        print("⚠️ 未安装 pynput，切换为终端逐键模式")
        mode_terminal(ser)
        return

    pressed = set()
    moving = False

    def on_press(key):
        nonlocal moving
        try:
            k = key.char.lower() if hasattr(key, 'char') and key.char else None
        except Exception:
            return
        if not k:
            return
        if k in ('w', 'a', 's', 'd'):
            pressed.add(k)
        elif k == 'q':
            send_cmd(ser, CMD["stop"])
            return False

    def on_release(key):
        nonlocal moving
        try:
            k = key.char.lower() if hasattr(key, 'char') and key.char else None
        except Exception:
            return
        if k in ('w', 'a', 's', 'd'):
            pressed.discard(k)

    # 轮询 pressed 集合，生成指令
    import threading
    stop = threading.Event()

    def loop():
        nonlocal moving
        last_cmd = None
        while not stop.is_set():
            if 'w' in pressed:
                cmd = CMD["forward"]
            elif 's' in pressed:
                cmd = CMD["backward"]
            elif 'a' in pressed:
                cmd = CMD["turn_left"]
            elif 'd' in pressed:
                cmd = CMD["turn_right"]
            else:
                cmd = CMD["stop"]
            if cmd != last_cmd:
                send_cmd(ser, cmd)
                last_cmd = cmd
            read_back(ser)
            time.sleep(0.1)

    listener = keyboard.Listener(on_press=on_press, on_release=on_release)
    listener.start()
    print("\n=== WASD 键盘控制模式 ===")
    print("按 W/A/S/D 控制，松开即停止；Q 退出")
    try:
        loop_t = threading.Thread(target=loop, daemon=True)
        loop_t.start()
        while True:
            time.sleep(0.2)
            if not listener.is_alive():
                break
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        listener.stop()
        send_cmd(ser, CMD["stop"])
        print("\n已停止")


def main():
    ap = argparse.ArgumentParser(description="WASD 遥控小车 (RDK上位机)")
    ap.add_argument("--port", default="/dev/ttyS1", help="串口设备，默认 /dev/ttyS1 (你实测 RDK UART)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--keyboard", action="store_true", help="用 pynput 键盘模式")
    ap.add_argument("--terminal", action="store_true", help="用终端逐键模式")
    args = ap.parse_args()

    ser = open_serial(args.port, args.baud)
    if ser is None:
        return

    if args.terminal:
        mode_terminal(ser)
    else:
        mode_keyboard(ser)

    if ser:
        ser.close()
        print("串口已关闭")


if __name__ == "__main__":
    main()
