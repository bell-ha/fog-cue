보행동결 자극 시스템 — 구현 (0_구현)

걷다가 발이 굳는 순간에만 박자를 들려주는 허리 벨트. ESP32 + MPU-6050.
학습용 프로토타입이다. 의료기기가 아니다.


1 ---------------- 폴더
구현기록.txt               아이디어 발표와 달라진 점, 그 근거 (중간발표 자료용)
걷기와_동결흉내_차이.txt     걷기와 동결 흉내를 숫자로 비교한 것
firmware/                  ESP32 코드. firmware.ino · config.h(설정값과 근거) · src/(걸음·동결·자극)
analysis/                  PC에서 기기와 같은 계산을 하는 Python 코드
tools/                     walk_monitor.html(웹 화면) · bringup/(처음 배선 확인용 스케치)
data/                      기록 CSV


2 ---------------- 실행
* 단계 고르기: firmware/firmware.ino 맨 위 #define STAGE
  1 걸음 세기 · 2 멈추면 울림 · 3 동결만 골라냄 · 4 부가 기능.  지금은 3.
* 업로드: Arduino IDE 또는 arduino-cli. 보드 ESP32 Dev Module, 업로드 속도 460800 (921600은 실패한다).
* 웹 화면: tools/walk_monitor.html 을 Chrome으로 열고 '포트 연결'.
  업로드할 때는 웹 화면에서 '연결 끊기'를 먼저 누른다 (포트는 한 프로그램만 쓸 수 있다).
* 부저 소리: 웹 화면 위쪽 '부저 소리'에서 크기를 바꾸고(기기 기본 20%) '시험 소리'로 들어 본다
* PC와 기기 값 맞추기: python3 analysis/crosscheck.py   — 판정 코드를 고치면 다시 돌린다
* 환자 데이터 평가: python3 analysis/evaluate.py <DAPHNet 폴더>   — https://doi.org/10.24432/C56K78


3 ---------------- 배선
ESP32 3V3  ── MPU-6050 VCC                     VIN(5V)이 아니라 3V3
ESP32 GND  ── MPU-6050 GND · 부저(−) · 스위치
GPIO21     ── SDA        GPIO22 ── SCL          바꿔 꽂으면 센서가 안 보인다
GPIO25     ── 부저(+)     GPIO14 ── 스위치 (누르면 LOW)
GPIO26     ── 진동 모터 모듈 IN (VCC는 3V3, GND는 GND)
센서 주소 0x68. 센서는 허리 뒤쪽에 단단히 고정한다 (손에 들면 결과가 흔들린다).


4 ---------------- 지우면 안 되는 안전장치 (firmware/config.h)
* FADEIN_MS     첫 박을 작은 소리에서 시작한다 — 갑자기 울리면 놀라 넘어질 수 있다
* CUE_MAX_MS    30초가 지나면 소리를 강제로 멈춘다 — 어떤 경우에도 계속 울리지 않게
* FI_MIN_POWER  움직임이 거의 없으면 판정하지 않는다 — 가만히 선 것을 동결로 보지 않게
