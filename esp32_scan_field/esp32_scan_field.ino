/*
 * esp32_scan_field.ino —— 慢衰减电机 + 4路VL53L0X连续扫描（场地实测版）
 * -------------------------------------------------------------------
 * 相比 esp32_max_test 增加「连续流式测距」，一次烧录即可完成场地扫描测试。
 *
 * 指令（RDK → ESP32, 115200）：
 *   W  前进（半速，慢衰减）
 *   S  后退（半速，慢衰减）
 *   X  停止
 *   T  单次读四方向 → 回 "T:F R B L"
 *   C  开始连续流式测距（约 10Hz，每帧 "D:F R B L"）
 *   E  停止流式测距
 *   其余单字符命令执行后回 'K'
 *
 * 单帧输出格式：D:1234 567 4321 890     （前 右 后 左，单位 mm，-1 表示超量程/无效）
 *
 * 接线：
 *   电机 左 IN1=2 IN2=4   右 IN1=13 IN2=27
 *   VL53L0X XSHUT：前18 后23 左5 右19，I2C 板载 21/22
 *   RDK UART：GPIO32=RX, GPIO33=TX, GND 共地
 */

#include <Wire.h>
#include <Adafruit_VL53L0X.h>

#define USE_UART 1
#if USE_UART
HardwareSerial RDK(1);
#define UART_RX 32
#define UART_TX 33
#endif

// ============ 电机 ============
#define L_IN1 2
#define L_IN2 4
#define R_IN1 13
#define R_IN2 27

#define PWM_FREQ 20000
#define PWM_RES  8
#define CRUISE_SPEED 128     // 半速（255 的一半），慢衰减下扭矩足够
#define SPIN_SPEED   140     // 原地转向速度（要克服更大的静摩擦）

#define MPU_ADDR 0x68        // 板载陀螺仪（WHO_AM_I=0x68，MPU6050 兼容）

// ============ VL53L0X ============
#define VL_FRONT_XSHUT 18
#define VL_BACK_XSHUT  23
#define VL_LEFT_XSHUT  5
#define VL_RIGHT_XSHUT 19

#define TIMING_BUDGET_US 20000   // 单次测量 20ms，四路一轮约 80ms ≈ 12Hz

Adafruit_VL53L0X sensorFront = Adafruit_VL53L0X();
Adafruit_VL53L0X sensorBack  = Adafruit_VL53L0X();
Adafruit_VL53L0X sensorLeft  = Adafruit_VL53L0X();
Adafruit_VL53L0X sensorRight = Adafruit_VL53L0X();

// ============ 状态 ============
bool streaming = false;
unsigned long lastStreamMs = 0;
const unsigned long STREAM_PERIOD_MS = 100;

// 各路传感器是否初始化成功（失败的路一律不碰，直接返回 -1，避免卡死重启）
bool okF = false, okR = false, okB = false, okL = false;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
static void pwmInit() {
  ledcAttach(L_IN1, PWM_FREQ, PWM_RES);
  ledcAttach(L_IN2, PWM_FREQ, PWM_RES);
  ledcAttach(R_IN1, PWM_FREQ, PWM_RES);
  ledcAttach(R_IN2, PWM_FREQ, PWM_RES);
}
#else
#define CH_L1 0
#define CH_L2 1
#define CH_R1 2
#define CH_R2 3
static void pwmInit() {
  ledcSetup(CH_L1, PWM_FREQ, PWM_RES); ledcAttachPin(L_IN1, CH_L1);
  ledcSetup(CH_L2, PWM_FREQ, PWM_RES); ledcAttachPin(L_IN2, CH_L2);
  ledcSetup(CH_R1, PWM_FREQ, PWM_RES); ledcAttachPin(R_IN1, CH_R1);
  ledcSetup(CH_R2, PWM_FREQ, PWM_RES); ledcAttachPin(R_IN2, CH_R2);
}
#endif

