/*
 * esp32_enc_diag.ino —— 编码器诊断（只读编码器计数，不动电机）
 * ============================================================
 * 功能：上电后持续打印左右编码器计数。用手拨动轮子，
 *       看计数是否变化，确认编码器信号通不通。
 *
 * 用法：
 *   1) 烧录本固件，打开串口监视器（USB）看输出
 *   2) 用手拨动左轮 → 看 encL 变不变
 *   3) 用手拨动右轮 → 看 encR 变不变
 *
 *   左编码器：GPIO 25、26
 *   右编码器：GPIO 16、17
 *
 * 若 USE_UART=1，输出走 RDK 串口；=0 走 USB 串口监视器。
 * 诊断时建议用 USB（=0），直接看串口监视器最方便。
 */

#define USE_UART 0   // 诊断用 USB 串口监视器最直观，设 0
#if USE_UART
HardwareSerial RDK(1);
#define UART_RX 32
#define UART_TX 33
#endif

#define ENC_L_A 25
#define ENC_L_B 26
#define ENC_R_A 16
#define ENC_R_B 17

volatile long encL = 0;
volatile long encR = 0;

void IRAM_ATTR isrEncL() {
  bool a = digitalRead(ENC_L_A);
  bool b = digitalRead(ENC_L_B);
  if (a != b) encL++; else encL--;
}
void IRAM_ATTR isrEncR() {
  bool a = digitalRead(ENC_R_A);
  bool b = digitalRead(ENC_R_B);
  if (a != b) encR++; else encR--;
}

void out(const char* s) {
#if USE_UART
  RDK.print(s);
#else
  Serial.print(s);
#endif
}

void setup() {
  pinMode(ENC_L_A, INPUT_PULLUP);
  pinMode(ENC_L_B, INPUT_PULLUP);
  pinMode(ENC_R_A, INPUT_PULLUP);
  pinMode(ENC_R_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_L_A), isrEncL, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_L_B), isrEncL, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_R_A), isrEncR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_R_B), isrEncR, CHANGE);

#if USE_UART
  RDK.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
#else
  Serial.begin(115200);
#endif
  out("ENC_DIAG ready. 拨动左轮看encL, 拨动右轮看encR\n");
}

void loop() {
  static unsigned long last = 0;
  if (millis() - last > 500) {   // 每 500ms 打印一次
    last = millis();
    char b[48];
    sprintf(b, "encL=%ld encR=%ld\n", (long)encL, (long)encR);
    out(b);
  }
}

// [最后修改] 2026-10-18 14:05 —— 编码器诊断：只读计数，拨轮看变化
