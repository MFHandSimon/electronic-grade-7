/*
 * esp32_uart_link.ino —— RDK↔ESP32 串口链路最小验证固件
 * ======================================================
 * 功能：纯验证 RDK X3 与 ESP32 之间 UART 链路是否打通。
 *       ESP32 收到任意字节，都原样回显，并打印到 USB 串口监视器。
 *
 * 用法：
 *   1) 烧录本固件（USB 模式，串口监视器看启动日志）
 *   2) 烧完后把 ESP32 的 GPIO32/33 用杜邦线交叉接到 RDK 40PIN
 *   3) 在 RDK 上用 rdk_link_test.py 发 'T'，能收到 'T' 就说明链路通
 *
 * 接线（RDK 40PIN → ESP32，必须交叉）：
 *   RDK TXD → ESP32 RX (GPIO32)
 *   RDK RXD ← ESP32 TX (GPIO33)
 *   RDK GND → ESP32 GND
 *
 * 波特率：115200
 * 注意：GPIO32/33 在部分板上是 RTC 脚，此固件已正确配置为 UART1。
 */

#define USE_UART 1     // 本固件固定走 GPIO 串口（RDK 用）

#if USE_UART
HardwareSerial RDK(1);   // UART1
#define UART_RX 32
#define UART_TX 33
void setupSerial() {
  RDK.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
  Serial.begin(115200);
}
void sendRDK(const char* s){ RDK.print(s); }
#else
void setupSerial() {
  Serial.begin(115200);
}
void sendRDK(const char* s){ Serial.print(s); }
#endif

void setup() {
  setupSerial();
  delay(300);
  Serial.println("=== UART LINK TEST (ESP32 side) ===");
  sendRDK("ESP32_ONLINE\n");   // 上电主动上报，RDK 端应能读到
  Serial.println("Sent: ESP32_ONLINE");
}

void loop() {
  // ESP32 从 RDK 收到字节 → 原样回显 + USB 打印
  while (RDK.available()) {
    char c = RDK.read();
    RDK.write(c);            // 回显给 RDK
    Serial.write(c);         // 同时打印到 USB 监视器
  }
}

// [最后修改] 2026-10-11 14:10 —— 首次创建：RDK↔ESP32 UART链路最小回显验证
