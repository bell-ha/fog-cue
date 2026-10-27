// cue.h — 수동 부저로 박자를 낸다
//
// 수동 부저는 능동 부저와 달리 전원만으로는 울지 않는다. 주파수를 실어 보내야 소리가 난다.
// 그래서 첫 박과 나머지 박의 음높이를 다르게 낼 수 있다 — 한 마디의 시작이 귀로 구분된다.
//
// 갑자기 울리면 놀라서 넘어질 수 있다는 지적이 있었다(발표 10번).
// 그래서 첫 박은 작은 소리에서 시작해 키운다.
#pragma once
#include <Arduino.h>
#include "config.h"

class CuePlayer {
public:
  void begin();

  // 템포를 정해 자극을 시작한다. cadence_bpm이 0이면 기본값을 쓴다.
  void start(int cadence_bpm, uint32_t now_ms);
  void stop();

  // 루프마다 호출. 박자 타이밍을 맞춘다.
  void update(uint32_t now_ms);

  bool active() const { return active_; }
  int  tempo()  const { return tempo_bpm_; }

private:
  bool     active_ = false;
  int      tempo_bpm_ = 0;
  uint32_t interval_ms_ = 0;
  uint32_t started_ms_ = 0;
  uint32_t next_beat_ms_ = 0;
  uint32_t tone_off_ms_ = 0;
  int      beat_ = 0;          // 마디 안에서 몇 번째 박인지
  bool     sounding_ = false;

  void beatOn(uint32_t now_ms);
  void beatOff();
};
