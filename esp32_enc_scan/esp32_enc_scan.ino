/*
 * esp32_enc_scan.ino —— 编码器引脚自动扫描（找出真实接编码器的 GPIO）
 * ====================================================================
 * 功能：轮询多个候选 GPIO，检测电平变化（边沿），统计每个引脚的跳变次数。
 *       用手拨动轮子，看哪个引脚在计数 —— 那就是编码器实际接的引脚。
 *
 * 用法：
 *   1) 烧录，打开串口监视器（USB）
 *   2) 上电后看到所有候选引脚计数=0
 *   3) 用手快速拨动左轮 → 观察哪个引脚计数增长
 *   4) 拨动右轮 → 观察哪个引脚计数增长
 *
 * 候选引脚：常见可用的 GPIO（避开了电机2/4/13/27、UART32/33、
 *            I2C21/22、测距使能5/18/19/23）
 *   [0,12,14,15,16,17,25,26,34,35,36,39]
 *   （34-39 是输入专用，也可检测）
 *
 * 输出格式：每 500ms 一行，列出跳变次数>0 的引脚
 */

#define USE_UART 0   // 诊断用 USB 串口监视器

#if USE_UART
HardwareSerial RDK(1);
#define UART_RX 32
#define UART_TX 33
#endif

// 候选引脚
const int PINS[] = {0, 12, 14, 15, 16, 17, 25, 26, 34, 35, 36, 39};
const int N = sizeof(PINS) / sizeof(PINS[0]);

// 上次电平 + 跳变计数
volatile int lastLevel[N];
volatile unsigned long edgeCount[N];

void out(const char* s) {
#if USE_UART
  RDK.print(s);
#else
  Serial.print(s);
#endif
}

void setup() {
#if USE_UART
  RDK.begin(115200, SERIAL_8N1, UART_RX, UART_TX);
#else
  Serial.begin(115200);
#endif
  for (int i = 0; i < N; i++) {
    pinMode(PINS[i], INPUT_PULLUP);
    lastLevel[i] = digitalRead(PINS[i]);
    edgeCount[i] = 0;
  }
  char b[80];
  sprintf(b, "ENC_SCAN 候选引脚: ");
  out(b);
  for (int i = 0; i < N; i++) {
    char t[12];
    sprintf(t, "%d ", PINS[i]);
    out(t);
  }
  out("\n请用手拨动轮子，观察哪些引脚计数增长\n");
}

void loop() {
  // 快速轮询检测边沿
  for (int i = 0; i < N; i++) {
    int lv = digitalRead(PINS[i]);
    if (lv != lastLevel[i]) {
      edgeCount[i]++;
      lastLevel[i] = lv;
    }
  }

  // 每 500ms 打印一次有变化的引脚
  static unsigned long last = 0;
  if (millis() - last > 500) {
    last = millis();
    bool any = false;
    for (int i = 0; i < N; i++) {
      if (edgeCount[i] > 0) {
        char t[40];
        sprintf(t, "  GPIO%d:%lu", PINS[i], edgeCount[i]);
        out(t);
        any = true;
      }
    }
    if (!any) out("  (无变化)");
    out("\n");
  }
}

// [最后修改] 2026-10-18 14:15 —— 编码器引脚自动扫描
