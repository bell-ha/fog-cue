#include "cue.h"

static constexpr int LEDC_CH  = 0;
static constexpr int LEDC_RES = 10;          // 10비트. duty 0~1023

// 음악 박자 — 한 박에 한 음 (Hz). 긴 음은 박마다 다시 친다
enum { N_C = 1047, N_D = 1175, N_E = 1319, N_F = 1397, N_G = 1568, N_A = 1760 };   // 6옥타브 도~라
static const uint16_t SONG_STAR[] = {        // 작은 별
  N_C, N_C, N_G, N_G, N_A, N_A, N_G, N_G,  N_F, N_F, N_E, N_E, N_D, N_D, N_C, N_C,
  N_G, N_G, N_F, N_F, N_E, N_E, N_D, N_D,  N_G, N_G, N_F, N_F, N_E, N_E, N_D, N_D,
  N_C, N_C, N_G, N_G, N_A, N_A, N_G, N_G,  N_F, N_F, N_E, N_E, N_D, N_D, N_C, N_C };
static const uint16_t SONG_JOY[] = {         // 환희의 송가
  N_E, N_E, N_F, N_G, N_G, N_F, N_E, N_D,  N_C, N_C, N_D, N_E, N_E, N_D, N_D, N_D,
  N_E, N_E, N_F, N_G, N_G, N_F, N_E, N_D,  N_C, N_C, N_D, N_E, N_D, N_C, N_C, N_C };

// 성자의 행진 (When the Saints Go Marching In) — 행진곡이라 걸음 박자가 또렷하다
static const uint16_t SONG_SAINTS[] = {
  N_C, N_E, N_F, N_G, N_G, N_G,
  N_C, N_E, N_F, N_G, N_G, N_G,
  N_C, N_E, N_F, N_G, N_G, N_E, N_E, N_C, N_C, N_E, N_E, N_D, N_D, N_D,
  N_E, N_E, N_D, N_D, N_C, N_C, N_C, N_E, N_E, N_G, N_G, N_F, N_F, N_F,
  N_E, N_F, N_G, N_G, N_E, N_E, N_C, N_C, N_D, N_D, N_C, N_C, N_C };

void CuePlayer::begin() {
  ledcSetup(LEDC_CH, 2000, LEDC_RES);
  ledcAttachPin(PIN_BUZZER, LEDC_CH);
  ledcWrite(LEDC_CH, 0);
  pinMode(PIN_VIBE, OUTPUT);
  vibOff();
}

void CuePlayer::start(int cadence_bpm, uint32_t now_ms, int mode) {
  if (active_) return;
  int bpm = cadence_bpm;
  // 직전 케이던스를 모르거나 사람이 낼 수 없는 값이면 기본값으로 간다.
  // 템포를 '올리려는' 것이 아니라 '맞추려는' 것이므로 임의로 높이지 않는다.
  if (bpm < CADENCE_MIN || bpm > CADENCE_MAX) bpm = CADENCE_FALLBACK;
  tempo_bpm_    = bpm;
  interval_ms_  = 60000UL / bpm;
  run_mode_     = mode < 0 ? mode_ : mode;
  started_ms_   = now_ms;
  next_beat_ms_ = now_ms;
  // '둘 다'는 진동을 먼저 주고 그 뒤에 박자를 시작한다 (발표 10번)
  if (run_mode_ == CUE_BOTH) {
    vibOn(now_ms, VIBE_LEAD_MS);
    next_beat_ms_ = now_ms + VIBE_LEAD_MS;
  }
  first_beat_ms_ = next_beat_ms_;
  beat_         = 0;
  active_       = true;
}

void CuePlayer::setVolume(int percent) {
  volume_ = percent < 0 ? 0 : percent > 100 ? 100 : percent;
}

void CuePlayer::setMode(int mode) {
  mode_ = (mode == CUE_SOUND || mode == CUE_VIBE) ? mode : CUE_BOTH;
}

void CuePlayer::setSong(int song) {
  song_ = (song >= 1 && song <= 3) ? song : 0;
}

void CuePlayer::chirp(uint32_t now_ms, int hz, uint32_t ms) {
  ledcWriteTone(LEDC_CH, hz);
  ledcWrite(LEDC_CH, 512 * volume_ / 100);
  sounding_    = true;
  tone_off_ms_ = now_ms + ms;
}

void CuePlayer::stop() {
  beatOff();
  vibOff();
  active_ = false;
  beat_ = 0;
}

void CuePlayer::beatOn(uint32_t now_ms) {
  bool downbeat = (beat_ % BEATS_PER_BAR) == 0;
  if (run_mode_ != CUE_VIBE) {
    int hz = downbeat ? TONE_DOWNBEAT_HZ : TONE_BEAT_HZ;
    uint32_t len = TONE_LEN_MS;
    if (song_) {
      const uint16_t* notes = song_ == 1 ? SONG_STAR : song_ == 2 ? SONG_JOY : SONG_SAINTS;
      int n = song_ == 1 ? sizeof(SONG_STAR) / sizeof(SONG_STAR[0])
            : song_ == 2 ? sizeof(SONG_JOY) / sizeof(SONG_JOY[0])
            : sizeof(SONG_SAINTS) / sizeof(SONG_SAINTS[0]);
      hz  = notes[beat_ % n];
      len = MUSIC_TONE_MS;
      if (len > interval_ms_ * 7 / 10) len = interval_ms_ * 7 / 10;
    }
    ledcWriteTone(LEDC_CH, hz);
    // 소리 크기는 duty로 정한다. 512(50%)가 가장 크다
    int full = 512 * volume_ / 100;
    // 첫 박 직후 FADEIN_MS 동안은 duty를 올려 가며 작게 낸다
    uint32_t since = now_ms - first_beat_ms_;
    int duty = full;
    if (since < (uint32_t)FADEIN_MS) duty = (int)((uint32_t)full * since / FADEIN_MS) + (full < 40 ? full : 40);
    if (duty > full) duty = full;
    ledcWrite(LEDC_CH, duty);
    sounding_    = true;
    tone_off_ms_ = now_ms + len;
  }
  if (run_mode_ != CUE_SOUND) vibOn(now_ms, downbeat ? VIBE_DOWNBEAT_MS : VIBE_BEAT_MS);
}

void CuePlayer::beatOff() {
  ledcWrite(LEDC_CH, 0);
  sounding_ = false;
}

void CuePlayer::vibOn(uint32_t now_ms, uint32_t len_ms) {
  digitalWrite(PIN_VIBE, VIBE_ACTIVE_HIGH ? HIGH : LOW);
  vibrating_  = true;
  vib_off_ms_ = now_ms + len_ms;
}

void CuePlayer::vibOff() {
  digitalWrite(PIN_VIBE, VIBE_ACTIVE_HIGH ? LOW : HIGH);
  vibrating_ = false;
}

void CuePlayer::update(uint32_t now_ms) {
  if (sounding_ && (int32_t)(now_ms - tone_off_ms_) >= 0) beatOff();
  if (vibrating_ && (int32_t)(now_ms - vib_off_ms_) >= 0) vibOff();
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
