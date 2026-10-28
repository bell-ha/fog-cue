// config.h — 보행동결 자극 시스템 설정값
// 값마다 근거가 있다. 바꾸기 전에 주석을 읽을 것.
#pragma once

// ─── 핀 ───────────────────────────────────────────────
// I2C는 MPU-6050(0x68)과 OLED(0x3C)가 한 버스를 공유한다. 주소가 달라 충돌하지 않는다.
constexpr int PIN_SDA    = 21;
constexpr int PIN_SCL    = 22;
constexpr int PIN_BUZZER = 25;   // 수동 부저. LEDC PWM으로 주파수를 실어 보낸다
constexpr int PIN_BUTTON = 14;   // 택트스위치. INPUT_PULLUP, 누르면 LOW
constexpr int PIN_VIBE   = 26;   // 코인형 진동 모터 모듈(3핀)의 IN. 모듈 안의 트랜지스터가 모터를 돌린다
// 심박센서를 붙인다면 ADC1 계열(GPIO32~39)에만 붙일 것.
// ESP32는 WiFi가 켜져 있는 동안 ADC2를 읽을 수 없다.
constexpr int PIN_PULSE  = 34;

// ─── 샘플링 ───────────────────────────────────────────
// 기기는 62.5 Hz, DAPHNet은 64 Hz다. 일부러 다른 게 아니라 **64를 못 만든다.**
//   DLPF를 켜면 MPU-6050 내부 출력이 1 kHz이고 SMPLRT_DIV로 정수 분주한다.
//     DIV=15 → 1000/16 = 62.5 Hz
//     DIV=14 → 1000/15 = 66.7 Hz      ← 그 사이에 64가 없다
//   DLPF를 끄면 base가 8 kHz라 8000/125 = 64를 만들 수 있지만,
//   그러면 21 Hz 저역통과를 잃어 에일리어싱이 생긴다. DLPF를 지키는 쪽이 맞다.
//
// 2.3% 차이는 **대역 경계를 Hz로 두고 빈 번호를 실행 중에 계산하면** 문제가 되지 않는다.
// 빈 번호를 상수로 박는 순간 PC에서 정한 임계값이 기기에서 어긋난다. freeze.cpp 참조.
constexpr float SAMPLE_HZ      = 62.5f;  // 실측으로 확인할 것 (1분 샘플 수 ÷ 60)
constexpr int   MPU_SMPLRT_DIV = 15;     // 레지스터 0x19. 1000/(1+15) = 62.5 Hz
constexpr int   MPU_DLPF_CFG   = 4;      // 레지스터 0x1A. ≈21 Hz 저역통과
constexpr float DAPHNET_HZ     = 64.0f;  // 데이터셋 쪽 값. analysis/ 에서 쓴다

// ─── 걸음 검출 ────────────────────────────────────────
// 보행 성분만 남기는 대역통과. 사람 보행은 0.5~3 Hz 안에 들어온다.
constexpr float STEP_BP_LOW   = 0.5f;
constexpr float STEP_BP_HIGH  = 3.0f;
constexpr int   STEP_REFRACTORY_MS = 250;  // 한 걸음 뒤 이 시간 안의 피크는 같은 걸음으로 본다
constexpr int   CADENCE_WINDOW = 4;        // 최근 4걸음 간격의 '중앙값'을 쓴다.
                                           // 평균이 아닌 이유 — 한 번 튄 값에 템포가 끌려가지 않게
constexpr int   STEP_TIMEOUT_MS = 2000;    // 이만큼 걸음이 없으면 멈춤으로 본다

