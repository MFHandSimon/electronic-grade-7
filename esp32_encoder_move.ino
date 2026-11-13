/*
 * esp32_encoder_move.ino —— 编码器精确运动 + 测距走廊测试（ESP32）
 * =================================================================
 * 功能：
 *   1) 编码器闭环精确运动（前进/后退指定距离、原地转指定角度）
 *   2) 走廊直线测试：走 3 米直线，边走边用左右 VL53L0X 测距，
 *      每走约 100mm 回传一行 "L:xx R:xx"，供 RDK 画走廊图判断走姿。
 *
 * 编码器（实测扫描，与教材相反！）：
 *   每转 624 counts（2倍频）
 *   左编码器 → GPIO 16、17
 *   右编码器 → GPIO 25、26
 *
 * 几何参数（实测）：轮径 65mm，轮距 93mm，每 mm=3.0558 counts
 *
 * 测距（教材 7.2.5）：4× VL53L0X
 *   前 XSHUT=18 / 后=23 / 左=5 / 右=19，地址 前0x30 后0x31 左0x32 右0x33
 *
 * 电机驱动：双 H 桥（DRV8837）真 PWM
 *   前进 = IN1 输出 PWM、IN2=0；后退 = IN2 输出 PWM、IN1=0
 *   启动用 kickStart（满速100ms）再回落巡航，避免低速转不动
 *
 * 指令协议（RDK → ESP32, 115200）：
 *   F   前进一格(320mm)    B   后退一格(320mm)
 *   W   前进360mm          S   后退360mm
 *   L   左转90°            R   右转90°
 *   T   走廊测试：走3000mm，每100mm回传 "L:xx R:xx"，走完回K
 *   X   停止
 *   ESP32 动作完成回 'K'
 *
 * 接线：RDK 引脚8→ESP32 GPIO32(RX), 10→GPIO33(TX), GND→GND
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
#define LEFT_IN1   2
#define LEFT_IN2   4
#define RIGHT_IN1  13
#define RIGHT_IN2  27

// ============ 编码器（实测：左16/17、右25/26，与教材相反）============
#define ENC_L_A    16
#define ENC_L_B    17
#define ENC_R_A    25
#define ENC_R_B    26

// ============ PWM ============
#define PWM_FREQ   20000
#define PWM_RES    8
#define CRUISE_SPEED 170    // 巡航速度（够维持转动，比255慢）
#define TURN_SPEED  170     // 转向速度

// ============ 几何 ============
#define WHEEL_DIAM  65.0f
#define WHEEL_TRACK 93.0f
#define COUNTS_PER_TURN 624
#define COUNTS_PER_MM  (COUNTS_PER_TURN / (3.14159f * WHEEL_DIAM))
#define DIST_ONE_CELL  320

// ============ 测距引脚 ============
#define VL_FRONT_XSHUT 18
#define VL_BACK_XSHUT  23
#define VL_LEFT_XSHUT  5
#define VL_RIGHT_XSHUT 19
#define ADDR_FRONT 0x30
#define ADDR_BACK  0x31
#define ADDR_LEFT  0x32
#define ADDR_RIGHT 0x33

// 走廊测试参数
#define CORRIDOR_LEN   3000.0f   // 走 3 米
#define CORRIDOR_SAMPLE  100.0f  // 每 100mm 采样一次

Adafruit_VL53L0X sFront, sBack, sLeft, sRight;

// ============ 编码器中断 ============
volatile long encL = 0;
volatile long encR = 0;
void IRAM_ATTR isrEncL() { bool a=digitalRead(ENC_L_A), b=digitalRead(ENC_L_B); if(a!=b) encL++; else encL--; }
void IRAM_ATTR isrEncR() { bool a=digitalRead(ENC_R_A), b=digitalRead(ENC_R_B); if(a!=b) encR++; else encR--; }

// ============ 电机驱动（双 H 桥真 PWM，4 通道）============
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
static void pwmAttachAll(){
  ledcAttach(LEFT_IN1,  PWM_FREQ, PWM_RES);
  ledcAttach(LEFT_IN2,  PWM_FREQ, PWM_RES);
  ledcAttach(RIGHT_IN1, PWM_FREQ, PWM_RES);
  ledcAttach(RIGHT_IN2, PWM_FREQ, PWM_RES);
}
// 左电机：fwd=true 前进(IN1 PWM/IN2=0)，fwd=false 后退(IN2 PWM/IN1=0)
void motorLeft(int speed, bool fwd) {
  if (fwd) { ledcWrite(LEFT_IN2, 0); ledcWrite(LEFT_IN1, speed); }
  else     { ledcWrite(LEFT_IN1, 0); ledcWrite(LEFT_IN2, speed); }
}
void motorRight(int speed, bool fwd) {
  if (fwd) { ledcWrite(RIGHT_IN2, 0); ledcWrite(RIGHT_IN1, speed); }
  else     { ledcWrite(RIGHT_IN1, 0); ledcWrite(RIGHT_IN2, speed); }
}
void carStop() { ledcWrite(LEFT_IN1,0); ledcWrite(LEFT_IN2,0); ledcWrite(RIGHT_IN1,0); ledcWrite(RIGHT_IN2,0); }
#else
#define CH_L1 0
#define CH_L2 1
#define CH_R1 2
#define CH_R2 3
static void pwmAttachAll(){
  ledcSetup(CH_L1, PWM_FREQ, PWM_RES); ledcAttachPin(LEFT_IN1,  CH_L1);
  ledcSetup(CH_L2, PWM_FREQ, PWM_RES); ledcAttachPin(LEFT_IN2,  CH_L2);
  ledcSetup(CH_R1, PWM_FREQ, PWM_RES); ledcAttachPin(RIGHT_IN1, CH_R1);
  ledcSetup(CH_R2, PWM_FREQ, PWM_RES); ledcAttachPin(RIGHT_IN2, CH_R2);
}
void motorLeft(int speed, bool fwd) {
  if (fwd) { ledcWrite(CH_L2, 0); ledcWrite(CH_L1, speed); }
  else     { ledcWrite(CH_L1, 0); ledcWrite(CH_L2, speed); }
}
void motorRight(int speed, bool fwd) {
  if (fwd) { ledcWrite(CH_R2, 0); ledcWrite(CH_R1, speed); }
  else     { ledcWrite(CH_R1, 0); ledcWrite(CH_R2, speed); }
}
void carStop() { ledcWrite(CH_L1,0); ledcWrite(CH_L2,0); ledcWrite(CH_R1,0); ledcWrite(CH_R2,0); }
#endif

// 启动冲刺：满速100ms让轮子转起来，再回落巡航
void kickStart(bool lfwd, bool rfwd) {
  motorLeft(255, lfwd);
  motorRight(255, rfwd);
  delay(100);
}

void sendRDK(const char* s) {
#if USE_UART
  RDK.print(s);
#else
  Serial.print(s);
#endif
}
void sendRDK(char c) {
#if USE_UART
  RDK.write(c);
#else
  Serial.write(c);
#endif
}

// 读单个测距（返回 mm，失败返回 -1）
int readDist(Adafruit_VL53L0X& s) {
  VL53L0X_RangingMeasurementData_t m;
  s.rangingTest(&m, false);
  return (m.RangeStatus != 4) ? m.RangeMilliMeter : -1;
}

// ============ 精确运动（带超时保护）============
void moveDist(float dist_mm, bool forward) {
  long target = (long)(fabs(dist_mm) * COUNTS_PER_MM);
  encL=0; encR=0;
  kickStart(forward, forward);
  unsigned long t0 = millis();
  while (true) {
    long avg = (labs(encL)+labs(encR))/2;
    if (avg >= target) break;
    if (millis() - t0 > 10000) {   // 10秒超时，防编码器不计数死循环
      carStop();
      char b[48];
      sprintf(b, "ENC_TIMEOUT encL=%ld encR=%ld\n", (long)encL, (long)encR);
      sendRDK(b);
      sendRDK('K');
      return;
    }
    motorLeft(CRUISE_SPEED, forward); motorRight(CRUISE_SPEED, forward); delay(2);
  }
  carStop();
}

void turnAngle(float angle_deg) {
  bool lt = angle_deg > 0;
  float deg = fabs(angle_deg);
  long target = (long)((WHEEL_TRACK*3.14159f*deg/360.0f) * COUNTS_PER_MM);
  encL=0; encR=0;
  kickStart(!lt, lt);   // 左转=左轮反转、右轮正转
  unsigned long t0 = millis();
  while (true) {
    long avg = (labs(encL)+labs(encR))/2;
    if (avg >= target) break;
    if (millis() - t0 > 10000) {
      carStop();
      char b[48];
      sprintf(b, "ENC_TIMEOUT encL=%ld encR=%ld\n", (long)encL, (long)encR);
      sendRDK(b);
      sendRDK('K');
      return;
    }
    motorLeft(TURN_SPEED, !lt); motorRight(TURN_SPEED, lt); delay(2);
  }
  carStop();
}

// ============ 走廊测试 ============
void corridorTest() {
  long target = (long)(CORRIDOR_LEN * COUNTS_PER_MM);
  long sampleCounts = (long)(CORRIDOR_SAMPLE * COUNTS_PER_MM);
  long nextSample = sampleCounts;
  encL=0; encR=0;
  kickStart(true, true);
  while (true) {
    long avg = (labs(encL)+labs(encR))/2;
    if (avg >= target) break;
    motorLeft(CRUISE_SPEED, true); motorRight(CRUISE_SPEED, true);
    if (avg >= nextSample) {
      int l = readDist(sLeft);
      int r = readDist(sRight);
      char buf[32];
      sprintf(buf, "L:%d R:%d\n", l, r);
      sendRDK(buf);
      nextSample += sampleCounts;
    }
    delay(2);
  }
  carStop();
  sendRDK("K\n");
}

void execCmd(char c) {
  switch (c) {
    case 'F': moveDist(DIST_ONE_CELL, true);  break;
    case 'B': moveDist(DIST_ONE_CELL, false); break;
    case 'W': moveDist(360.0f, true);  break;
    case 'S': moveDist(360.0f, false); break;
    case 'L': turnAngle(90);  break;
    case 'R': turnAngle(-90); break;
    case 'T': corridorTest(); return;  // 自带 K\n
    case 'X': carStop(); break;
    default: return;
  }
  sendRDK('K');
}

// ============ 测距初始化（带超时保护）============
typedef struct { Adafruit_VL53L0X* p; uint8_t addr; int xshut; const char* name; } vls_t;
vls_t vls[] = { {&sFront, ADDR_FRONT, VL_FRONT_XSHUT, "F"},
                {&sBack,  ADDR_BACK,  VL_BACK_XSHUT,  "B"},
                {&sLeft,  ADDR_LEFT,  VL_LEFT_XSHUT,  "L"},
                {&sRight, ADDR_RIGHT, VL_RIGHT_XSHUT, "R"} };

bool vlBegin(Adafruit_VL53L0X& s, uint8_t addr) {
  unsigned long t0 = millis();
  while (millis() - t0 < 1200) {
    if (s.begin(addr, false, &Wire)) return true;
    delay(50);
  }
  return false;
}

void initVL53() {
  sendRDK("BOOT1\n");
  for (auto& v : vls) { pinMode(v.xshut, OUTPUT); digitalWrite(v.xshut, LOW); }
  delay(10);
  Wire.begin();
  sendRDK("BOOT2\n");
  for (auto& v : vls) {
    digitalWrite(v.xshut, HIGH); delay(20);
    if (vlBegin(*v.p, v.addr)) { sendRDK("VL ok "); sendRDK(v.name); sendRDK("\n"); }
    else { sendRDK("VL FAIL "); sendRDK(v.name); sendRDK("\n"); }
  }
  sendRDK("BOOT3\n");
}

void setup() {
  sendRDK("BOOT0\n");
  pinMode(LEFT_IN1, OUTPUT); pinMode(LEFT_IN2, OUTPUT);
  pinMode(RIGHT_IN1, OUTPUT); pinMode(RIGHT_IN2, OUTPUT);
  pinMode(ENC_L_A, INPUT_PULLUP); pinMode(ENC_L_B, INPUT_PULLUP);
  pinMode(ENC_R_A, INPUT_PULLUP); pinMode(ENC_R_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_L_A), isrEncL, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_L_B), isrEncL, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_R_A), isrEncR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_R_B), isrEncR, CHANGE);
  pwmAttachAll();

#if USE_UART
  RDK.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
#else
  Serial.begin(115200);
#endif
  sendRDK("BOOT_UART\n");
  carStop();
  delay(200);
  initVL53();
  char buf[64];
  sprintf(buf, "ENC ready c/mm=%.2f\n", COUNTS_PER_MM);
  sendRDK(buf);
}

void loop() {
#if USE_UART
  while (RDK.available()) {
    char c = RDK.read();
    if (c != '\n' && c != '\r') execCmd(c);
  }
#else
  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n' && c != '\r') execCmd(c);
  }
#endif
  delay(5);
}

// [最后修改] 2026-10-25 13:01 —— 升级：双H桥真PWM+kickStart+巡航170+编码器引脚左16/17右25/26（含测距与走廊测试）
