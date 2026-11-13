/*
 * esp32_vl53l0x_4ch.ino —— 四个 VL53L0X 激光测距实时输出（ESP32）
 * =============================================================
 * 功能：读取 ESP32 主控板上 4 个 VL53L0X（前/后/左/右）的距离，
 *       通过串口实时打印。
 *
 * 硬件（教材 7.2.5，VL53L0X 插口直接插在主控板上）：
 *   前方 VL53L0X：XSHUT/使能 = GPIO18，分配地址 0x30
 *   后方 VL53L0X：XSHUT/使能 = GPIO23，分配地址 0x31
 *   左侧 VL53L0X：XSHUT/使能 = GPIO5，  分配地址 0x32
 *   右侧 VL53L0X：XSHUT/使能 = GPIO19， 分配地址 0x33
 *   I²C 总线（SDA/SCL）：板载固定（默认 GPIO21=SCL, GPIO22=SDA）
 *
 * 库依赖：Adafruit_VL53L0X（Arduino 库管理器搜索安装）+ Adafruit_BusIO
 *
 * 原理（教材 7.2.5 + Adafruit 官方多传感器示例）：
 *   4 个模块共用一条 I²C 总线，默认地址都是 0x29，必须各不相同。
 *   用 XSHUT 引脚逐个"点名"：
 *     1) 全部 XSHUT 拉低（模块全部关闭）
 *     2) 依次拉高某个模块的 XSHUT 使其上线
 *     3) 立即 begin(新地址) 给该模块分配唯一地址
 *   初始化完成后 4 个模块地址 0x30/0x31/0x32/0x33 互不冲突。
 *
 * 输出格式（波特率 115200）：
 *   F:  前  R:  右  B:  后  L:  左  （单位 mm，超量程显示 8190+）
 */

#include <Wire.h>
#include <Adafruit_VL53L0X.h>

// ============ 引脚定义 ============
// 每个 VL53L0X 的 XSHUT（使能/地址分配）引脚
#define VL_FRONT_XSHUT 18   // 前方
#define VL_BACK_XSHUT  23   // 后方
#define VL_LEFT_XSHUT  5    // 左侧
#define VL_RIGHT_XSHUT 19   // 右侧

// 每个传感器分配的唯一 I²C 地址
#define ADDR_FRONT 0x30
#define ADDR_BACK  0x31
#define ADDR_LEFT  0x32
#define ADDR_RIGHT 0x33

// ============ 传感器对象 ============
Adafruit_VL53L0X sensorFront = Adafruit_VL53L0X();
Adafruit_VL53L0X sensorBack  = Adafruit_VL53L0X();
Adafruit_VL53L0X sensorLeft  = Adafruit_VL53L0X();
Adafruit_VL53L0X sensorRight = Adafruit_VL53L0X();

// 结构体：统一管理 4 个传感器（对照 Adafruit 官方多传感器示例）
typedef struct {
  Adafruit_VL53L0X *psensor;   // 传感器对象指针
  uint8_t addr;                // 分配的 I²C 地址
  int shutdown_pin;            // XSHUT 引脚
  const char* name;            // 位置名
} sensor_t;

sensor_t sensors[] = {
  { &sensorFront, ADDR_FRONT, VL_FRONT_XSHUT, "F(前)" },
  { &sensorBack,  ADDR_BACK,  VL_BACK_XSHUT,  "B(后)" },
  { &sensorLeft,  ADDR_LEFT,  VL_LEFT_XSHUT,  "L(左)" },
  { &sensorRight, ADDR_RIGHT, VL_RIGHT_XSHUT, "R(右)" },
};
const int N_SENSORS = sizeof(sensors) / sizeof(sensors[0]);

// ============ 初始化 ============
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("VL53L0X x4 init...");

  // 1) 全部 XSHUT 拉低 → 所有模块关闭（脱离 I²C 总线）
  for (int i = 0; i < N_SENSORS; i++) {
    pinMode(sensors[i].shutdown_pin, OUTPUT);
    digitalWrite(sensors[i].shutdown_pin, LOW);
  }
  delay(10);

  Wire.begin();   // 启动 I²C

  // 2) 逐个"点名"：拉高 XSHUT 使其上线，然后 begin(新地址) 分配地址
  for (int i = 0; i < N_SENSORS; i++) {
    digitalWrite(sensors[i].shutdown_pin, HIGH);
    delay(20);   // 等待模块上电
    if (sensors[i].psensor->begin(sensors[i].addr, false, &Wire)) {
      Serial.printf("%s OK @0x%02X\n", sensors[i].name, sensors[i].addr);
    } else {
      Serial.printf("%s FAIL 0x%02X\n", sensors[i].name, sensors[i].addr);
    }
  }

  Serial.println("=== 实时测距开始 (F:前 R:右 B:后 L:左, 单位mm) ===");
}

// ============ 主循环 ============
void loop() {
  VL53L0X_RangingMeasurementData_t measure;
  int f = -1, b = -1, l = -1, r = -1;

  sensorFront.rangingTest(&measure, false);
  f = (measure.RangeStatus != 4) ? measure.RangeMilliMeter : -1;

  sensorBack.rangingTest(&measure, false);
  b = (measure.RangeStatus != 4) ? measure.RangeMilliMeter : -1;

  sensorLeft.rangingTest(&measure, false);
  l = (measure.RangeStatus != 4) ? measure.RangeMilliMeter : -1;

  sensorRight.rangingTest(&measure, false);
  r = (measure.RangeStatus != 4) ? measure.RangeMilliMeter : -1;

  Serial.printf("F:%4d  R:%4d  B:%4d  L:%4d  mm\n", f, r, b, l);

  delay(100);   // 100ms 刷新一次（约10Hz）
}

// [最后修改] 2026-10-11 14:00 —— 首次创建：4路VL53L0X串口实时输出（对照Adafruit官方多传感器模式）
