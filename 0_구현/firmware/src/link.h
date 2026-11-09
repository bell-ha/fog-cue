// link.h — Wi-Fi로 웹 화면과 잇는다
//
// USB 시리얼로 내보내는 줄을 Wi-Fi로도 똑같이 내보내고, Wi-Fi로 온 명령도 USB와 똑같이 처리한다.
//   벨트 → 화면: http://<벨트 IP>/events   (이벤트 스트림, 한 줄 = 한 이벤트)
//   화면 → 벨트: http://<벨트 IP>/cmd?c=<명령>
//
// 네트워크 일은 코어 0의 따로 도는 작업이 맡는다. 화면 쪽이 잠깐 막혀도
// 센서를 읽는 loop(코어 1)는 밀리지 않는다 — 샘플을 놓치면 판정이 틀어진다.
// loop는 버퍼에 글자를 넣기만 한다. 버퍼가 차면 그 글자는 버린다(USB 쪽은 그대로 나간다).
#pragma once
#include <Arduino.h>

// wifi_secret.h(WIFI_SSID, WIFI_PASS)가 있을 때만 Wi-Fi를 켠다. git에는 올라가지 않는다
#if __has_include("../wifi_secret.h")
  #include "../wifi_secret.h"
  #define LINK_WIFI 1
#else
  #define LINK_WIFI 0
#endif

namespace wlink {
  void begin();                                  // Wi-Fi 접속은 작업이 알아서 한다 (못 붙으면 다시 시도)
  void write(const uint8_t* data, size_t n);     // 내보낼 글자를 버퍼에 넣는다
  bool readCommand(char* out, size_t n);         // Wi-Fi로 온 명령 하나. 없으면 false
  int  viewers();                                // 붙어 있는 화면 수
}

// USB와 Wi-Fi에 같이 쓰는 출력. Serial.printf 대신 out.printf를 쓴다
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
