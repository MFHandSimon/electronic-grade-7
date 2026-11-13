/*
 * esp32_test_forward.ino —— 极简验证：上电后小车持续前进
 * ======================================================
 * 目的：不依赖串口、不接收任何指令，ESP32 上电后两个电机
 *       直接以固定 PWM 持续前进。
 *
 * 用途：验证「电机 + DRV8837 驱动 + 引脚 + 供电」整条链路。
 *   - 烧录后上电，两个轮子应该同时转动（前进）
 *   - 若这个能动 → 电机/驱动/引脚/供电全对，问题在串口
 *   - 若不动     → 电机侧有问题，先查供电/接线
 *
 * 引脚（教材 7.2.3）：
 *   左电机：IN1=GPIO2(调速)  IN2=GPIO4(方向)
 *   右电机：IN1=GPIO13(调速) IN2=GPIO27(方向)
 *
 * 兼容 ESP32 core 2.x 与 3.x 的 LEDC API。
 */

#define LEFT_IN1   2
#define LEFT_IN2   4
#define RIGHT_IN1  13
#define RIGHT_IN2  27

#define PWM_FREQ   20000
#define PWM_RES    8
#define SPEED      220     // 前进 PWM（0-255）

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  static void pwmAttachL(){ ledcAttach(LEFT_IN1,  PWM_FREQ, PWM_RES); }
  static void pwmAttachR(){ ledcAttach(RIGHT_IN1, PWM_FREQ, PWM_RES); }
  static void pwmWriteL(int v){ ledcWrite(LEFT_IN1,  v); }
  static void pwmWriteR(int v){ ledcWrite(RIGHT_IN1, v); }
#else
  #define CH_L 0
  #define CH_R 1
  static void pwmAttachL(){ ledcSetup(CH_L, PWM_FREQ, PWM_RES); ledcAttachPin(LEFT_IN1, CH_L); }
  static void pwmAttachR(){ ledcSetup(CH_R, PWM_FREQ, PWM_RES); ledcAttachPin(RIGHT_IN1, CH_R); }
  static void pwmWriteL(int v){ ledcWrite(CH_L, v); }
  static void pwmWriteR(int v){ ledcWrite(CH_R, v); }
#endif

void setup() {
  pinMode(LEFT_IN1,  OUTPUT);
  pinMode(LEFT_IN2,  OUTPUT);
  pinMode(RIGHT_IN1, OUTPUT);
  pinMode(RIGHT_IN2, OUTPUT);

  pwmAttachL();
  pwmAttachR();

  // 设方向：前进（IN2 为方向脚，HIGH=正转）
  digitalWrite(LEFT_IN2, HIGH);
  digitalWrite(RIGHT_IN2, HIGH);

  // 启动冲刺 200ms（满速突破静摩擦）
  pwmWriteL(255);
  pwmWriteR(255);
  delay(200);

  // 持续前进
  pwmWriteL(SPEED);
  pwmWriteR(SPEED);
}

void loop() {
  // 什么都不做，一直前进
  delay(1000);
}

// [最后修改] 2026-10-11 13:14 —— 新增时间戳标注，便于核对烧录版本
