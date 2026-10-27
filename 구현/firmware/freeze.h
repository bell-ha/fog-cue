// freeze.h — Freeze Index로 보행 동결을 검출한다
//
//   Freeze Index = 3~8 Hz 대역의 세기 ÷ 0.5~3 Hz 대역의 세기
//
// 정상 보행은 0.5~3 Hz에 힘이 실린다. 얼어붙으면 다리가 빠르게 떨려 3~8 Hz로 옮겨간다.
// 크기 자체가 아니라 '비율'로 보는 이유는 떨림의 절대 크기가 사람마다, 걷는 속도마다
// 다르기 때문이다. 비율은 그 차이에 덜 흔들린다.
#pragma once
#include <Arduino.h>
#include "config.h"

class FreezeDetector {
public:
  void begin();

  // 샘플 하나를 버퍼에 넣는다. FFT_HOP마다 한 번 판정이 돌아간다.
  // 판정이 일어난 호출에서만 true를 돌려준다.
  bool push(float magnitude);

  float freezeIndex() const { return fi_; }
  bool  isFrozen()    const { return frozen_; }
  float lastPower()   const { return total_power_; }

  void reset();

private:
  float    buf_[FFT_N];        // 링버퍼
  int      head_ = 0;
  int      filled_ = 0;
  int      since_hop_ = 0;

  float    fi_ = 0;
  float    total_power_ = 0;
  int      hold_ = 0;          // 임계를 연속으로 넘은 횟수
  bool     frozen_ = false;

  float    window_[FFT_N];     // Hanning. 미리 만들어 둔다
  float    win_norm_ = 1.0f;   // 창함수가 깎은 파워를 되돌리는 계수 (begin에서 계산)
  float    re_[FFT_N], im_[FFT_N];

  void  compute();
  void  fft();
  float bandPower(float lo_hz, float hi_hz) const;
};
