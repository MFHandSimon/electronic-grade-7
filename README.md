# 电子学会柒级 · 智能小车（ESP32 + RDK X3）

四路 VL53L0X 激光测距 + MPU6050 航向角的小车，做定点 360° 旋转扫描，
把点云拟合成俯视图，验证场地边界。当前已跑通「单点位扫描 → 矩形拟合」。

## 目录结构

```
小车控制/
├── esp32_scan_field/            主固件 v1.2.0（Arduino IDE 工程）
├── esp32_max_test/              上一代测试固件
├── esp32_*.ino                  各阶段单文件固件（历史留档）
├── rdk_*.py                     RDK X3 端脚本
├── rdk_scripts/                 RDK 上实际使用的脚本（/app 目录同步版）
├── fit_rect.py                  矩形拟合（假设车在场地中心）
├── fit_rect2.py                 矩形拟合（带旋转角+偏移，任意车位）
├── plot_scan.py                 点云俯视图绘制
├── scan360_*.csv                360° 扫描实测数据
├── slow_142210.csv              慢速遍历数据
├── 扫描俯视图_*.png              各轮扫描生成的俯视图
└── 进度与下周计划_v1.0.0.md       完整进度存档与周计划
```

## 硬件

| 项目 | 引脚 / 参数 |
|---|---|
| 左电机 | LEFT_IN1 = GPIO2，LEFT_IN2 = GPIO4 |
| 右电机 | RIGHT_IN1 = GPIO13，RIGHT_IN2 = GPIO27 |
| VL53L0X XSHUT | 前 18(0x30)、后 23(0x31)、左 5(0x32)、右 19(0x33) |
| I2C | SCL = GPIO21，SDA = GPIO22 |
| MPU6050 | I2C 0x68（板载） |
| ESP32↔RDK | GPIO32=RX，GPIO33=TX，115200 8N1，必须共地 |
| RDK 串口 | /dev/ttyS3 |

PWM：20kHz / 8bit，CRUISE_SPEED=128，SPIN_SPEED=140

## 固件命令（v1.2.0）

| 命令 | 作用 |
|---|---|
| W / S / X | 前进 / 后退 / 停 |
| A / D | 原地左转 / 右转 |
| Z | 航向角归零 |
| T | 单次读 → `T:f r b l yaw×10` |
| C / E | 开 / 停流式 → `D:f r b l yaw×10` |
| G | 陀螺探针 |

**注意三点**
1. 电机必须用**慢衰减**（IN1=255、IN2=255-SPEED），快衰减低速没扭矩
2. 数据帧是 **5 个字段**，末尾多了 yaw
3. 串口只能用 pyserial 读，`cat /dev/ttyS3` 读不到

## 关键结论（踩坑换来的）

- **传感器不能有向下俯角**。俯角 11.5° 时地面距离只有 250mm，VL53L0X 优先返回最近物体，墙面永远被地面挡住。把镜头侧垫高 2–3mm 让它微微上仰，问题消失。
- **车必须放在场地中心附近**。放角落时对角距离 1.94m 逼近量程上限，会大量返回 -1。
- **扫描步数 12 步 × 30° = 360°**，写成 24 步会转两圈。
- **串口驱动机器人的脚本必须 try/finally 发 X**，否则脚本崩溃后车会保持最后一条运动指令一直冲。

## 已验证结果

中心位 360° 扫描（48 点），矩形拟合 **1270 × 1480 mm，平均残差 39.8mm**，
与卷尺实测 1.26 / 1.48 m 误差在 1cm 以内。

## 复现

```bash
# ESP32 编译（Windows）
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 esp32_scan_field

# 绘图（任意机器，无需 matplotlib）
python plot_scan.py scan360_run4.csv --png out.png
python fit_rect.py scan360_run4.csv --png out.png
```
