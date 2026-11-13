/*
 * esp32_max_test.ino —— 最简单的全速前进测试
 * 上电后两个电机直接满速正转，不做任何其他事
 * 用来纯粹测试电机硬件
 */

// 电机引脚（左IN1=2 IN2=4，右IN1=13 IN2=27）
#define L_IN1 2
#define L_IN2 4
#define R_IN1 13
#define R_IN2 27

#define PWM_FREQ 20000
#define PWM_RES 8

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void setup() {
  pinMode(L_IN1, OUTPUT); pinMode(L_IN2, OUTPUT);
  pinMode(R_IN1, OUTPUT); pinMode(R_IN2, OUTPUT);
  ledcAttach(L_IN1, PWM_FREQ, PWM_RES);
  ledcAttach(L_IN2, PWM_FREQ, PWM_RES);
  ledcAttach(R_IN1, PWM_FREQ, PWM_RES);
  ledcAttach(R_IN2, PWM_FREQ, PWM_RES);
  // 两个轮子全速正转：IN1=255, IN2=0
  ledcWrite(L_IN2, 0); ledcWrite(L_IN1, 255);
  ledcWrite(R_IN2, 0); ledcWrite(R_IN1, 255);
}
void loop() {}
#else
void setup() {
  pinMode(L_IN1, OUTPUT); pinMode(L_IN2, OUTPUT);
  pinMode(R_IN1, OUTPUT); pinMode(R_IN2, OUTPUT);
  ledcSetup(0, PWM_FREQ, PWM_RES); ledcAttachPin(L_IN1, 0);
  ledcSetup(1, PWM_FREQ, PWM_RES); ledcAttachPin(L_IN2, 1);
  ledcSetup(2, PWM_FREQ, PWM_RES); ledcAttachPin(R_IN1, 2);
  ledcSetup(3, PWM_FREQ, PWM_RES); ledcAttachPin(R_IN2, 3);
  ledcWrite(1, 0); ledcWrite(0, 255);  // 左轮全速
  ledcWrite(3, 0); ledcWrite(2, 255);  // 右轮全速
}
void loop() {}
#endif

// [最后修改] 2026-11-01 13:40 —— 最简单全速测试，上电即两个轮子满速正转