// 慢衰减驱动：IN1 常高，IN2 写 255-speed，刹车区间电流回流，低速扭矩大
static void carForward() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(L_IN1, 255); ledcWrite(L_IN2, 255 - CRUISE_SPEED);
  ledcWrite(R_IN1, 255); ledcWrite(R_IN2, 255 - CRUISE_SPEED);
#else
  ledcWrite(CH_L1, 255); ledcWrite(CH_L2, 255 - CRUISE_SPEED);
  ledcWrite(CH_R1, 255); ledcWrite(CH_R2, 255 - CRUISE_SPEED);
#endif
}

static void carBackward() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(L_IN2, 255); ledcWrite(L_IN1, 255 - CRUISE_SPEED);
  ledcWrite(R_IN2, 255); ledcWrite(R_IN1, 255 - CRUISE_SPEED);
#else
  ledcWrite(CH_L2, 255); ledcWrite(CH_L1, 255 - CRUISE_SPEED);
  ledcWrite(CH_R2, 255); ledcWrite(CH_R1, 255 - CRUISE_SPEED);
#endif
}

static void carStop() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(L_IN1, 0); ledcWrite(L_IN2, 0);
  ledcWrite(R_IN1, 0); ledcWrite(R_IN2, 0);
#else
  ledcWrite(CH_L1, 0); ledcWrite(CH_L2, 0);
  ledcWrite(CH_R1, 0); ledcWrite(CH_R2, 0);
#endif
}

// 原地左转：左轮后退、右轮前进（均慢衰减）
static void carSpinLeft() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(L_IN2, 255); ledcWrite(L_IN1, 255 - SPIN_SPEED);
  ledcWrite(R_IN1, 255); ledcWrite(R_IN2, 255 - SPIN_SPEED);
#else
  ledcWrite(CH_L2, 255); ledcWrite(CH_L1, 255 - SPIN_SPEED);
  ledcWrite(CH_R1, 255); ledcWrite(CH_R2, 255 - SPIN_SPEED);
#endif
}

// 原地右转：左轮前进、右轮后退
static void carSpinRight() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(L_IN1, 255); ledcWrite(L_IN2, 255 - SPIN_SPEED);
  ledcWrite(R_IN2, 255); ledcWrite(R_IN1, 255 - SPIN_SPEED);
#else
  ledcWrite(CH_L1, 255); ledcWrite(CH_L2, 255 - SPIN_SPEED);
  ledcWrite(CH_R2, 255); ledcWrite(CH_R1, 255 - SPIN_SPEED);
#endif
}

// ===== MPU6050 航向角 =====
// WHO_AM_I 已确认为 0x68；陀螺量程 ±500dps（65.5 LSB/dps），只积分 Z 轴
static float yawDeg = 0.0;
static float gyroBiasZ = 0.0;
static unsigned long lastGyroUs = 0;

static void mpuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static bool mpuReadGyroZ(int16_t &raw) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x47);                 // GYRO_ZOUT_H
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, (uint8_t)2) != 2) return false;
  uint8_t hi = Wire.read(), lo = Wire.read();
  raw = (int16_t)((hi << 8) | lo);
  return true;
}

static void mpuInit() {
  mpuWrite(0x6B, 0x00);   // 解除睡眠
  delay(50);
  mpuWrite(0x1B, 0x08);   // 陀螺 ±500 dps
  mpuWrite(0x19, 0x04);   // DLPF 约 20Hz 输出，压噪声
  delay(50);

  // 静止时采 200 个样本求零偏（约 200ms）
  int32_t sum = 0;
  int n = 0;
  int16_t raw;
  for (int i = 0; i < 200; i++) {
    if (mpuReadGyroZ(raw)) { sum += raw; n++; }
    delay(1);
  }
  gyroBiasZ = n ? (float)sum / n : 0.0;

  char buf[60];
  snprintf(buf, sizeof(buf), "MPU: bias=%d n=%d", (int)gyroBiasZ, n);
  RDK.println(buf);

  yawDeg = 0.0;
  lastGyroUs = micros();
}

