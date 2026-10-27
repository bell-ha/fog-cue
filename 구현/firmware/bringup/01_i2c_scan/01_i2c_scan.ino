// 01_i2c_scan — I2C 버스에 뭐가 붙어 있는지 본다
//
// 이 스케치가 통과하기 전에는 다음 단계로 가지 않는다.
// 라이브러리를 하나도 안 쓴다. 안 되면 원인이 배선이거나 보드 설정이지
// 라이브러리 버전 문제일 수가 없다. 그래서 첫 단계로 쓴다.
//
// 기대 출력:
//    0x3C  OLED (SSD1306)
//    0x68  MPU-6050
#include <Wire.h>

constexpr int PIN_SDA = 21;
constexpr int PIN_SCL = 22;

void setup() {
  Serial.begin(115200);
  delay(1000);                      // USB 시리얼이 붙을 시간을 준다
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);            // 처음엔 100 kHz로 느리게. 되면 400k로 올린다
  Serial.println("\n=== I2C 스캔 ===");
}

void loop() {
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {   // 0 = 응답함
      Serial.printf("  0x%02X  ", addr);
      if      (addr == 0x3C || addr == 0x3D) Serial.println("OLED (SSD1306)");
      else if (addr == 0x68 || addr == 0x69) Serial.println("MPU-6050");
      else                                   Serial.println("?");
      found++;
    }
  }

  if (found == 0) {
    Serial.println("  아무것도 없음.");
    Serial.println("  1) 두 모듈 VCC가 3V3에 꽂혔는가 (VIN 아님)");
    Serial.println("  2) GND가 공통인가");
    Serial.println("  3) SDA=21 / SCL=22 가 서로 바뀌지 않았는가");
    Serial.println("  4) 점퍼선을 하나씩 다시 꽂아본다 — 가장 흔한 원인이다");
  } else {
    Serial.printf("  → %d개 찾음\n", found);
  }
  Serial.println();
  delay(3000);
}
