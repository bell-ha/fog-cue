// 04_wifi_link — Wi-Fi 시험
// 벨트가 Wi-Fi에 붙어 http://fogcue.local 로 기다린다. 맥북의 tools/app.html 이 여기에 붙는다.
//   벨트 → 화면: /events (이벤트 스트림). 버튼(D14)을 누르면 "e,<번호>" (동결 감지 시험)
//   화면 → 벨트: /cmd?c=<글자> → 시리얼에 찍는다
// 본 펌웨어와 상관없는 시험용 스케치다.
#include <WiFi.h>
#include <ESPmDNS.h>
#include "wifi_secret.h"

static const int PIN_BUTTON = 14;
static const int MAX_VIEWERS = 4;          // 동시에 붙을 수 있는 화면 수

WiFiServer server(80);
WiFiClient viewers[MAX_VIEWERS];
int seq = 0;

void connectWifi() {
  Serial.printf("[WIFI] %s 에 접속 중", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);                    // 응답이 늦어지지 않게
  int n = WiFi.scanNetworks();             // 벨트에 보이는 Wi-Fi (2.4GHz만 보인다)
  Serial.printf("\n[WIFI] 보이는 Wi-Fi %d개:", n);
  for (int i = 0; i < n; i++) Serial.printf(" %s(%d)", WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  Serial.println();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) { delay(500); Serial.print("."); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("[WIFI] 접속 %lu ms · IP %s · 세기 %d dBm\n",
                  millis() - t0, WiFi.localIP().toString().c_str(), WiFi.RSSI());
  else
    Serial.printf("[WIFI] 실패 (상태 %d). 이름 · 비밀번호 · 2.4GHz인지 확인\n", WiFi.status());
}

// 붙어 있는 모든 화면에 한 줄 보낸다
void broadcast(const String& line) {
  int n = 0;
  for (auto& v : viewers) {
    if (v && v.connected()) { v.print("data: " + line + "\n\n"); n++; }
  }
  Serial.printf("[SEND] %s → 화면 %d개\n", line.c_str(), n);
}

void reply(WiFiClient& c, const char* type, const String& body) {
  c.printf("HTTP/1.1 200 OK\r\nContent-Type: %s\r\nAccess-Control-Allow-Origin: *\r\n"
           "Content-Length: %u\r\nConnection: close\r\n\r\n", type, body.length());
  c.print(body);
  c.stop();
}

void handleClient() {
  WiFiClient c = server.available();
  if (!c) return;
  c.setTimeout(500);
  String first = c.readStringUntil('\n');          // GET /path HTTP/1.1
  while (c.connected()) {                           // 나머지 헤더는 버린다
    String h = c.readStringUntil('\n');
    if (h.length() <= 1) break;
  }
  int a = first.indexOf(' '), b = first.indexOf(' ', a + 1);
  String path = (a > 0 && b > a) ? first.substring(a + 1, b) : "/";

  if (path.startsWith("/events")) {
    for (auto& v : viewers) {
      if (!v || !v.connected()) {
        v = c;
        v.print("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\n"
                "Access-Control-Allow-Origin: *\r\nConnection: keep-alive\r\n\r\n");
        v.print("data: hello\n\n");
        Serial.println("[VIEW] 화면 붙음");
        return;
      }
    }
    reply(c, "text/plain", "busy");
  } else if (path.startsWith("/cmd?c=")) {
    Serial.printf("[RECV] %s\n", path.substring(7).c_str());
    reply(c, "text/plain", "ok");
  } else {
    reply(c, "text/plain; charset=utf-8", "FogCue\n");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n04_wifi_link");
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  WiFi.setAutoReconnect(true);             // 붙은 뒤 끊기면 알아서 다시 붙는다
  connectWifi();
  if (MDNS.begin("fogcue")) MDNS.addService("http", "tcp", 80);
  server.begin();
  Serial.println("[BTN ] 버튼을 누르면 시험 알림을 보낸다");
}

void loop() {
  // 켤 때 못 붙었으면 10초마다 다시 시도한다
  static uint32_t lastTry = 0;
  static bool wasUp = false;
  bool up = WiFi.status() == WL_CONNECTED;
  if (!up && millis() - lastTry > 10000) {
    lastTry = millis();
    Serial.println("[WIFI] 다시 시도");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
  if (up && !wasUp) Serial.printf("[WIFI] 연결됨 · IP %s\n", WiFi.localIP().toString().c_str());
  wasUp = up;
  if (!up) { delay(100); return; }
  handleClient();

  // 화면이 연결을 확인할 수 있게 5초마다 신호
  static uint32_t lastBeat = 0;
  if (millis() - lastBeat > 5000) {
    lastBeat = millis();
    for (auto& v : viewers) if (v && v.connected()) v.print(": beat\n\n");
  }

  static int stable = HIGH, last = HIGH, same = 0;
  int v = digitalRead(PIN_BUTTON);
  same = (v == last) ? same + 1 : 0;
  last = v;
  if (same == 3 && v != stable) {
    stable = v;
    if (v == LOW) { Serial.println("[BTN ] 누름"); broadcast("e," + String(++seq)); }
  }
  delay(10);
}
