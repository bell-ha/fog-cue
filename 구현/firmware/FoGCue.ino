// FoGCue.ino — 보행동결 자극 시스템
//
//   걷다가 발이 굳는 순간에만 박자를 들려주는 허리 벨트
//
// 상태 흐름
//   WALKING  걷는 중. 조용하다. 걸음 간격으로 케이던스를 계속 갱신한다
//   STOPPED  걸음이 끊겼다 (2단계 최소 목표는 여기서 바로 울린다)
//   FROZEN   Freeze Index가 임계를 연속으로 넘었다. 동결로 확정하고 자극을 준다
//   다시 걸음이 잡히면 즉시 WALKING으로 돌아가고 소리는 멎는다
//
// 빌드 단계를 STAGE로 고른다. 발표 9번의 4단계와 같다.
//   STAGE 1  걸음만 센다 (시리얼로 확인)
//   STAGE 2  멈추면 울린다              ← 최소 목표
//   STAGE 3  동결만 골라낸다 (Freeze Index)
//   STAGE 4  기록 화면 · 택트스위치 · WiFi
#define STAGE 2

#include <Wire.h>
#include "config.h"
#include "gait.h"
#include "freeze.h"
#include "cue.h"

constexpr uint8_t MPU_ADDR = 0x68;

GaitTracker     gait;
FreezeDetector  freeze;
CuePlayer       cue;

enum State { WALKING, STOPPED, FROZEN };
State state = WALKING;

// 샘플 주기는 MPU가 정한다(아래 loop의 DATA_RDY 참조). 여기서 타이머를 돌리지 않는다.
// SAMPLE_HZ는 '언제 읽을지'가 아니라 '대역 경계를 어디로 둘지' 계산에만 쓰인다.

// ─── MPU-6050 ────────────────────────────────────────
void mpuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg); Wire.write(val);
  Wire.endTransmission();
}

void mpuBegin() {
  // 절전 해제를 빼먹으면 MPU는 자고 있고 값이 전부 0이다. 가장 흔한 첫 실수다.
  mpuWrite(0x6B, 0x00);
  delay(100);
  // 순서 주의 — DLPF를 먼저 켜야 내부 출력이 1 kHz가 되고, 그 다음에야
  // SMPLRT_DIV가 1000/(1+DIV)로 동작한다. 반대로 하면 분주 기준이 8 kHz다.
  mpuWrite(0x1A, MPU_DLPF_CFG);     // DLPF ≈21 Hz. 샘플링 62.5 Hz의 나이퀴스트(31.25) 아래
  mpuWrite(0x19, MPU_SMPLRT_DIV);   // 1000/(1+15) = 62.5 Hz
  mpuWrite(0x1C, 0x00);   // 가속도 ±2 g. 보행 가속도는 대부분 여기 들어온다
  // 자이로는 쓰지 않는다. 동결 판정에 필요 없고 전류만 먹는다
  mpuWrite(0x6C, 0x07);   // 자이로 3축 standby
}

// 가속도 크기. 중력 성분은 뒤에서 각 모듈이 알아서 뺀다
float readMagnitude() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom((int)MPU_ADDR, 6, true);
  if (Wire.available() < 6) return 0;
  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  const float LSB = 16384.0f;   // ±2 g
  float x = ax / LSB, y = ay / LSB, z = az / LSB;
  return sqrtf(x * x + y * y + z * z);
}

// ─── 기록 ────────────────────────────────────────────
struct Event { uint32_t t_ms; int tempo; uint8_t manual; };
Event  events[MAX_EVENTS];
int    n_events = 0;

void logEvent(uint32_t t, int tempo, bool manual) {
  if (n_events >= MAX_EVENTS) {           // 넘치면 오래된 것부터 버린다
    for (int i = 0; i < MAX_EVENTS - 1; i++) events[i] = events[i + 1];
    n_events = MAX_EVENTS - 1;
  }
  events[n_events++] = { t, tempo, (uint8_t)manual };
}

