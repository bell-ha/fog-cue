#include "freeze.h"
#include <math.h>

namespace { constexpr float PI_F = 3.14159265358979f; }

void FreezeDetector::begin() {
  // Hanning 창 — 자르는 경계에서 생기는 가짜 주파수 성분(누설)을 줄인다
  float sum_sq = 0;
  for (int i = 0; i < FFT_N; i++) {
    window_[i] = 0.5f * (1.0f - cosf(2.0f * PI_F * i / (FFT_N - 1)));
    sum_sq += window_[i] * window_[i];
  }
  // 파워를 g²로 되돌리는 계수. 2는 한쪽 스펙트럼만 더하기 때문이다.
  // 이렇게 두면 모든 대역의 파워 합이 원래 신호의 분산과 같아진다.
  win_norm_ = 2.0f / ((float)FFT_N * sum_sq);
  reset();
}

void FreezeDetector::reset() {
  for (int i = 0; i < FFT_N; i++) buf_[i] = 0;
  head_ = filled_ = since_hop_ = 0;
  fi_ = reg_ = total_power_ = 0;
  candidate_ = false;
  hold_ = release_ = 0;
  frozen_ = false;
}

bool FreezeDetector::push(float magnitude) {
  buf_[head_] = magnitude;
  head_ = (head_ + 1) % FFT_N;
  if (filled_ < FFT_N) filled_++;

  if (++since_hop_ < FFT_HOP) return false;
  since_hop_ = 0;
  if (filled_ < FFT_N) return false;   // 창이 다 차기 전에는 판정하지 않는다

  compute();
  return true;
}

// 제자리 radix-2 FFT. FFT_N은 2의 거듭제곱이어야 한다.
void FreezeDetector::fft() {
  const int n = FFT_N;
  // 비트 역순 정렬
  for (int i = 1, j = 0; i < n; i++) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { float t = re_[i]; re_[i] = re_[j]; re_[j] = t;
                 t = im_[i]; im_[i] = im_[j]; im_[j] = t; }
  }
  for (int len = 2; len <= n; len <<= 1) {
    float ang = -2.0f * PI_F / len;
    float wr = cosf(ang), wi = sinf(ang);
    for (int i = 0; i < n; i += len) {
      float cr = 1.0f, ci = 0.0f;
      for (int k = 0; k < len / 2; k++) {
        int a = i + k, b = i + k + len / 2;
        float xr = re_[b] * cr - im_[b] * ci;
        float xi = re_[b] * ci + im_[b] * cr;
        re_[b] = re_[a] - xr;  im_[b] = im_[a] - xi;
        re_[a] += xr;          im_[a] += xi;
        float nr = cr * wr - ci * wi;
        ci = cr * wi + ci * wr;  cr = nr;
      }
    }
  }
}

// 주파수 → 빈 번호.
//
// ★ 이 함수는 analysis/freeze_index.py 의 _band_power 와 **글자 그대로 같은 규칙**이어야 한다.
//   한쪽만 고치면 PC에서 정한 임계값이 기기에서 안 맞고, 그 원인을 찾는 데 며칠이 든다.
//
// 규칙 두 가지:
//  1. bin = round(hz * FFT_N / fs)  — 실행 중에 실제 fs로 계산한다. 상수로 박지 않는다.
//     fs가 62.5든 64든 같은 물리 대역을 본다. (62.5 Hz에서 8 Hz는 빈 32.77 → 33)
//  2. 구간은 [lo, hi) 반열림. 닫힌 구간으로 하면 3 Hz 빈이 두 대역에 모두 들어가
//     분자와 분모에서 같은 값을 두 번 센다.
//
// 반환 단위는 g². 창함수가 깎은 만큼 되돌리기 때문에(win_norm_) 총 파워를
// 절대값 문턱(FI_MIN_POWER)과 직접 비교할 수 있다.
// — FI는 비율이라 창함수가 분자·분모에서 상쇄되지만, 문턱은 상쇄되지 않는다.
float FreezeDetector::bandPower(float lo_hz, float hi_hz) const {
  const float scale = (float)FFT_N / SAMPLE_HZ;
  int lo = (int)lroundf(lo_hz * scale);
  int hi = (int)lroundf(hi_hz * scale);
  if (lo < 1) lo = 1;                   // DC(빈 0)는 어느 대역에도 넣지 않는다
  if (hi > FFT_N / 2) hi = FFT_N / 2;
  float p = 0;
  for (int k = lo; k < hi; k++) p += re_[k] * re_[k] + im_[k] * im_[k];
  return p * win_norm_;
}

