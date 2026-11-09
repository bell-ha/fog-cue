// link.h — 블루투스 또는 Wi-Fi로 웹 화면과 잇는다
//
// USB 시리얼로 내보내는 줄을 무선으로도 똑같이 내보내고, 무선으로 온 명령도 USB와 똑같이 처리한다.
//   블루투스 (link_ble.cpp): 이름 FogCue. Nordic UART 꼴 — 벨트 → 화면은 알림(TX), 화면 → 벨트는 쓰기(RX)
//     벨트와 맥북이 직접 잇는다. 맥북 가까이에서 시연할 때
//   Wi-Fi (link.cpp): 벨트 → 화면 http://<벨트 IP>/events · 화면 → 벨트 http://<벨트 IP>/cmd?c=<명령>
//     공유기를 거친다. 맥북이 멀어도 공유기 가까이면 된다
//
// 네트워크 일은 코어 0의 따로 도는 작업이 맡는다. 화면 쪽이 잠깐 막혀도
// 센서를 읽는 loop(코어 1)는 밀리지 않는다 — 샘플을 놓치면 판정이 틀어진다.
// loop는 버퍼에 글자를 넣기만 한다. 버퍼가 차면 그 글자는 버린다(USB 쪽은 그대로 나간다).
#pragma once
#include <Arduino.h>

// 무선 방식 — 1 블루투스 · 0 Wi-Fi
// 둘을 같이 켜지 않는다. 안테나 하나를 나눠 써서 둘 다 느려지고, Wi-Fi 절전을 끈 설정(link.cpp)과 함께 못 쓴다
// 블루투스는 프로그램이 커서 업로드할 때 파티션을 'Huge APP'으로 둔다 (README 참조)
#define LINK_BLE 0

// Wi-Fi는 wifi_secret.h(WIFI_SSID, WIFI_PASS)가 있을 때만 켠다. git에는 올라가지 않는다
#if LINK_BLE
  #define LINK_WIFI 0
#elif __has_include("../wifi_secret.h")
  #include "../wifi_secret.h"
  #define LINK_WIFI 1
#else
  #define LINK_WIFI 0
#endif

namespace wlink {
  void begin();                                  // 접속은 작업이 알아서 한다 (끊기면 다시 기다린다)
  void write(const uint8_t* data, size_t n);     // 내보낼 글자를 버퍼에 넣는다
  bool readCommand(char* out, size_t n);         // 무선으로 온 명령 하나. 없으면 false
  int  viewers();                                // 붙어 있는 화면 수
}

// USB와 무선에 같이 쓰는 출력. Serial.printf 대신 out.printf를 쓴다
class TeeOut : public Print {
public:
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* b, size_t n) override {
    Serial.write(b, n);
    wlink::write(b, n);
    return n;
  }
};
extern TeeOut out;
