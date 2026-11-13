/*
 * esp32_max_test.ino —— 慢衰减电机 + 4路VL53L0X测距
 * 指令（RDK → ESP32, 115200）：
 *   W 前进(半速)  S 后退(半速)  X 停
 *   T 读四个方向距离并输出  F:xxx R:xxx B:xxx L:xxx
 */

#include <Wire.h>
#include <Adafruit_VL53L0X.h>

#define USE_UART 1
#if USE_UART
HardwareSerial RDK(1);
#define UART_RX 32
#define UART_TX 33
#endif

// 电机引脚
#define L_IN1 2
#define L_IN2 4
#define R_IN1 13
#define R_IN2 27

#define PWM_FREQ 20000
#define PWM_RES 8
#define CRUISE_SPEED 128

// VL53L0X XSHUT 引脚
#define VL_FRONT_XSHUT 18
#define VL_BACK_XSHUT  23
#define VL_LEFT_XSHUT  5
#define VL_RIGHT_XSHUT 19

Adafruit_VL53L0X sensorFront = Adafruit_VL53L0X();
Adafruit_VL53L0X sensorBack  = Adafruit_VL53L0X();
Adafruit_VL53L0X sensorLeft  = Adafruit_VL53L0X();
Adafruit_VL53L0X sensorRight = Adafruit_VL53L0X();

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void setup() {
#if USE_UART
  RDK.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
#endif
  pinMode(L_IN1, OUTPUT); pinMode(L_IN2, OUTPUT);
  pinMode(R_IN1, OUTPUT); pinMode(R_IN2, OUTPUT);
  ledcAttach(L_IN1, PWM_FREQ, PWM_RES);
  ledcAttach(L_IN2, PWM_FREQ, PWM_RES);
  ledcAttach(R_IN1, PWM_FREQ, PWM_RES);
  ledcAttach(R_IN2, PWM_FREQ, PWM_RES);
  ledcWrite(L_IN1, 0); ledcWrite(L_IN2, 0);
  ledcWrite(R_IN1, 0); ledcWrite(R_IN2, 0);

  // VL53L0X 初始化
  pinMode(VL_FRONT_XSHUT, OUTPUT); pinMode(VL_BACK_XSHUT, OUTPUT);
  pinMode(VL_LEFT_XSHUT, OUTPUT);  pinMode(VL_RIGHT_XSHUT, OUTPUT);
  digitalWrite(VL_FRONT_XSHUT, LOW); digitalWrite(VL_BACK_XSHUT, LOW);
  digitalWrite(VL_LEFT_XSHUT, LOW);  digitalWrite(VL_RIGHT_XSHUT, LOW);
  delay(10);
  Wire.begin();
  digitalWrite(VL_FRONT_XSHUT, HIGH); delay(20);
  if(sensorFront.begin(0x30, false, &Wire)) RDK.println("VL F OK"); else RDK.println("VL F FAIL");
  digitalWrite(VL_BACK_XSHUT, HIGH); delay(20);
  if(sensorBack.begin(0x31, false, &Wire)) RDK.println("VL B OK"); else RDK.println("VL B FAIL");
  digitalWrite(VL_LEFT_XSHUT, HIGH); delay(20);
  if(sensorLeft.begin(0x32, false, &Wire)) RDK.println("VL L OK"); else RDK.println("VL L FAIL");
  digitalWrite(VL_RIGHT_XSHUT, HIGH); delay(20);
  if(sensorRight.begin(0x33, false, &Wire)) RDK.println("VL R OK"); else RDK.println("VL R FAIL");

#if USE_UART
  RDK.println("READY");
#endif
}