// 걸음 규칙성 — seq_(평균을 뺀 시간 순서 신호)의 자기상관을 걸음 간격 범위에서 본다.
//
// ★ analysis/freeze_index.py 의 step_regularity 와 **같은 규칙**이어야 한다:
//   1. 시차 = round(초 * fs) — 실행 중에 실제 fs로 계산한다
//   2. 범위는 [lo, hi) 반열림
//   3. 0번 시차로 나눈다. 겹치는 샘플 수(N-L)로 나누지 않는다 — 시차가 길수록 값이 조금 깎인다
float FreezeDetector::stepRegularity() const {
  int lo = (int)lroundf(REG_LAG_MIN_S * SAMPLE_HZ);
  int hi = (int)lroundf(REG_LAG_MAX_S * SAMPLE_HZ);
  if (hi > FFT_N) hi = FFT_N;
  float r0 = 0;
  for (int i = 0; i < FFT_N; i++) r0 += seq_[i] * seq_[i];
  if (r0 < 1e-12f) return 0;
  float best = -1;
  for (int lag = lo; lag < hi; lag++) {
    float r = 0;
    for (int i = 0; i + lag < FFT_N; i++) r += seq_[i] * seq_[i + lag];
    if (r > best) best = r;
  }
  return best / r0;
}

void FreezeDetector::compute() {
  // 링버퍼를 시간 순서로 펴면서 평균을 뺀다(중력 성분 제거).
  // 규칙성은 그대로, FFT는 창을 곱해서 쓴다
  float mean = 0;
  for (int i = 0; i < FFT_N; i++) mean += buf_[i];
  mean /= FFT_N;

  for (int i = 0; i < FFT_N; i++) {
    int idx = (head_ + i) % FFT_N;
    seq_[i] = buf_[idx] - mean;
    re_[i] = seq_[i] * window_[i];
    im_[i] = 0;
  }
  reg_ = stepRegularity();
  fft();

  float loco   = bandPower(FI_LOCO_LOW,   FI_LOCO_HIGH);
  float freeze = bandPower(FI_FREEZE_LOW, FI_FREEZE_HIGH);
  total_power_ = loco + freeze;
  fi_ = (loco > 1e-9f) ? (freeze / loco) : 0.0f;

  // 완전히 가만히 서 있으면 FI도 규칙성도 의미를 잃는다(둘 다 잡음의 비율이 된다).
  // 그때는 동결이 아니라 그냥 정지다. 2단계(멈추면 울린다)가 맡는다.
  candidate_ = total_power_ >= FI_MIN_POWER &&
               (reg_ < REG_THRESHOLD || fi_ >= FI_THRESHOLD);

  // 한 번 넘었다고 켜지 않고, 한 번 내려갔다고 끄지 않는다.
  // 계단·방향 전환·물건을 집으려 멈추는 동작은 순간적으로 넘지만 이어지지 않는다.
  if (!frozen_) {
    hold_ = candidate_ ? hold_ + 1 : 0;
    if (hold_ >= FI_HOLD_COUNT) { frozen_ = true; release_ = 0; }
  } else {
    release_ = candidate_ ? 0 : release_ + 1;
    if (release_ >= FREEZE_RELEASE_COUNT) { frozen_ = false; hold_ = 0; }
  }
}