// ─── 동결 검출 (Freeze Index) ─────────────────────────
// 256샘플 = 62.5 Hz에서 4.096초, 64 Hz에서 정확히 4.000초.
// 4초라는 길이는 Bächlin 2010(DAPHNet 논문)이 고른 값이다.
// — Moore 2008 원 논문의 윈도우 길이는 확인하지 못했다. '원 정의와 같다'고 쓰면 안 된다.
constexpr int   FFT_N        = 256;
constexpr int   FFT_HOP      = 64;    // 75% 겹침 → 1초마다 한 번 판정
constexpr float FI_LOCO_LOW  = 0.5f;  // 보행 대역
constexpr float FI_LOCO_HIGH = 3.0f;
constexpr float FI_FREEZE_LOW  = 3.0f;  // 떨림 대역
constexpr float FI_FREEZE_HIGH = 8.0f;
// 임계값은 PC에서 DAPHNet으로 정한 뒤 여기에 넣는다. 아래는 문헌 범위의 출발점일 뿐이다.
constexpr float FI_THRESHOLD = 1.5f;
// 임계를 넘은 판정이 연속 2회(유지 2초) 이어질 때만 동결로 확정한다.
// 주의 — 이 2초는 '유지 시간'이지 '검출 지연'이 아니다.
// 검출 지연은 창이 채워지는 시간이 더해져 3~4초다 (10/28 기록에서 흉내 시작부터 중앙 3.0초).
//
// 교환비:  유지 +1회  =  지연 + hop 1개(약 1초)
//   hold=1 → 2.8초     hold=2 → 3.8초     hold=3 → 4.8초  (측정 중앙값)
// 오탐이 안 잡힐 때 3으로 올리는 건 흔한 처방이지만 공짜가 아니다.
// 올렸으면 검출 대상으로 삼는 동결의 최소 지속시간도 같이 올려야 한다.
// 계단·방향 전환·물건을 집으려 멈추는 동작은 순간적으로 넘지만 이어지지 않는다.
constexpr int   FI_HOLD_COUNT = 2;
// 동결 판정에는 최소한의 움직임 에너지가 있어야 한다. 단위는 g².
//
// 이건 선택이 아니라 필수다. **가만히 서 있으면 분모(보행 대역)가 작아져 FI가 저절로 치솟는다.**
// FI 하나만으로는 '정상 정지'와 '동결'이 구분되지 않고, 연속 2회 조건으로도 못 막는다
// — 서 있는 내내 FI가 높게 유지되기 때문이다.
// Bächlin 2010이 Moore의 FI에 총 파워 임계를 덧붙인 이유가 이것이다.
//
// 아래 값은 출발점이다. FI_THRESHOLD와 함께 W9에서 데이터로 확정한다.
constexpr float FI_MIN_POWER = 0.005f;

// ─── 걸음 규칙성 ──────────────────────────────────────
// 같은 4초 창에서, 흔들림이 한 걸음 간격으로 되풀이되는 정도. 0~1이고 높을수록 고르게 걷는 중이다.
//   규칙성 = 시차 L ∈ [REG_LAG_MIN_S, REG_LAG_MAX_S) 에서 자기상관(L) 의 최대 ÷ 자기상관(0)
// 시차 범위는 걸음 하나의 간격이다. 0.3초 = 200걸음/분, 1.2초 = 50걸음/분.
// 샘플 수는 실행 중에 SAMPLE_HZ로 계산한다 — FI의 빈 번호를 상수로 박지 않는 것과 같은 이유.
//
// FI만으로 부족했던 이유 (2026-10-28 허리 고정 기록):
//   흉내 7구간 중 3구간은 3~8 Hz가 거의 없어 FI가 1.0도 못 넘었다. 대신 일곱 구간 모두
//   걷는 리듬이 무너져 있었다. 규칙성은 흉내 0.10~0.35, 걷기 0.47~0.69 (기기와 같은 창·hop).
// 0.41은 그 빈틈의 가운데다. 0.35~0.47 어디에 둬도 결과가 같았고 0.50부터 걷기에서 울렸다.
// 한 사람의 흉내 7번·걷기 3번으로 정한 출발값이다. DAPHNet 몸통 채널로 다시 확인할 것.
constexpr float REG_LAG_MIN_S = 0.3f;
constexpr float REG_LAG_MAX_S = 1.2f;
constexpr float REG_THRESHOLD = 0.41f;

// 판정식 (창마다):
//   총파워 ≥ FI_MIN_POWER  AND  ( 규칙성 < REG_THRESHOLD  OR  FI ≥ FI_THRESHOLD )
// 이게 FI_HOLD_COUNT번 연속이면 동결로 켜고, FREEZE_RELEASE_COUNT번 연속 아니면 끈다.
//
// FI를 OR로 남기는 이유 — 4~6 Hz로 고르게 떠는 신호는 규칙성이 0.80~0.87로 높게 나온다.
// 규칙성만 보면 일정한 박자의 떨림을 걷기로 착각한다. 그 경우는 FI가 잡는다.
// 총파워 조건이 없으면 가만히 선 것도 '리듬 없음'으로 걸린다.
//
// 푸는 쪽을 연속 2회로 두는 이유 — 1회로 풀면 떨림이 잠깐 약해질 때마다 소리가 끊겼다 다시 난다
// (FI만으로 풀었을 때 14:02:48 기록의 한 떨기가 두 번으로 쪼개졌다).
// 대가: 떨기를 멈춘 뒤에도 2~3초 더 울린다. 다시 걷는 첫 몇 걸음이 고르지 않으면 더 길다
// (14:02:19 기록, 떨기 사이에 몇 걸음만 걸은 곳에서 6.4초).
constexpr int   FREEZE_RELEASE_COUNT = 2;

