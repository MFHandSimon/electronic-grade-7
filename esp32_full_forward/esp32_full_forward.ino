/*
 * esp32_full_forward.ino —— 纯全速前进（不依赖编码器，验证电机驱动）
 * ====================================================================
 * 功能：RDK 发指令直接控制电机，全速/低速前进后退，不做编码器闭环。
 *       用于验证电机能不能转起来、车能不能走。
 *
 * 指令（RDK → ESP32, 115200）：
 *   W  全速前进     S  全速后退
 *   A  全速左转     D  全速右转
 *   X  停止
 *   ESP32 每收到指令执行后回 'K'
 *
 * 接线：RDK 引脚8→ESP32 GPIO32(RX), 10→GPIO33(TX), GND→GND
 */

#define USE_UART 1
#if USE_UART
HardwareSerial RDK(1);
#define UART_RX 32
#define UART_TX 33
#endif

// 电机引脚
#define LEFT_IN1   2
#define LEFT_IN2   4
#define RIGHT_IN1  13
#define RIGHT_IN2  27

#define PWM_FREQ 20000
#define PWM_RES  8
#define FULL_SPEED 170   // 原255削半后128转不动，提到170(低速但能维持)
#define MID_SPEED  140

// ============ 电机驱动（双 H 桥 DRV8837 真 PWM）============
// 真值表（DRV8837）：
//   IN1=1,IN2=0 → 正转    IN1=0,IN2=1 → 反转    IN1=1,IN2=1 → 刹车
// 所以：前进 = IN1 输出 PWM、IN2=0；后退 = IN2 输出 PWM、IN1=0
// 每个电机 2 个 PWM 通道（IN1、IN2 各一个）
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  static void pwmAttachAll(){
    ledcAttach(LEFT_IN1,  PWM_FREQ, PWM_RES);
    ledcAttach(LEFT_IN2,  PWM_FREQ, PWM_RES);
    ledcAttach(RIGHT_IN1, PWM_FREQ, PWM_RES);
    ledcAttach(RIGHT_IN2, PWM_FREQ, PWM_RES);
  }
  static void pwmWr(int pin, int v){ ledcWrite(pin, v); }
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
  static void pwmWrL1(int v){ ledcWrite(CH_L1, v); }
  static void pwmWrL2(int v){ ledcWrite(CH_L2, v); }
  static void pwmWrR1(int v){ ledcWrite(CH_R1, v); }
  static void pwmWrR2(int v){ ledcWrite(CH_R2, v); }
#endif

// 左电机：fwd=true 前进(IN1 PWM/IN2=0)，fwd=false 后退(IN2 PWM/IN1=0)
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
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
void motorLeft(int speed, bool fwd) {
  if (fwd) { pwmWrL2(0); pwmWrL1(speed); }
  else     { pwmWrL1(0); pwmWrL2(speed); }
}
void motorRight(int speed, bool fwd) {
  if (fwd) { pwmWrR2(0); pwmWrR1(speed); }
  else     { pwmWrR1(0); pwmWrR2(speed); }
}
void carStop() { pwmWrL1(0); pwmWrL2(0); pwmWrR1(0); pwmWrR2(0); }
#endif

// ============ 启动冲刺 ============
// 电机低速启动扭矩不足，先满速冲 60ms 让轮子转起来，再降到巡航速度
void kickStart(int lfwd, int rfwd) {
  motorLeft(255, lfwd);
  motorRight(255, rfwd);
  delay(100);   // 延长到100ms，确保转起来
}

void sendRDK(char c) {
#if USE_UART
  RDK.write(c);
#else
  Serial.write(c);
#endif
}

void execCmd(char c) {
  switch (c) {
    case 'W': motorLeft(255, true); motorRight(255, true); break;   // 满速前进
    case 'S': motorLeft(255, false); motorRight(255, false); break; // 满速后退
    case 'A': motorLeft(255, true); motorRight(255, false); break;  // 满速左转
    case 'D': motorLeft(255, false); motorRight(255, true); break;  // 满速右转
    case 'X': carStop(); break;
    default: return;
  }
  sendRDK('K');
}

void setup() {
  pinMode(LEFT_IN1, OUTPUT); pinMode(LEFT_IN2, OUTPUT);
  pinMode(RIGHT_IN1, OUTPUT); pinMode(RIGHT_IN2, OUTPUT);
  pwmAttachAll();
#if USE_UART
  RDK.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
#else
  Serial.begin(115200);
#endif
  carStop();
  delay(100);
#if USE_UART
  RDK.print("FULL_FWD ready\n");
#else
  Serial.print("FULL_FWD ready\n");
#endif
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

// [最后修改] 2026-10-25 13:20 —— W/S/A/D全部满速255持续，先让车全速冲起来
