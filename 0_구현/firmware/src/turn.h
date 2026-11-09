// turn.h — 몸이 돌고 있는지 본다 (자이로)
//
// 그냥 돌아설 때도 걸음이 고르지 않아 '걸음 리듬이 무너짐'으로 판정이 켜질 수 있다.
// 그렇다고 돌 때 판정을 끄면 안 된다 — 돌아서다 굳는 일이 가장 흔하다.
//
// 그래서 이렇게 가른다.
//   몸이 계속 돌아가고 있으면 → 아직 도는 중이다. 판정을 미룬다
//   돌던 몸이 멈췄는데 발이 떨리면 → 굳은 것이다. 미룬 판정을 다시 한다
// 돌다 굳으면 발은 떨어도 몸통 회전은 거의 멈춘다. 그 차이를 본다.
//
// 회전 속도는 '세로축(중력 방향)으로 도는 빠르기'만 쓴다. 벨트를 어떻게 차도
// 중력 방향은 가속도에서 알 수 있어, 센서 방향을 정해 두지 않아도 된다.
#pragma once
#include <Arduino.h>
#include "../config.h"

class TurnTracker {
public:
  void begin();

  // 샘플 하나. 가속도 g, 자이로 °/s (영점 보정 전 값)
  void update(float ax, float ay, float az, float gx, float gy, float gz, uint32_t now_ms);

  float yawRate()  const { return yaw_dps_; }    // 세로축 회전 빠르기 °/s (부호는 차는 방향에 따라 다르다)
  bool  turning()  const { return turning_; }    // 최근 1초에 TURN_DEG 넘게 돌았다
  // 판정을 미룰지 — 도는 중이거나, 돌기를 멈춘 지 TURN_SETTLE_MS가 안 됐다
  bool  holdOff(uint32_t now_ms) const {
    return last_turn_ms_ && now_ms - last_turn_ms_ < TURN_SETTLE_MS;
  }

private:
  float    grav_[3] = {0, 0, 1};   // 중력 방향 (가속도를 느리게 따라간다)
  float    bias_[3] = {0, 0, 0};   // 자이로 영점
  float    slow_[3] = {0, 0, 0};   // 자이로를 느리게 따라간 값 — 가만히 있는지 볼 때 쓴다
  float    slow_mag_ = 1;          // 가속도 크기를 느리게 따라간 값
  int      still_n_ = 0;           // 가만히 있던 샘플 수 (영점을 처음 잡을 때까지 센다)
  bool     calibrated_ = false;

  static constexpr int WIN = (int)(SAMPLE_HZ + 0.5f);   // 1초
  float    yaw_buf_[WIN] = {0};
  int      head_ = 0;
  float    yaw_sum_ = 0;
  float    yaw_dps_ = 0;
  bool     turning_ = false;
  uint32_t last_turn_ms_ = 0;
};
