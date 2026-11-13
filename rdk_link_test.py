#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
rdk_link_test.py —— RDK↔ESP32 UART 链路测试
===========================================
用途：确认 RDK X3 与 ESP32 之间 UART 是否打通。
方法：尝试多个候选串口设备，向 ESP32 发 'T'，
      若收到回显 'T' 或 ESP32 的 'ESP32_ONLINE'，则该设备链路通。

用法（RDK 上）：
    python3 rdk_link_test.py                    # 自动遍历 /dev/ttyS1 /dev/ttyS3
    python3 rdk_link_test.py --port /dev/ttyS3  # 指定设备
    python3 rdk_link_test.py --list             # 只列出可用串口

对应接线（RDK 40PIN → ESP32，交叉）：
    RDK TXD → ESP32 RX (GPIO32)
    RDK RXD ← ESP32 TX (GPIO33)
    RDK GND → ESP32 GND
"""

import serial
import serial.tools.list_ports
import argparse
import time
import glob
import sys


def list_devices():
    """列出系统可用串口设备"""
    ports = []
    # 常见 UART 设备名
    for p in glob.glob('/dev/ttyS*') + glob.glob('/dev/ttyUSB*') + \
            glob.glob('/dev/ttyACM*') + glob.glob('/dev/ttyTHS*'):
        if 'printk' in p or p.endswith('S0'):   # 跳过调试口
            continue
        ports.append(p)
    return ports


def test_one(port, baud=115200, timeout=1.0):
    """打开串口，发 'T'，等回显。返回 True=通"""
    try:
        ser = serial.Serial(port, baud, timeout=timeout)
    except Exception as e:
        print(f"  ✗ 打不开 {port}: {e}")
        return None

    try:
        # 先清空缓冲区
        ser.reset_input_buffer()
        # 发测试字节
        ser.write(b'T')
        ser.flush()

        # 读回显，最多等 1s
        deadline = time.time() + 1.0
        received = b''
        while time.time() < deadline:
            n = ser.in_waiting
            if n:
                received += ser.read(n)
                # 若收到回显 T 即成功
                if b'T' in received or b'ESP32_ONLINE' in received:
                    print(f"  ✓ {port} 链路通！收到: {received!r}")
                    ser.close()
                    return True
            time.sleep(0.02)
        # 超时未收到
        print(f"  ✗ {port} 发送成功但未收到回显 (收到: {received!r})")
        ser.close()
        return False
    except Exception as e:
        print(f"  ✗ {port} 出错: {e}")
        try:
            ser.close()
        except Exception:
            pass
        return False


def main():
    ap = argparse.ArgumentParser(description="RDK↔ESP32 UART 链路测试")
    ap.add_argument("--port", default=None, help="指定串口设备，如 /dev/ttyS3")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--list", action="store_true", help="只列出可用串口")
    args = ap.parse_args()

    if args.list:
        devs = list_devices()
        print("可用串口设备:")
        for d in devs:
            print(f"  {d}")
        sys.exit(0)

    if args.port:
        devs = [args.port]
    else:
        devs = list_devices()
        if not devs:
            print("未找到可用串口设备（试 --list 或指定 --port）")
            sys.exit(1)

    print(f"=== RDK↔ESP32 UART 链路测试 (波特率 {args.baud}) ===")
    print("请确认 ESP32 已烧录 esp32_uart_link.ino 且已接好杜邦线。")
    print("提示: 上电后 ESP32 应主动发 'ESP32_ONLINE\\n'\n")

    ok = False
    for d in devs:
        print(f"测试 {d} ...")
        r = test_one(d, args.baud)
        if r:
            ok = True
            break

    print()
    if ok:
        print("✅ 链路已打通！RDK ↔ ESP32 通信正常。")
    else:
        print("❌ 所有设备均未打通。请检查：")
        print("   1) 接线是否交叉（RDK TX→ESP32 RX32, RDK RX←ESP32 TX33, 共地）")
        print("   2) ESP32 是否烧了 esp32_uart_link.ino 且已上电")
        print("   3) 换一组 RDK 引脚试（不同串口设备对应不同引脚）")


if __name__ == "__main__":
    main()
