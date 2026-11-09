보행동결 자극 시스템 — 구현 (0_구현)

걷다가 발이 굳는 순간에만 박자를 들려주는 허리 벨트. ESP32 + MPU-6050.
학습용 프로토타입이다. 의료기기가 아니다.


1 ---------------- 폴더
구현기록.txt               아이디어 발표와 달라진 점, 그 근거 (중간발표 자료용)
걷기와_동결흉내_차이.txt     걷기와 동결 흉내를 숫자로 비교한 것
firmware/                  ESP32 코드. firmware.ino · config.h(설정값과 근거) · src/(걸음·동결·돌기·자극·Wi-Fi)
analysis/                  PC에서 기기와 같은 계산을 하는 Python 코드
tools/                     walk_monitor.html(웹 화면 · 앱 화면) · bringup/(처음 배선 · Wi-Fi 확인용 스케치)
data/                      기록 CSV


2 ---------------- 실행
* 단계 고르기: firmware/firmware.ino 맨 위 #define STAGE
  1 걸음 세기 · 2 멈추면 울림 · 3 동결만 골라냄 · 4 부가 기능.  지금은 3.
* 업로드: Arduino IDE 또는 arduino-cli. 보드 ESP32 Dev Module, 업로드 속도 460800 (921600은 실패한다).
* 웹 화면: tools/walk_monitor.html 을 Chrome으로 열고 USB 또는 Wi-Fi를 골라 '연결'.
  USB로 업로드할 때는 웹 화면에서 '연결 끊기'를 먼저 누른다 (포트는 한 프로그램만 쓸 수 있다).
* Wi-Fi: firmware/wifi_secret.h 에 WIFI_SSID · WIFI_PASS 를 적는다 (git에 안 올라간다. 없으면 USB만 쓴다).
  2.4GHz · 비밀번호 방식만 된다. 벨트 IP는 시리얼의 [WIFI] 줄에 나온다. 맥북도 같은 공유기에 붙어 있어야 한다.
  WIFI_SSID2 · WIFI_PASS2를 더 적으면 두 곳 중 보이는 쪽에 붙는다 (핫스팟이면 주소 칸에 fogcue.local)
  아이폰 핫스팟: '호환성 최대화'를 켠다. 맥북이 192.0.0.2만 받으면 벨트가 안 보인다 →
    networksetup -setv6linklocal Wi-Fi   (핫스팟에서 172.20.10.x를 받는다)
    networksetup -setv6automatic Wi-Fi   (다른 Wi-Fi로 돌아오면 되돌린다)
* 박자 시험: 웹 화면 '기기 진단' 탭의 박자 카드에서 소리 크기 · 빠르기를 바꾸고 '소리 울려 보기 / 진동 울려 보기'
  소리 크기 · 박자 방식은 벨트 플래시에 남는다 (처음 값 20% · 둘 다)
* 돌아서기 시험: '측정' 탭 '방향 전환'. 돌 때 M을 누른다. 벨트를 켜고 1초 가만히 두어야 자이로 영점이 잡힌다
  CSV 마지막 두 열 turn(도는 중이라 판정을 미룸) · yaw(세로축 회전 °/s)
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
