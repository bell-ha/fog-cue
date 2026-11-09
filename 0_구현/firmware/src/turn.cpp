#include "turn.h"
#include <math.h>

void TurnTracker::begin() {
  for (int i = 0; i < WIN; i++) yaw_buf_[i] = 0;
  head_ = 0; yaw_sum_ = 0; yaw_dps_ = 0;
  turning_ = false; last_turn_ms_ = 0;
  still_n_ = 0; calibrated_ = false;
  for (int k = 0; k < 3; k++) bias_[k] = slow_[k] = 0;
  slow_mag_ = 1;
  grav_[0] = grav_[1] = 0; grav_[2] = 1;
}

void TurnTracker::update(float ax, float ay, float az, float gx, float gy, float gz, uint32_t now) {
  const float a[3] = {ax, ay, az};
  const float g[3] = {gx, gy, gz};

  // 중력 방향 — 가속도를 약 1초 시간상수로 따라간다. 걸음의 출렁임은 평균되어 빠진다
  for (int k = 0; k < 3; k++) grav_[k] += 0.016f * (a[k] - grav_[k]);

  // 자이로 영점. MPU-6050은 가만히 있어도 축마다 수 °/s(최대 ±20)씩 치우친다.
  // 가만히 있을 때(자이로도 가속도 크기도 흔들리지 않을 때)만 영점을 그쪽으로 옮긴다.
  // 가속도 크기를 1 g와 비교하지 않는 이유 — 이 센서는 가만히 있어도 1.12 g로 읽힌다(11/09).
  // 처음엔 빨리 잡고, 잡은 뒤엔 천천히 — 천천히 도는 동작이 영점으로 새지 않게
  bool steady = true;
  for (int k = 0; k < 3; k++) {
    slow_[k] += 0.05f * (g[k] - slow_[k]);
    if (fabsf(g[k] - slow_[k]) > GYRO_STILL_DPS) steady = false;
  }
  float mag = sqrtf(ax * ax + ay * ay + az * az);
  slow_mag_ += 0.05f * (mag - slow_mag_);
  if (steady && fabsf(mag - slow_mag_) < 0.02f) {
    if (!calibrated_) {
      for (int k = 0; k < 3; k++) bias_[k] += 0.05f * (g[k] - bias_[k]);
      if (++still_n_ >= (int)SAMPLE_HZ) {   // 1초 가만히 있었으면 잡힌 것으로 본다
        calibrated_ = true;
        for (int i = 0; i < WIN; i++) yaw_buf_[i] = 0;   // 영점 잡기 전 값(치우침)을 버린다
        yaw_sum_ = 0;
      }
    } else {
      bool near = true;
      for (int k = 0; k < 3; k++) if (fabsf(g[k] - bias_[k]) > GYRO_STILL_DPS * 2) near = false;
      if (near) for (int k = 0; k < 3; k++) bias_[k] += 0.002f * (g[k] - bias_[k]);
    }
  } else if (!calibrated_) {
    still_n_ = 0;
  }

  // 세로축 회전 = 자이로를 중력 방향으로 내린 성분
  float gn = sqrtf(grav_[0] * grav_[0] + grav_[1] * grav_[1] + grav_[2] * grav_[2]);
  float yaw = 0;
  if (gn > 0.5f) {
    for (int k = 0; k < 3; k++) yaw += (g[k] - bias_[k]) * grav_[k];
    yaw /= gn;
  }
  yaw_dps_ = yaw;

  // 최근 1초 동안 돈 각도. 걸을 때 몸통은 좌우로 비틀리지만 한 걸음씩 되돌아와 거의 0이 된다.
  // 돌아설 때는 한쪽으로만 쌓인다
  yaw_sum_ += yaw - yaw_buf_[head_];
  yaw_buf_[head_] = yaw;
  head_ = (head_ + 1) % WIN;
  if (head_ == 0) {   // 더하고 빼기를 오래 하면 float 오차가 쌓인다. 한 바퀴마다 다시 더한다
    yaw_sum_ = 0;
    for (int i = 0; i < WIN; i++) yaw_sum_ += yaw_buf_[i];
  }
  float turned = fabsf(yaw_sum_) / SAMPLE_HZ;   // 도
  // 영점을 잡기 전에는 치우침이 회전처럼 쌓이므로 보지 않는다
  turning_ = calibrated_ && turned >= TURN_DEG;
  if (turning_) last_turn_ms_ = now ? now : 1;
}