void loop() {
#if USE_UART
  while (RDK.available()) {
    char c = RDK.read();
    if (c == 'W') {
      // 慢衰减正转
      ledcWrite(L_IN1, 255); ledcWrite(L_IN2, 255 - CRUISE_SPEED);
      ledcWrite(R_IN1, 255); ledcWrite(R_IN2, 255 - CRUISE_SPEED);
      RDK.write('K');
    } else if (c == 'S') {
      // 慢衰减反转
      ledcWrite(L_IN2, 255); ledcWrite(L_IN1, 255 - CRUISE_SPEED);
      ledcWrite(R_IN2, 255); ledcWrite(R_IN1, 255 - CRUISE_SPEED);
      RDK.write('K');
    } else if (c == 'X') {
      ledcWrite(L_IN1, 0); ledcWrite(L_IN2, 0);
      ledcWrite(R_IN1, 0); ledcWrite(R_IN2, 0);
      RDK.write('K');
    } else if (c == 'T') {
      // 读四个方向距离
      VL53L0X_RangingMeasurementData_t measure;
      sensorFront.rangingTest(&measure, false);
      int f = (measure.RangeStatus != 4) ? measure.RangeMilliMeter : -1;
      sensorBack.rangingTest(&measure, false);
      int b = (measure.RangeStatus != 4) ? measure.RangeMilliMeter : -1;
      sensorLeft.rangingTest(&measure, false);
      int l = (measure.RangeStatus != 4) ? measure.RangeMilliMeter : -1;
      sensorRight.rangingTest(&measure, false);
      int r = (measure.RangeStatus != 4) ? measure.RangeMilliMeter : -1;
      RDK.printf("F:%d R:%d B:%d L:%d\n", f, r, b, l);
    }
  }
#endif
  delay(5);
}
#else
#define CH_L1 0
#define CH_L2 1
#define CH_R1 2
#define CH_R2 3
void setup() {
#if USE_UART
  RDK.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
#endif
  pinMode(L_IN1, OUTPUT); pinMode(L_IN2, OUTPUT);
  pinMode(R_IN1, OUTPUT); pinMode(R_IN2, OUTPUT);
  ledcSetup(CH_L1, PWM_FREQ, PWM_RES); ledcAttachPin(L_IN1, CH_L1);
  ledcSetup(CH_L2, PWM_FREQ, PWM_RES); ledcAttachPin(L_IN2, CH_L2);
  ledcSetup(CH_R1, PWM_FREQ, PWM_RES); ledcAttachPin(R_IN1, CH_R1);
  ledcSetup(CH_R2, PWM_FREQ, PWM_RES); ledcAttachPin(R_IN2, CH_R2);
  ledcWrite(CH_L1, 0); ledcWrite(CH_L2, 0);
  ledcWrite(CH_R1, 0); ledcWrite(CH_R2, 0);

  pinMode(VL_FRONT_XSHUT, OUTPUT); pinMode(VL_BACK_XSHUT, OUTPUT);
  pinMode(VL_LEFT_XSHUT, OUTPUT);  pinMode(VL_RIGHT_XSHUT, OUTPUT);
  digitalWrite(VL_FRONT_XSHUT, LOW); digitalWrite(VL_BACK_XSHUT, LOW);
  digitalWrite(VL_LEFT_XSHUT, LOW);  digitalWrite(VL_RIGHT_XSHUT, LOW);
  delay(10);
  Wire.begin();
  digitalWrite(VL_FRONT_XSHUT, HIGH); delay(20);
  if(sensorFront.begin(0x30, false, &Wire)) RDK.println("VL F OK"); else RDK.println("VL F FAIL");
  digitalWrite(VL_BACK_XSHUT, HIGH); delay(20);
  if(sensorBack.begin(0x31, false, &Wire)) RDK.println("VL B OK"); else RDK.println("VL B FAIL");
  digitalWrite(VL_LEFT_XSHUT, HIGH); delay(20);
  if(sensorLeft.begin(0x32, false, &Wire)) RDK.println("VL L OK"); else RDK.println("VL L FAIL");
  digitalWrite(VL_RIGHT_XSHUT, HIGH); delay(20);
  if(sensorRight.begin(0x33, false, &Wire)) RDK.println("VL R OK"); else RDK.println("VL R FAIL");

#if USE_UART
  RDK.println("READY");
#endif
}
void loop() {
#if USE_UART
  while (RDK.available()) {
    char c = RDK.read();
    if (c == 'W') {
      ledcWrite(CH_L1, 255); ledcWrite(CH_L2, 255 - CRUISE_SPEED);
      ledcWrite(CH_R1, 255); ledcWrite(CH_R2, 255 - CRUISE_SPEED);
      RDK.write('K');
    } else if (c == 'S') {
      ledcWrite(CH_L2, 255); ledcWrite(CH_L1, 255 - CRUISE_SPEED);
      ledcWrite(CH_R2, 255); ledcWrite(CH_R1, 255 - CRUISE_SPEED);
      RDK.write('K');
    } else if (c == 'X') {
      ledcWrite(CH_L1, 0); ledcWrite(CH_L2, 0);
      ledcWrite(CH_R1, 0); ledcWrite(CH_R2, 0);
      RDK.write('K');
    } else if (c == 'T') {
      VL53L0X_RangingMeasurementData_t measure;
      sensorFront.rangingTest(&measure, false);
      int f = (measure.RangeStatus != 4) ? measure.RangeMilliMeter : -1;
      sensorBack.rangingTest(&measure, false);
      int b = (measure.RangeStatus != 4) ? measure.RangeMilliMeter : -1;
      sensorLeft.rangingTest(&measure, false);
      int l = (measure.RangeStatus != 4) ? measure.RangeMilliMeter : -1;
      sensorRight.rangingTest(&measure, false);
      int r = (measure.RangeStatus != 4) ? measure.RangeMilliMeter : -1;
      RDK.printf("F:%d R:%d B:%d L:%d\n", f, r, b, l);
    }
  }
#endif
  delay(5);
}
#endif

// [最后修改] 2026-11-01 14:35 —— RDK.begin()移到最前面，修复传感器状态输出