// 取一次 Z 轴角速度并积分到 yaw（dt 由调用时刻自动算）
static void gyroPoll() {
  unsigned long now = micros();
  float dt = (now - lastGyroUs) / 1000000.0;
  lastGyroUs = now;
  if (dt <= 0.0 || dt > 0.5) return;   // 异常间隔丢弃

  int16_t raw;
  if (!mpuReadGyroZ(raw)) return;
  float dps = ((float)raw - gyroBiasZ) / 65.5;
  if (dps > -1.0 && dps < 1.0) dps = 0.0;   // 死区，抑制静止漂移
  yawDeg += dps * dt;                       // 左转为正（绕 Z 逆时针）
}

// 读四路，超量程、无效或该路未初始化成功都返回 -1
// 每读一路之前都插一次陀螺积分，避免长时间阻塞导致航向角采不到样本
static void readAll(int &f, int &r, int &b, int &l) {
  VL53L0X_RangingMeasurementData_t m;

  gyroPoll();
  if (okF) {
    sensorFront.rangingTest(&m, false);
    f = (m.RangeStatus != 4) ? (int)m.RangeMilliMeter : -1;
  } else f = -1;

  gyroPoll();
  if (okR) {
    sensorRight.rangingTest(&m, false);
    r = (m.RangeStatus != 4) ? (int)m.RangeMilliMeter : -1;
  } else r = -1;

  gyroPoll();
  if (okB) {
    sensorBack.rangingTest(&m, false);
    b = (m.RangeStatus != 4) ? (int)m.RangeMilliMeter : -1;
  } else b = -1;

  gyroPoll();
  if (okL) {
    sensorLeft.rangingTest(&m, false);
    l = (m.RangeStatus != 4) ? (int)m.RangeMilliMeter : -1;
  } else l = -1;

  gyroPoll();
}

// 扫一遍 I2C 总线，把在线的地址打出来（诊断用）
static void scanBus(const char *tag) {
  char buf[80];
  int n = snprintf(buf, sizeof(buf), "BUS[%s]:", tag);
  int cnt = 0;
  for (uint8_t a = 0x08; a < 0x78; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      n += snprintf(buf + n, sizeof(buf) - n, " %02X", a);
      cnt++;
      if (n > 60) break;
    }
  }
  if (cnt == 0) snprintf(buf + n, sizeof(buf) - n, " none");
  RDK.println(buf);
}

static void vlInit() {
  pinMode(VL_FRONT_XSHUT, OUTPUT); pinMode(VL_BACK_XSHUT, OUTPUT);
  pinMode(VL_LEFT_XSHUT, OUTPUT);  pinMode(VL_RIGHT_XSHUT, OUTPUT);
  digitalWrite(VL_FRONT_XSHUT, LOW); digitalWrite(VL_BACK_XSHUT, LOW);
  digitalWrite(VL_LEFT_XSHUT, LOW);  digitalWrite(VL_RIGHT_XSHUT, LOW);
  delay(10);
  Wire.begin();
  delay(20);

  // 全部 XSHUT 拉低时总线上不该有任何设备；若有，说明那一路 XSHUT 线没接上（悬空=常亮）
  scanBus("all_off");

  digitalWrite(VL_FRONT_XSHUT, HIGH); delay(30);
  scanBus("F_on");
  okF = sensorFront.begin(0x30, false, &Wire);
  RDK.println(okF ? "VL F OK" : "VL F FAIL");
  scanBus("F_after");

  digitalWrite(VL_BACK_XSHUT, HIGH); delay(30);
  scanBus("B_on");
  okB = sensorBack.begin(0x31, false, &Wire);
  RDK.println(okB ? "VL B OK" : "VL B FAIL");

  digitalWrite(VL_LEFT_XSHUT, HIGH); delay(30);
  scanBus("L_on");
  okL = sensorLeft.begin(0x32, false, &Wire);
  RDK.println(okL ? "VL L OK" : "VL L FAIL");

  digitalWrite(VL_RIGHT_XSHUT, HIGH); delay(30);
  scanBus("R_on");
  okR = sensorRight.begin(0x33, false, &Wire);
  RDK.println(okR ? "VL R OK" : "VL R FAIL");
  scanBus("final");

  // 只对初始化成功的传感器设测量时序，失败的一律不碰（否则会死等导致看门狗重启）
  if (okF) sensorFront.setMeasurementTimingBudgetMicroSeconds(TIMING_BUDGET_US);
  if (okB) sensorBack.setMeasurementTimingBudgetMicroSeconds(TIMING_BUDGET_US);
  if (okL) sensorLeft.setMeasurementTimingBudgetMicroSeconds(TIMING_BUDGET_US);
  if (okR) sensorRight.setMeasurementTimingBudgetMicroSeconds(TIMING_BUDGET_US);
}

