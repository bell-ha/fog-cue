#include "cue.h"

static constexpr int LEDC_CH  = 0;
static constexpr int LEDC_RES = 10;          // 10비트. duty 0~1023

void CuePlayer::begin() {
  ledcSetup(LEDC_CH, 2000, LEDC_RES);
  ledcAttachPin(PIN_BUZZER, LEDC_CH);
  ledcWrite(LEDC_CH, 0);
}

void CuePlayer::start(int cadence_bpm, uint32_t now_ms) {
  if (active_) return;
  int bpm = cadence_bpm;
  // 직전 케이던스를 모르거나 사람이 낼 수 없는 값이면 기본값으로 간다.
  // 템포를 '올리려는' 것이 아니라 '맞추려는' 것이므로 임의로 높이지 않는다.
  if (bpm < CADENCE_MIN || bpm > CADENCE_MAX) bpm = CADENCE_FALLBACK;
  tempo_bpm_    = bpm;
  interval_ms_  = 60000UL / bpm;
  started_ms_   = now_ms;
  next_beat_ms_ = now_ms;
  beat_         = 0;
  active_       = true;
}

void CuePlayer::stop() {
  beatOff();
  active_ = false;
  beat_ = 0;
}

void CuePlayer::beatOn(uint32_t now_ms) {
  bool downbeat = (beat_ % BEATS_PER_BAR) == 0;
  ledcWriteTone(LEDC_CH, downbeat ? TONE_DOWNBEAT_HZ : TONE_BEAT_HZ);
  // 자극이 시작된 직후 FADEIN_MS 동안은 duty를 올려 가며 작게 낸다
  uint32_t since = now_ms - started_ms_;
  int duty = 512;
  if (since < (uint32_t)FADEIN_MS) duty = (int)(512UL * since / FADEIN_MS) + 40;
  ledcWrite(LEDC_CH, duty);
  sounding_    = true;
  tone_off_ms_ = now_ms + TONE_LEN_MS;
}

void CuePlayer::beatOff() {
  ledcWrite(LEDC_CH, 0);
  sounding_ = false;
}

void CuePlayer::update(uint32_t now_ms) {
  if (sounding_ && (int32_t)(now_ms - tone_off_ms_) >= 0) beatOff();
  if (!active_) return;

  // 안전장치 — 어떤 이유로든 상태가 풀리지 않으면 강제로 멈춘다.
  // 계속 울리는 기기가 되지 않게 하는 것이 이 프로젝트의 전부다.
  if (now_ms - started_ms_ > (uint32_t)CUE_MAX_MS) { stop(); return; }

  if ((int32_t)(now_ms - next_beat_ms_) >= 0) {
    beatOn(now_ms);
    beat_++;
    next_beat_ms_ += interval_ms_;
  }
}
