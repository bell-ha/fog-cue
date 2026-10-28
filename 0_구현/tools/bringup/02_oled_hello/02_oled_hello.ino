// 02_oled_hello — OLED에 글자가 나오는지 본다
//
// 01에서 0x3C를 봤다면 주소는 맞다. 여기서 안 나오면 라이브러리 설정 문제다.
// 라이브러리 매니저에서 "U8g2" (oliver 作) 설치.
//
// ※ 01에서 0x3D가 떴다면 아래 생성자의 마지막 인자를 바꿔야 한다. 주석 참고.
#include <U8g2lib.h>
#include <Wire.h>

constexpr int PIN_SDA = 21;
constexpr int PIN_SCL = 22;

// 0.96" SSD1306 128x64, 하드웨어 I2C, 회전 없음
// 화면이 안 나오는데 0x3C가 보였다면 SSD1306 대신 SH1106 모듈일 수 있다.
//   그 경우: U8G2_SH1106_128X64_NONAME_F_HW_I2C 로 바꾼다
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL);
  // 주소가 0x3D였다면 아래 줄의 주석을 푼다 (U8g2는 8비트 주소라 2배를 쓴다)
  // u8g2.setI2CAddress(0x3D * 2);
  u8g2.begin();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(0, 20, "FoG Cue");
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 38, "OLED OK");
  u8g2.drawStr(0, 52, "W1 passed");
  u8g2.sendBuffer();                 // 이걸 빼먹으면 화면이 안 바뀐다

  Serial.println("OLED에 글자가 보이면 W1 통과.");
}

void loop() {}
