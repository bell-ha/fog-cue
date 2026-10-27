// 03_mpu_rate — MPU가 실제로 몇 Hz를 내는지 재고, 정지 상태 magnitude를 본다
//
// 커리큘럼 W2의 통과 기준 두 가지를 한 번에 확인한다.
//   · 샘플레이트 오차 ±1% 이내
//   · 정지 상태에서 magnitude = 1.00 g ± 0.05
//
// ★ 중요 — '루프를 몇 번 돌았나'를 세면 안 된다.
//   그건 ESP32가 스스로 정한 주기를 다시 읽는 것일 뿐, MPU의 출력 속도가 아니다.
//   MPU에는 새 샘플이 준비되면 1이 되는 비트가 있다(INT_STATUS 0x3A, bit0 DATA_RDY).
//   그 비트가 몇 번 올라오는지를 세야 MPU의 진짜 속도가 나온다.
//
//   이게 왜 중요하냐 — MPU-6050은 자이로를 끄면 내부 RC 발진기로 도는데 오차가 ±1% 쯤 된다.
//   ESP32는 수정 발진기라 훨씬 정확하다. 둘이 어긋나면 ESP32가 고정 주기로 읽을 때
//   같은 샘플을 두 번 읽거나 한 샘플을 건너뛴다.
//   그리고 **대역 경계 계산에 들어가는 fs는 ESP32가 정한 62.5가 아니라 MPU가 내는 실제 값**이다.
//   여기서 잰 값이 62.5와 1% 넘게 다르면 config.h의 SAMPLE_HZ를 그 값으로 바꾼다.
#include <Wire.h>

constexpr int     PIN_SDA   = 21;
constexpr int     PIN_SCL   = 22;
constexpr uint8_t MPU       = 0x68;
constexpr float   NOMINAL   = 62.5f;   // 1000 / (1 + SMPLRT_DIV)
constexpr uint32_t MEASURE_MS = 20000; // 20초면 충분하다

void w(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU); Wire.write(reg); Wire.write(val); Wire.endTransmission();
}
uint8_t r(uint8_t reg) {
  Wire.beginTransmission(MPU); Wire.write(reg); Wire.endTransmission(false);
  Wire.requestFrom((int)MPU, 1, true);
  return Wire.available() ? Wire.read() : 0;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(400000);

  if (r(0x75) != 0x68) {            // WHO_AM_I
    Serial.println("MPU가 응답하지 않는다. 01_i2c_scan 부터 다시.");
    while (1) delay(1000);
  }

  // 절전 해제를 빼먹으면 값이 전부 0이다. 가장 흔한 첫 실수.
  w(0x6B, 0x00);  delay(100);
  // 순서 주의 — DLPF를 먼저 켜야 내부 출력이 1 kHz가 되고,
  // 그 다음에야 SMPLRT_DIV가 1000/(1+DIV)로 동작한다. 반대면 기준이 8 kHz다.
  w(0x1A, 0x04);                    // DLPF ≈21 Hz
  w(0x19, 15);                      // 1000/(1+15) = 62.5 Hz
  w(0x1C, 0x00);                    // 가속도 ±2 g  → LSB 16384 = 1 g
  w(0x6C, 0x07);                    // 자이로 3축 standby (안 쓴다, 전류 아낌)

  Serial.println("\n=== MPU 실측 ===");
  Serial.println("기기를 책상에 가만히 두세요. 20초 잽니다.\n");
}

void loop() {
  uint32_t t0 = millis(), n = 0;
  double sum = 0, sum2 = 0, lo = 9, hi = 0;

  // DATA_RDY가 0→1로 바뀌는 순간만 센다. 같은 샘플을 두 번 세지 않기 위해서다.
  uint8_t prev = 0;
  while (millis() - t0 < MEASURE_MS) {
    uint8_t now = r(0x3A) & 0x01;
    if (now && !prev) {
      Wire.beginTransmission(MPU); Wire.write(0x3B); Wire.endTransmission(false);
      Wire.requestFrom((int)MPU, 6, true);
      if (Wire.available() >= 6) {
        int16_t ax = (Wire.read() << 8) | Wire.read();
        int16_t ay = (Wire.read() << 8) | Wire.read();
        int16_t az = (Wire.read() << 8) | Wire.read();
        float g = sqrtf((float)ax*ax + (float)ay*ay + (float)az*az) / 16384.0f;
        sum += g; sum2 += (double)g * g; n++;
        if (g < lo) lo = g;
        if (g > hi) hi = g;
      }
    }
    prev = now;
  }

  uint32_t el = millis() - t0;
  float fs = n * 1000.0f / el;
  float mean = n ? sum / n : 0;
  float sd = n ? sqrt(sum2 / n - (double)mean * mean) : 0;
  float err = (fs - NOMINAL) / NOMINAL * 100.0f;

  Serial.printf("샘플 %lu개 / %.1f초  →  실측 %.3f Hz  (기대 %.1f, 오차 %+.2f%%)\n",
                (unsigned long)n, el / 1000.0f, fs, NOMINAL, err);
  Serial.printf("magnitude  평균 %.4f g · 표준편차 %.4f · 범위 %.3f~%.3f\n", mean, sd, lo, hi);

  Serial.println();
  bool ok_fs  = fabsf(err) <= 1.0f;
  bool ok_mag = fabsf(mean - 1.0f) <= 0.05f;
  Serial.printf("  [%s] 샘플레이트 ±1%%\n", ok_fs  ? "통과" : "실패");
  Serial.printf("  [%s] 정지 magnitude 1.00 ± 0.05 g\n", ok_mag ? "통과" : "실패");

  if (!ok_fs)
    Serial.printf("  → config.h 의 SAMPLE_HZ 를 %.2ff 로 바꾸고 crosscheck.py 를 다시 돌린다.\n", fs);
  if (!ok_mag) {
    Serial.println("  → ±2 g 설정(0x1C=0x00)이 맞는지, 16384로 나눴는지 확인한다.");
    Serial.println("    1.00이 아니라 2.00 근처면 레인지가 ±4 g로 잡혀 있다.");
  }
  if (ok_fs && ok_mag) Serial.println("\n  W2 통과. 다음은 걸음 검출(W3).");

  Serial.println("\n다시 재려면 리셋.\n");
  while (1) delay(1000);
}
