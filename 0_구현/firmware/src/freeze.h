// freeze.h — 보행 동결을 검출한다
//
// 같은 4초 창에서 두 가지를 본다.
//
//   Freeze Index = 3~8 Hz 대역의 세기 ÷ 0.5~3 Hz 대역의 세기
//     정상 보행은 0.5~3 Hz에 힘이 실린다. 얼어붙으면 다리가 빠르게 떨려 3~8 Hz로 옮겨간다.
//     크기 자체가 아니라 '비율'로 보는 이유는 떨림의 절대 크기가 사람마다, 걷는 속도마다
//     다르기 때문이다. 비율은 그 차이에 덜 흔들린다.
//
//   걸음 규칙성 = 흔들림이 한 걸음 간격으로 되풀이되는 정도 (0~1)
//     걸을 때는 매 걸음이 거의 같은 모양으로 반복된다. 떨림이 3~8 Hz에 실리지 않아도
//     걷는 리듬이 무너지면 이 값이 떨어진다. FI가 놓친 흉내를 이걸로 잡았다 (config.h 참조).
#pragma once
#include <Arduino.h>
#include "../config.h"

class FreezeDetector {
public:
  void begin();

  // 샘플 하나를 버퍼에 넣는다. FFT_HOP마다 한 번 판정이 돌아간다.
  // 판정이 일어난 호출에서만 true를 돌려준다.
  bool push(float magnitude);

  float freezeIndex() const { return fi_; }
  float regularity()  const { return reg_; }
  float lastPower()   const { return total_power_; }
  bool  candidate()   const { return candidate_; }   // 이번 창 하나만 본 판정
  bool  isFrozen()    const { return frozen_; }      // 연속 조건까지 거친 판정

  void reset();

private:
  float    buf_[FFT_N];        // 링버퍼
  int      head_ = 0;
  int      filled_ = 0;
  int      since_hop_ = 0;

  float    fi_ = 0;
  float    reg_ = 0;
  float    total_power_ = 0;
  bool     candidate_ = false;
  int      hold_ = 0;          // 후보가 연속으로 나온 횟수
  int      release_ = 0;       // 동결 중에 후보가 아닌 창이 연속으로 나온 횟수
  bool     frozen_ = false;

  float    window_[FFT_N];     // Hanning. 미리 만들어 둔다
  float    win_norm_ = 1.0f;   // 창함수가 깎은 파워를 되돌리는 계수 (begin에서 계산)
  float    seq_[FFT_N];        // 시간 순서로 편 창, 평균을 뺀 것. 규칙성은 창함수 없이 본다
  float    re_[FFT_N], im_[FFT_N];

  void  compute();
  void  fft();
  float bandPower(float lo_hz, float hi_hz) const;
  float stepRegularity() const;
};
