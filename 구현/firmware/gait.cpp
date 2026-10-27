#include "gait.h"
#include <math.h>

// 2차 버터워스 계수를 쌍선형 변환으로 만든다.
// fc를 tan(pi*fc/fs)로 미리 '휘어' 두면, 디지털로 바꿨을 때 차단주파수가 제자리에 온다.
namespace {
  constexpr float PI_F = 3.14159265f;

  // 고역통과 2차 버터워스
  void hp(float fc, float fs, float& b0, float& b1, float& b2, float& a1, float& a2) {
    float w = tanf(PI_F * fc / fs);
    float k = 1.0f / (1.0f + 1.41421356f * w + w * w);
    b0 =  k;  b1 = -2.0f * k;  b2 =  k;
    a1 =  2.0f * (w * w - 1.0f) * k;
    a2 =  (1.0f - 1.41421356f * w + w * w) * k;
  }
  // 저역통과 2차 버터워스
  void lp(float fc, float fs, float& b0, float& b1, float& b2, float& a1, float& a2) {
    float w = tanf(PI_F * fc / fs);
    float k = 1.0f / (1.0f + 1.41421356f * w + w * w);
    b0 = w * w * k;  b1 = 2.0f * b0;  b2 = b0;
    a1 =  2.0f * (w * w - 1.0f) * k;
    a2 =  (1.0f - 1.41421356f * w + w * w) * k;
  }
}

void GaitTracker::begin() {
  hp(STEP_BP_LOW,  SAMPLE_HZ, hp_.b0, hp_.b1, hp_.b2, hp_.a1, hp_.a2);
  lp(STEP_BP_HIGH, SAMPLE_HZ, lp_.b0, lp_.b1, lp_.b2, lp_.a1, lp_.a2);
  reset();
}

void GaitTracker::reset() {
  hp_.clear(); lp_.clear();
  prev_ = prev2_ = 0;
  last_step_ms_ = 0;
  n_intervals_ = 0;
  cadence_bpm_ = 0;
  env_ = 0;
}

void GaitTracker::pushInterval(uint32_t dt_ms) {
  // 최근 CADENCE_WINDOW개를 굴린다
  if (n_intervals_ < CADENCE_WINDOW) {
    intervals_[n_intervals_++] = dt_ms;
  } else {
    for (int i = 0; i < CADENCE_WINDOW - 1; i++) intervals_[i] = intervals_[i + 1];
    intervals_[CADENCE_WINDOW - 1] = dt_ms;
  }
  if (n_intervals_ < 2) return;

  // 중앙값 — 한 번 튄 간격에 템포가 끌려가지 않게
  uint32_t s[CADENCE_WINDOW];
  for (int i = 0; i < n_intervals_; i++) s[i] = intervals_[i];
  for (int i = 1; i < n_intervals_; i++)
    for (int j = i; j > 0 && s[j] < s[j - 1]; j--) { uint32_t t = s[j]; s[j] = s[j - 1]; s[j - 1] = t; }
  uint32_t med = (n_intervals_ % 2) ? s[n_intervals_ / 2]
                                    : (s[n_intervals_ / 2 - 1] + s[n_intervals_ / 2]) / 2;
  if (med == 0) return;

  int bpm = (int)(60000UL / med);
  // 사람이 낼 수 없는 값은 버린다. 노이즈 피크가 걸음으로 잡힌 경우다.
  if (bpm >= CADENCE_MIN && bpm <= CADENCE_MAX) cadence_bpm_ = bpm;
}

bool GaitTracker::update(float magnitude, uint32_t now_ms) {
  float x = lp_.run(hp_.run(magnitude));   // 0.5~3 Hz만 남긴다

  // 신호 크기를 천천히 따라가는 포락선. 임계를 여기에 비례시킨다.
  float a = fabsf(x);
  env_ += (a > env_ ? 0.05f : 0.005f) * (a - env_);   // 빠르게 올라가고 천천히 내려온다
  float thr = env_ * 0.6f;
  if (thr < 0.02f) thr = 0.02f;   // 정지 상태에서 노이즈가 걸음으로 잡히지 않게

  bool step = false;
  // 국소 최대 + 임계 초과 + 불응 구간 경과
  if (prev_ > thr && prev_ >= prev2_ && prev_ >= x) {
    uint32_t dt = now_ms - last_step_ms_;
    if (last_step_ms_ == 0 || dt >= (uint32_t)STEP_REFRACTORY_MS) {
      if (last_step_ms_ != 0) pushInterval(dt);
      last_step_ms_ = now_ms;
      step = true;
    }
  }
  prev2_ = prev_; prev_ = x;
  return step;
}
