// config.h — 보행동결 자극 시스템 설정값
// 값마다 근거가 있다. 바꾸기 전에 주석을 읽을 것.
#pragma once

// ─── 핀 ───────────────────────────────────────────────
// I2C는 MPU-6050(0x68)과 OLED(0x3C)가 한 버스를 공유한다. 주소가 달라 충돌하지 않는다.
constexpr int PIN_SDA    = 21;
constexpr int PIN_SCL    = 22;
constexpr int PIN_BUZZER = 25;   // 수동 부저. LEDC PWM으로 주파수를 실어 보낸다
constexpr int PIN_BUTTON = 14;   // 택트스위치. INPUT_PULLUP, 누르면 LOW
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
// 검출 지연은 창이 채워지는 시간이 더해져 3~4초다. HARDWARE.md 3절 참조.
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
// 판정식:  FI ≥ FI_THRESHOLD  AND  총파워 ≥ FI_MIN_POWER
// 아래 값은 출발점이다. FI_THRESHOLD와 함께 W9에서 데이터로 확정한다.
constexpr float FI_MIN_POWER = 0.005f;

// ─── 자극 출력 ────────────────────────────────────────
constexpr int   TONE_DOWNBEAT_HZ = 1200;  // 첫 박만 높게. 한 마디의 시작이 귀로 구분된다
constexpr int   TONE_BEAT_HZ     = 800;
constexpr int   TONE_LEN_MS      = 60;
constexpr int   BEATS_PER_BAR    = 4;
constexpr int   FADEIN_MS        = 100;   // 첫 박은 작은 소리에서 시작해 키운다.
                                          // 갑작스러운 소리에 놀라 넘어지지 않게 하려는 것
constexpr int   CUE_MAX_MS       = 30000; // 안전장치. 이 시간을 넘기면 강제 종료한다
constexpr int   CADENCE_MIN = 40;         // 걸음/분. 이 범위를 벗어난 추정치는 버린다
constexpr int   CADENCE_MAX = 140;
constexpr int   CADENCE_FALLBACK = 90;    // 직전 케이던스를 모를 때 쓰는 값

// ─── 로깅 ─────────────────────────────────────────────
constexpr int   MAX_EVENTS = 64;   // 하루치 동결 기록. 넘치면 오래된 것부터 버린다
