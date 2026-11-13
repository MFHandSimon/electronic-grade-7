#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_serial_probe.py —— RDK 串口探测
======================================
依次尝试打开 ttyS1 / ttyS3，各发送一个字符 'F'，读回 ESP32 的回复，
判断哪个串口设备能与 ESP32 通信。

用法（RDK 上）：
    python3 test_serial_probe.py
"""
import serial
import time

PORTS = ["/dev/ttyS1", "/dev/ttyS3"]
BAUD = 115200


def probe(port):
    try:
        ser = serial.Serial(port, BAUD, timeout=0.5)
        time.sleep(0.3)
        print(f"\n[OK] 打开 {port} @ {BAUD}")
        # 清空接收缓冲
        ser.reset_input_buffer()
        # 发送 'F'（前进）
        ser.write(b"F\n")
        ser.flush()
        print(f"[->] 已发送: F")
        # 等 0.5 秒读 ESP32 回复
        time.sleep(0.5)
        n = ser.in_waiting
        if n:
            data = ser.read(n)
            print(f"[<-] ESP32 回复: {data.decode(errors='ignore').strip()}")
        else:
            print("[..] 未收到回复（可能接线/设备名不对，或 ESP32 未回传）")
        ser.close()
        return True
    except Exception as e:
        print(f"[X] {port} 打开失败: {e}")
        return False


def main():
    print("=== RDK 串口探测（ttyS1 / ttyS3）===")
    ok = False
    for p in PORTS:
        if probe(p):
            ok = True
    if not ok:
        print("\n❌ 两个串口都没能打开/通信。")
        print("   检查：接线（TXD↔RX 交叉）、共地、设备名。")
    else:
        print("\n✅ 至少一个串口已尝试通信。若有回复，说明 ESP32 已响应；")
        print("   若无回复但打开成功，可能是 ESP32 未回传（正常，只收指令）。")


if __name__ == "__main__":
    main()
