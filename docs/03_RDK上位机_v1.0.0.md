# 03 · RDK X3 上位机与串口通信

> 目标：在 RDK 上能发一条 `T` 收到四方向距离，能发 `W/X` 让小车走和停。
> ESP32 已经烧好、串口线已接（GPIO32←RDK TX，GPIO33→RDK RX，共地）。

---

## 一、连上 RDK

### 方式 A：SSH（推荐，用你自己的电脑操作）

RDK 上电联网后，在它屏幕上查 IP（`ifconfig` / `ip a`），然后在你电脑上：

```powershell
ssh root@<RDK的IP>
```

默认密码按你们板子的出厂说明（通常是 `root`）。

### 方式 B：直接接显示器键盘

也行，只是敲命令累一点。

---

## 二、装依赖（只做一次）

```bash
python3 -c "import serial; print(serial.__version__)"
```

- 有版本号输出 → 已经装好，跳过
- 报 `ModuleNotFoundError: No module named 'serial'` → 装一下：

```bash
pip3 install pyserial
```

如果 `pip3` 也没有：

```bash
apt update && apt install -y python3-pip
```

---

## 三、确认串口

```bash
ls -l /dev/ttyS3
```

能看到 `/dev/ttyS3` 就对了。看不到的话检查 RDK 是否把该 UART 启用了（40pin 上的 UART 引脚）。

**权限**：如果打开时报 `Permission denied`，用 `sudo` 跑，或者：

```bash
sudo usermod -aG dialout $USER
```

（改完要重新登录才生效，图省事就全程 `sudo python3 xxx.py`。）

---

## 四、第一件事：读 8 秒串口

**不要用 `cat /dev/ttyS3`**。`cat` 不会设波特率，永远是空的，会白白浪费半小时。
必须用 pyserial 明确指定 115200。

```bash
python3 -c "
import serial,time
s=serial.Serial('/dev/ttyS3',115200,timeout=0.5)
t=time.time()
while time.time()-t<8:
    d=s.read(400)
    if d: print(d.decode(errors='ignore'),end='',flush=True)
"
```

**然后按一下 ESP32 板上的 RST 键**，应该刷出：

```
BUS[all_off]: 68
...
VL R OK
...
READY
```

- 看到 `READY` → 链路通了，往下走
- 一片空白 → 见 `05_排障手册` 第 3 条（先发后听、共地、交叉接线）

> 为什么必须按 RST？因为 `READY` 是开机时打印一次的。你开监听的时候它早就打完了。
> 想看当前状态就直接发 `T`，那个是即时回复。

---

## 五、发指令测响应

```bash
python3 -c "
import serial,time
s=serial.Serial('/dev/ttyS3',115200,timeout=0.3)
time.sleep(0.2); s.reset_input_buffer()
s.write(b'T'); time.sleep(0.3)
print(s.readline().decode(errors='ignore').strip())
"
```

正常输出形如：

```
T:573 982 610 845 37
```

含义：前 573 / 右 982 / 后 610 / 左 845 mm，航向 3.7°。

> 如果读到的是 `? 未知指令: T` 之类，说明你在跟**旧版本固件**说话，回 `02` 重烧 v1.2.0。

---

## 六、让车动起来（先架空轮子！）

```bash
# 前进 1 秒然后停
python3 -c "
import serial,time
s=serial.Serial('/dev/ttyS3',115200,timeout=0.2)
time.sleep(0.2)
try:
    s.write(b'W'); time.sleep(1.0)
finally:
    s.write(b'X')          # 必须放在 finally 里
    s.close()
print('done')
"
```

### 为什么必须 `try/finally`

固件是「收到 W 就一直走，直到收到 X」。如果脚本中途崩溃、Ctrl+C、
或者解析数据时抛异常，**车会保持最后一条运动指令一直冲，撞墙了轮子还在转**。

实测踩过的坑：脚本按 4 个字段解析 5 字段的帧 → 第一帧就 `ValueError` 崩掉 →
固件停在 `W` → 车顶着墙继续转。

所以：**任何驱动电机的脚本，收尾都要发 `X`，并且写在 `finally` 里。**

---

## 七、项目里现成的脚本

仓库 `rdk_scripts/` 下三个脚本，拷到 RDK 的 `/app` 目录用：

```bash
# 从你自己的电脑传过去（在电脑上执行）
scp rdk_scripts/s360.py rdk_scripts/slowrun.py rdk_scripts/step_scan.py root@<RDK的IP>:/app/
```

| 脚本 | 用途 | 用法 |
|---|---|---|
| `s360.py` | 定点转一圈，每 30° 采一次四向距离 | `python3 /app/s360.py` |
| `slowrun.py` | 慢速前进遍历（走 0.3s 停 0.15s），测前方变化 | `python3 /app/slowrun.py` |
| `step_scan.py` | 前进到离前墙 550mm 停下，再扫一圈 | `python3 /app/step_scan.py` |

> 没有 `scp` 或者传不上去，就在 RDK 上 `nano /app/s360.py`，把仓库里文件内容整段粘进去，`Ctrl+O` 保存、`Ctrl+X` 退出。

---

## 八、去遥控（可选）

想在电脑上按 W/A/S/D 遥控，用仓库里的 `rdk_car_terminal.py` 或 `car_wasd.py`，
它们的作用就是把按键转成单字符发到串口。课上时间紧的话，直接用上一节的 python 一行命令更省事。

---

## 本节自检

- [ ] `python3 -c "import serial"` 不报错
- [ ] 8 秒监听 + 按 RST 能看到 `READY`
- [ ] 发 `T` 能收到 5 个数字
- [ ] 发 `W` 车走、发 `X` 车停（架空测试）
- [ ] 知道「驱动脚本必须 try/finally 发 X」这条规矩

四条全过 → 去 `04_扫描与建图`。
