/*
 * esp32_car_fw.ino —— WASD 遥控小车（ESP32 下位机固件）
 * =======================================================
 * 功能：通过串口接收指令，用板载 DRV8837 电机驱动芯片控制
 *       左右两个编码电机，实现小车前进 / 后退 / 原地左转 /
 *       原地右转 / 停止。
 *
 * 指令协议（单字符，波特率 115200）：
 *   W  前进
 *   S  后退
 *   A  原地左转（左轮反转、右轮正转）
 *   D  原地右转（左轮正转、右轮反转）
 *   兼容旧指令：F=前进 B=后退 L=左转 R=右转
 *
 * 使用方式（USB 模式）：
 *   烧录后打开 Arduino 串口监视器（波特率 115200，发送行尾选
 *   "换行符 NL" 或 "两者都"), 输入 W/S/A/D 控制。
 *
 * 引脚（教材 7.2.3，ESP32 主控板）：
 *   左侧电机控制：GPIO 2、4
 *   右侧电机驱动：GPIO 13、27
 *   上位机串口：  RX=32, TX=33
 *
 * 兼容性：同时兼容 Arduino-ESP32 core 2.x（ledcSetup/ledcAttachPin）
 *         和 3.x（ledcAttach）。两种版本都能编译。
 *
 * 烧录：Arduino IDE 选板卡 ESP32 Dev Module，选择 USB 串口。
 */

// ============ 引脚定义 ============
// DRV8837：每个电机 2 个输入脚，一路 PWM 调速，一路方向/使能
#define LEFT_IN1   2    // 左电机 方向/调速
#define LEFT_IN2   4    // 左电机 方向
#define RIGHT_IN1  13   // 右电机 方向/调速
#define RIGHT_IN2  27   // 右电机 方向

// ============ PWM 参数 ============
#define PWM_FREQ   20000 // 20kHz，超出人耳听域
#define PWM_RES    8     // 8 位分辨率（0-255）
#define BASE_SPEED 210   // 前进巡航速度（0-255，已削弱）
#define TURN_SPEED 180   // 原地旋转速度（削弱，稍慢）
#define BACK_SPEED 195   // 后退速度（适中）

// 串口通信模式：
//   0 = USB 模式（用电脑 Arduino 串口监视器控制，WASD）
//   1 = UART 模式（用 RDK 上位机经 GPIO32/33 控制，F/B/L/R/X）
#define USE_UART 1       // 当前：UART 模式（RDK 上位机控制）

#if USE_UART
#define UART_RX 32
#define UART_TX 33
HardwareSerial RDK(1);   // UART1
#endif

// ============ LEDC（PWM）兼容封装 ============
// ESP32 core 3.x 用 ledcAttach(pin,freq,res)，2.x 用 ledcSetup + ledcAttachPin
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  #define LEDC_CH_L LEFT_IN1   // 3.x 直接以引脚作通道
  #define LEDC_CH_R RIGHT_IN1
  static void pwmAttachL(){ ledcAttach(LEFT_IN1,  PWM_FREQ, PWM_RES); }
  static void pwmAttachR(){ ledcAttach(RIGHT_IN1, PWM_FREQ, PWM_RES); }
  static void pwmWriteL(int v){ ledcWrite(LEFT_IN1,  v); }
  static void pwmWriteR(int v){ ledcWrite(RIGHT_IN1, v); }
#else
  #define LEDC_CH_L 0
  #define LEDC_CH_R 1
  static void pwmAttachL(){ ledcSetup(LEDC_CH_L, PWM_FREQ, PWM_RES); ledcAttachPin(LEFT_IN1, LEDC_CH_L); }
  static void pwmAttachR(){ ledcSetup(LEDC_CH_R, PWM_FREQ, PWM_RES); ledcAttachPin(RIGHT_IN1, LEDC_CH_R); }
  static void pwmWriteL(int v){ ledcWrite(LEDC_CH_L, v); }
  static void pwmWriteR(int v){ ledcWrite(LEDC_CH_R, v); }
#endif