// ─── 자극 출력 ────────────────────────────────────────
constexpr int   TONE_DOWNBEAT_HZ = 1200;  // 첫 박만 높게. 한 마디의 시작이 귀로 구분된다
constexpr int   TONE_BEAT_HZ     = 800;
constexpr int   TONE_LEN_MS      = 60;
constexpr int   BEATS_PER_BAR    = 4;
constexpr int   FADEIN_MS        = 100;   // 첫 박은 작은 소리에서 시작해 키운다.
                                          // 갑작스러운 소리에 놀라 넘어지지 않게 하려는 것
constexpr int   CUE_VOLUME_DEFAULT = 20;  // 소리 크기 %(0~100). 처음엔 작게 켠다 — 갑자기 크게 울리면 놀랄 수 있다.
                                          // 웹 화면에서 바꾼다. 기기를 다시 켜면 이 값으로 돌아온다
// 진동 — 드라이버가 든 모듈을 쓴다. 모터만 GPIO에 바로 물리면 전류가 모자라고 보드가 다친다
constexpr bool  VIBE_ACTIVE_HIGH = true;  // IN이 HIGH일 때 떠는 모듈. 가만히 있는데 계속 떨면 false로 바꾼다
constexpr int   VIBE_LEAD_MS     = 500;   // '둘 다'일 때 진동을 먼저 주고 소리를 넣는다 (발표 10번 — 갑자기 울리면 놀랄 수 있다)
constexpr int   VIBE_BEAT_MS     = 200;   // 박자마다 떠는 길이. 90 ms는 거의 못 느꼈고 2초 연속은 잘 느껴졌다(10/28).
                                          // 코인 모터는 돌기 시작하는 데 시간이 걸려, 짧으면 다 돌기 전에 꺼진다
constexpr int   VIBE_DOWNBEAT_MS = 350;   // 첫 박은 길게 — 소리의 높은 첫 박과 같은 역할. 140걸음/분(박 간격 428 ms)에서도 틈이 남는다
constexpr int   CUE_MODE_DEFAULT = 2;     // 자극 방식 0 소리 · 1 진동 · 2 둘 다. 웹 화면에서 바꾼다
constexpr int   CUE_MAX_MS       = 30000; // 안전장치. 이 시간을 넘기면 강제 종료한다
constexpr int   CADENCE_MIN = 40;         // 걸음/분. 이 범위를 벗어난 추정치는 버린다
constexpr int   CADENCE_MAX = 140;
constexpr int   CADENCE_FALLBACK = 90;    // 직전 케이던스를 모를 때 쓰는 값
// 템포는 판정 시점이 아니라 그보다 앞의 케이던스로 정한다.
// 판정은 4초 창이 떨림으로 찬 뒤에야 나오는데, 그동안 떨림이 걸음으로 세어져 케이던스가 부푼다
// (10/28 기록: 떨기 중 95~137, 직전 걷기 85~90). 창 4초 + 두 번째 판정까지 1초 = 5초 전.
constexpr int   CUE_TEMPO_LOOKBACK_S = 5;

// ─── 로깅 ─────────────────────────────────────────────
constexpr int   MAX_EVENTS = 64;   // 하루치 동결 기록. 넘치면 오래된 것부터 버린다
// 샘플마다 원본 한 줄을 시리얼로 낸다:  r,t_ms,ax,ay,az,step,cadence
// 3단계부터는 판정할 때마다(약 1초) 한 줄을 더 낸다:  f,t_ms,fi,규칙성,총파워,후보,동결,소리
// tools/walk_monitor.html 이 이걸 받아 화면에 보여 주고 CSV로 저장한다.
// 한 줄 약 40바이트 × 62.5 Hz ≈ 2.5 KB/s — 115200 baud(약 11.5 KB/s)에 여유가 있다.
// 기기를 단독으로 쓸 때는 false로 둔다.
constexpr bool  LOG_RAW = true;