// ===== 0x68 惯性传感器识别 =====
// 读 0x68 上几个候选芯片的 WHO_AM_I 寄存器，判断板上到底是哪颗
static uint8_t i2cReadReg(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0xFF;
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) return 0xFF;
  return Wire.read();
}

static void gyroProbe() {
  char buf[120];
  uint8_t mpu = i2cReadReg(0x68, 0x75);   // MPU6050=0x68, MPU6500=0x70, ICM20689=0x98
  uint8_t qmi = i2cReadReg(0x68, 0x00);   // QMI8658  WHO_AM_I
  uint8_t lis = i2cReadReg(0x68, 0x0F);   // LIS3DH/LSM  WHO_AM_I
  snprintf(buf, sizeof(buf), "GYRO: 0x68 r75=%02X r00=%02X r0F=%02X", mpu, qmi, lis);
  RDK.println(buf);
  // 打印识别依据：MPU6050/6050兼容常见 0x68/0x70/0x98；QMI8658 为 0x05；LIS 系列 0x33
}

void setup() {
#if USE_UART
  RDK.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
#endif
  pinMode(L_IN1, OUTPUT); pinMode(L_IN2, OUTPUT);
  pinMode(R_IN1, OUTPUT); pinMode(R_IN2, OUTPUT);
  pwmInit();
  carStop();

  vlInit();
  gyroProbe();
  mpuInit();

#if USE_UART
  RDK.println("READY");
#endif
}

void loop() {
#if USE_UART
  while (RDK.available()) {
    char c = RDK.read();
    if (c == '\n' || c == '\r') continue;

    if (c == 'W') {
      carForward();
      RDK.write('K');
    } else if (c == 'S') {
      carBackward();
      RDK.write('K');
    } else if (c == 'A') {
      carSpinLeft();
      RDK.write('K');
    } else if (c == 'D') {
      carSpinRight();
      RDK.write('K');
    } else if (c == 'Z') {
      yawDeg = 0.0;
      RDK.println("YAW:0.0");
    } else if (c == 'X') {
      carStop();
      RDK.write('K');
    } else if (c == 'T') {
      int f, r, b, l;
      readAll(f, r, b, l);
      RDK.printf("T:%d %d %d %d %d\n", f, r, b, l, (int)(yawDeg * 10));
    } else if (c == 'C') {
      streaming = true;
      lastStreamMs = 0;      // 立刻吐第一帧
      RDK.write('K');
    } else if (c == 'E') {
      streaming = false;
      RDK.write('K');
    } else if (c == 'G') {
      gyroProbe();
    }
  }

  if (streaming && (millis() - lastStreamMs >= STREAM_PERIOD_MS)) {
    lastStreamMs = millis();
    int f, r, b, l;
    readAll(f, r, b, l);
    RDK.printf("D:%d %d %d %d %d\n", f, r, b, l, (int)(yawDeg * 10));
  }
#endif
  gyroPoll();     // 空闲时也持续积分，保证航向角不断档
  delay(2);
}

// [版本] v1.2.0 2026-11-08 14:15 —— MPU6050 航向角（Z 轴积分+零偏标定+死区）、A/D 原地转向、Z 归零；流式帧加第 5 字段 yaw