// ============ 电机控制 ============
void motorLeft(int speed, bool dir) {
  digitalWrite(LEFT_IN2, dir ? HIGH : LOW);
  pwmWriteL(speed);
}

void motorRight(int speed, bool dir) {
  digitalWrite(RIGHT_IN2, dir ? HIGH : LOW);
  pwmWriteR(speed);
}

void carStop() {
  pwmWriteL(0);
  pwmWriteR(0);
  digitalWrite(LEFT_IN2, LOW);
  digitalWrite(RIGHT_IN2, LOW);
}

// ============ 动作 ============
// 启动冲刺：先以指定速度走 40ms 让电机转起来，再保持同速。
// 速度由调用方传入，保证各动作（前后左右转）手速一致。
void kickStart(int spd, int lDir, int rDir, int durationMs = 40) {
  motorLeft(spd, lDir);   // 设方向 + 目标速度
  motorRight(spd, rDir);
  delay(durationMs);
}

void goForward() {
  kickStart(BASE_SPEED, false, false);
  motorLeft(BASE_SPEED, false);
  motorRight(BASE_SPEED, false);
}

void goBackward() {
  kickStart(BACK_SPEED, true, true);
  motorLeft(BACK_SPEED, true);
  motorRight(BACK_SPEED, true);
}

void turnLeft() {
  // 原地左转：左轮正转，右轮反转
  kickStart(TURN_SPEED, false, true);
  motorLeft(TURN_SPEED, false);
  motorRight(TURN_SPEED, true);
}

void turnRight() {
  // 原地右转：左轮反转，右轮正转
  kickStart(TURN_SPEED, true, false);
  motorLeft(TURN_SPEED, true);
  motorRight(TURN_SPEED, false);
}

void execCmd(char c) {
  // 统一指令集（USB 用 W/S/A/D，UART 用 F/B/L/R/X，互相兼容）
  switch (c) {
    case 'W': case 'F': goForward();  break;  // 前进
    case 'S': case 'B': goBackward(); break;  // 后退
    case 'A': case 'L': turnLeft();   break;  // 左转
    case 'D': case 'R': turnRight();  break;  // 右转
    case 'X': carStop();              break;  // 停止
    default: return;   // 未知指令忽略，不回 K
  }
  // 执行完成后回传 'K'（OK），让上位机知道指令已执行
  #if USE_UART
    RDK.print("K");
  #else
    Serial.print("K");
  #endif
}

// ============ 串口读取 ============
#if USE_UART
void setupSerial() { RDK.begin(115200, SERIAL_8N1, UART_RX, UART_TX); }
void readCommand() {
  while (RDK.available()) {
    char c = RDK.read();
    if (c != '\n' && c != '\r') {
      execCmd(c);
    }
  }
}
void sendInfo() { RDK.println("ESP32 ready"); }
#else
void setupSerial() { Serial.begin(115200); }
void readCommand() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n' && c != '\r') {
      execCmd(c);
    }
  }
}
void sendInfo() { Serial.println("ESP32 ready"); }
#endif

// ============ 初始化 ============
void setup() {
  pinMode(LEFT_IN1, OUTPUT);
  pinMode(LEFT_IN2, OUTPUT);
  pinMode(RIGHT_IN1, OUTPUT);
  pinMode(RIGHT_IN2, OUTPUT);

  pwmAttachL();   // 配置左电机 PWM
  pwmAttachR();   // 配置右电机 PWM

  setupSerial();
  carStop();
  sendInfo();
  delay(500);

  // ---- 开机自检：上电后自动前进 1 秒 ----
  // 用于确认电机和引脚接对。跑起来会看到两个轮子同时向前转 1 秒。
  goForward();
  delay(1000);
  carStop();
  sendInfo();
  #if USE_UART
    RDK.println("[SELFTEST] forward 1s done");
  #else
    Serial.println("[SELFTEST] forward 1s done");
  #endif
}

// ============ 主循环 ============
void loop() {
  readCommand();
  delay(10);
}

// [最后修改] 2026-10-11 14:32 —— 切UART模式(RDK控制)，指令F/B/L/R/X，执行完回K