// ─── 상태 전환 ───────────────────────────────────────
void enterCue(uint32_t now, bool manual) {
  cue.start(gait.cadence(), now);
  logEvent(now, cue.tempo(), manual);
  Serial.printf("[CUE ] t=%lus tempo=%d%s\n",
                now / 1000, cue.tempo(), manual ? " (수동)" : "");
}

void leaveCue(uint32_t now) {
  if (cue.active()) {
    cue.stop();
    Serial.printf("[STOP] t=%lus\n", now / 1000);
  }
}

// ─── setup / loop ────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  mpuBegin();
  gait.begin();
  freeze.begin();
  cue.begin();

  Serial.printf("보행동결 자극 시스템 · STAGE %d · %.1f Hz\n", STAGE, SAMPLE_HZ);
  // W2에서 이 값을 실측으로 확인할 것 — 1분간 샘플 수 ÷ 60 이 62.5 ± 1% 여야 한다.
}

// 새 샘플이 준비됐는지 MPU에게 묻는다. INT_STATUS(0x3A) bit0 = DATA_RDY.
bool mpuReady() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3A);
  Wire.endTransmission(false);
  Wire.requestFrom((int)MPU_ADDR, 1, true);
  return Wire.available() && (Wire.read() & 0x01);
}

void loop() {
  // ★ 고정 주기로 읽지 않고 MPU가 '준비됐다'고 할 때만 읽는다.
  //
  // MPU는 자이로를 끄면 내부 RC 발진기로 도는데 오차가 ±1% 쯤 된다.
  // ESP32는 수정 발진기라 훨씬 정확하다. 고정 주기로 읽으면 둘이 서서히 어긋나
  // **같은 샘플을 두 번 읽거나 한 샘플을 건너뛴다.** 1% 차이면 약 1.6초마다 한 번씩이다.
  // FFT는 '일정 간격으로 찍힌 샘플'을 전제하므로 이게 그대로 오차가 된다.
  //
  // DATA_RDY를 보면 샘플 하나당 정확히 한 번 읽는다. 대신 루프가 MPU 속도에 끌려가는데,
  // 그게 맞다 — **대역 경계 계산에 들어가야 할 fs는 ESP32가 정한 값이 아니라 MPU가 내는 값**이다.
  // bringup/03_mpu_rate 로 그 값을 재서 config.h 의 SAMPLE_HZ 에 넣는다.
  if (!mpuReady()) return;
  uint32_t now = millis();

  float mag = readMagnitude();
  bool  step = gait.update(mag, now);

#if STAGE >= 3
  freeze.push(mag);
#endif

#if STAGE == 1
  // ── 1단계: 걸음만 센다 ──
  static uint32_t count = 0;
  if (step) Serial.printf("step %lu  cadence=%d\n", ++count, gait.cadence());

#else
  // ── 2단계 이상: 상태기계 ──
  switch (state) {
    case WALKING:
      if (gait.stopped(now)) {
        state = STOPPED;
  #if STAGE == 2
        // 2단계는 '멈추면' 바로 울린다. 동결인지 아닌지는 아직 안 가린다.
        enterCue(now, false);
  #endif
      }
      break;

    case STOPPED:
  #if STAGE >= 3
      // 3단계는 멈춤 중에서 '동결'만 골라낸다.
      // 그냥 선 것과 얼어붙은 것은 다르다 — 떨림 주파수가 가른다.
      if (freeze.isFrozen()) { state = FROZEN; enterCue(now, false); }
  #endif
      if (step) { leaveCue(now); state = WALKING; }
      break;

    case FROZEN:
      if (step) { leaveCue(now); state = WALKING; }
      break;
  }
  if (state != WALKING && step) { leaveCue(now); state = WALKING; }
#endif

#if STAGE >= 4
  // 검출이 빗나갔을 때 사용자가 직접 요청하는 경로
  static bool prev_btn = HIGH;
  bool btn = digitalRead(PIN_BUTTON);
  if (prev_btn == HIGH && btn == LOW) {
    if (cue.active()) leaveCue(now); else enterCue(now, true);
  }
  prev_btn = btn;
#endif

  cue.update(now);
}
