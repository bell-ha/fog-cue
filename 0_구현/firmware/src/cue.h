// cue.h — 수동 부저와 진동 모터로 박자를 낸다
//
// 수동 부저는 능동 부저와 달리 전원만으로는 울지 않는다. 주파수를 실어 보내야 소리가 난다.
// 그래서 첫 박과 나머지 박의 음높이를 다르게 낼 수 있다 — 한 마디의 시작이 귀로 구분된다.
// 진동은 첫 박을 길게, 나머지를 짧게 떤다.
//
// 갑자기 울리면 놀라서 넘어질 수 있다는 지적이 있었다(발표 10번).
// 그래서 '둘 다'일 때는 진동을 먼저 주고 소리를 넣고, 첫 박 소리는 작게 시작해 키운다.
#pragma once
#include <Arduino.h>
#include "../config.h"

enum CueMode { CUE_SOUND = 0, CUE_VIBE = 1, CUE_BOTH = 2 };

class CuePlayer {
public:
  void begin();

  // 템포를 정해 자극을 시작한다. cadence_bpm이 범위 밖이면 기본값을 쓴다.
  // mode를 주면 이번 한 번만 그 방식으로 낸다 (시험 소리·시험 진동). 안 주면 설정된 방식
  void start(int cadence_bpm, uint32_t now_ms, int mode = -1);
  void stop();

  // 루프마다 호출. 박자 타이밍을 맞춘다.
  void update(uint32_t now_ms);

  bool active() const { return active_; }
  int  tempo()  const { return tempo_bpm_; }

  // 소리 크기 0~100%. 최대 duty를 줄여 작게 낸다. 0이면 소리 없이 상태만 돈다
  void setVolume(int percent);
  int  volume() const { return volume_; }

  // 진동만 ms 동안 쭉 낸다 — 배선을 확인하는 시험용. 박자와 상관없다
  void vibrateFor(uint32_t now_ms, uint32_t ms) { vibOn(now_ms, ms); }

  // 자극 방식 — CUE_SOUND · CUE_VIBE · CUE_BOTH
  void setMode(int mode);
  int  mode() const { return mode_; }

private:
  bool     active_ = false;
  int      tempo_bpm_ = 0;
  uint32_t interval_ms_ = 0;
  uint32_t started_ms_ = 0;
  uint32_t first_beat_ms_ = 0;  // 페이드인 기준. '둘 다'에서는 진동 뒤 첫 박이다
  uint32_t next_beat_ms_ = 0;
  uint32_t tone_off_ms_ = 0;
  uint32_t vib_off_ms_ = 0;
  int      beat_ = 0;          // 마디 안에서 몇 번째 박인지
  bool     sounding_ = false;
  bool     vibrating_ = false;
  int      volume_ = CUE_VOLUME_DEFAULT;
  int      mode_ = CUE_MODE_DEFAULT;
  int      run_mode_ = CUE_MODE_DEFAULT;   // 이번 자극에 쓰는 방식

  void beatOn(uint32_t now_ms);
  void beatOff();
  void vibOn(uint32_t now_ms, uint32_t len_ms);
  void vibOff();
};
