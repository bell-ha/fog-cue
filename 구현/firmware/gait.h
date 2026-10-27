// gait.h — 걸음 검출과 케이던스 추정
// 가속도 크기에서 걸음이 찍히는 순간을 찾고, 직전 걸음 간격으로 템포를 계산한다.
#pragma once
#include <Arduino.h>
#include "config.h"

class GaitTracker {
public:
  void begin();

  // 샘플 하나를 넣는다. 걸음이 검출되면 true.
  bool update(float magnitude, uint32_t now_ms);

  // 걸음/분. 아직 모르면 0.
  int  cadence() const { return cadence_bpm_; }

  // 마지막 걸음 이후 흐른 시간
  uint32_t sinceLastStep(uint32_t now_ms) const {
    return last_step_ms_ ? now_ms - last_step_ms_ : 0;
  }

  // 걸음이 끊겼는가 — 2단계(멈추면 울린다)의 판정 기준
  bool stopped(uint32_t now_ms) const {
    return last_step_ms_ && (now_ms - last_step_ms_) > STEP_TIMEOUT_MS;
  }

  void reset();

private:
  // 0.5~3 Hz 대역통과를 2차 IIR 두 단으로 근사한다.
  // 고역통과로 중력과 체간 흔들림을 걷어내고, 저역통과로 충격 잡음을 깎는다.
  struct Biquad {
    float b0, b1, b2, a1, a2;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    float run(float x) {
      float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
      x2 = x1; x1 = x; y2 = y1; y1 = y;
      return y;
    }
    void clear() { x1 = x2 = y1 = y2 = 0; }
  };
  Biquad hp_, lp_;

  float    prev_ = 0, prev2_ = 0;   // 피크 판정용 직전 두 샘플
  uint32_t last_step_ms_ = 0;
  uint32_t intervals_[CADENCE_WINDOW] = {0};
  int      n_intervals_ = 0;
  int      cadence_bpm_ = 0;

  // 적응형 임계 — 사람마다, 착용 상태마다 가속도 크기가 다르다.
  // 고정 임계를 쓰면 누구에게는 안 잡히고 누구에게는 과하게 잡힌다.
  float    env_ = 0;

  void pushInterval(uint32_t dt_ms);
};
