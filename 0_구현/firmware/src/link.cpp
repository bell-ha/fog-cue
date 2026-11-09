#include "link.h"

TeeOut out;

#if LINK_BLE
// 블루투스는 link_ble.cpp
#elif !LINK_WIFI
// Wi-Fi 설정이 없으면 USB만 쓴다
namespace wlink {
  void begin() { Serial.println("[WIFI] wifi_secret.h 없음 — USB만 쓴다"); }
  void write(const uint8_t*, size_t) {}
  bool readCommand(char*, size_t) { return false; }
  int  viewers() { return 0; }
}
#else

#include <WiFi.h>
#include <ESPmDNS.h>
#include <lwip/sockets.h>
#include <errno.h>

namespace {
  constexpr int    MAX_VIEWERS = 4;
  constexpr size_t RING = 8192;          // 약 2초 분량. 화면이 그보다 오래 막히면 버린다

  // loop(코어 1)가 넣고 작업(코어 0)이 꺼낸다
  uint8_t      ring[RING];
  size_t       head = 0, tail = 0;       // head = 다음에 넣을 자리, tail = 다음에 꺼낼 자리
  portMUX_TYPE ring_mux = portMUX_INITIALIZER_UNLOCKED;

  QueueHandle_t cmd_q;                   // Wi-Fi로 온 명령 (24글자)
  struct Cmd { char s[24]; };

  WiFiServer server(80);
  // 화면 하나. 못 보낸 줄은 pend에 쌓아 두고 다음 차례에 이어 보낸다.
  // WiFiClient::write는 신호가 약하면 최대 10초를 기다린다 — 그동안 명령도 못 받고 다른 화면도 멈춘다.
  // 그래서 소켓에 기다리지 않고(MSG_DONTWAIT) 들어가는 만큼만 넣는다
  struct Viewer { WiFiClient c; String pend; uint32_t since = 0; };
  Viewer viewers[MAX_VIEWERS];
  volatile int n_viewers = 0;
  constexpr size_t PEND_MAX = 24000;    // 약 7초 분량. 이만큼 밀리면 화면이 사라진 것으로 보고 끊는다

  // 버퍼에서 '완성된 줄'만 꺼내 SSE 꼴(data: 줄\n\n)로 만든다
  String takeLines() {
    String outbuf;
    portENTER_CRITICAL(&ring_mux);
    size_t h = head;
    portEXIT_CRITICAL(&ring_mux);
    // 마지막 줄바꿈까지만 꺼낸다. 반쯤 쓰인 줄은 다음 번에
    size_t end = tail, i = tail;
    while (i != h) { if (ring[i] == '\n') end = (i + 1) % RING; i = (i + 1) % RING; }
    String line;
    for (i = tail; i != end; i = (i + 1) % RING) {
      char c = (char)ring[i];
      if (c == '\n') { if (line.length()) { outbuf += "data: "; outbuf += line; outbuf += "\n\n"; } line = ""; }
      else if (c != '\r') line += c;
    }
    portENTER_CRITICAL(&ring_mux);
    tail = end;
    portEXIT_CRITICAL(&ring_mux);
    return outbuf;
  }

  String urlDecode(const String& s) {
    String r;
    for (size_t i = 0; i < s.length(); i++) {
      char c = s[i];
      if (c == '%' && i + 2 < s.length()) {
        r += (char)strtol(s.substring(i + 1, i + 3).c_str(), nullptr, 16);
        i += 2;
      } else if (c == '+') r += ' ';
      else r += c;
    }
    return r;
  }

  void reply(WiFiClient& c, const String& body) {
    c.printf("HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\n"
             "Content-Length: %u\r\nConnection: close\r\n\r\n", body.length());
    c.print(body);
    c.stop();
  }

  void acceptClient() {
    WiFiClient c = server.available();
    if (!c) return;
    c.setTimeout(1);                     // 초 단위. 요청을 오래 기다리지 않는다
    String first = c.readStringUntil('\n');
    while (c.connected()) { String h = c.readStringUntil('\n'); if (h.length() <= 1) break; }
    int a = first.indexOf(' '), b = first.indexOf(' ', a + 1);
    String path = (a > 0 && b > a) ? first.substring(a + 1, b) : "/";

    if (path.startsWith("/events")) {
      // 빈 자리가 없으면 가장 오래된 화면을 내보낸다.
      // 멀리 갔다 오면 끊긴 화면이 '연결됨'으로 남아 자리를 차지한다 — 그러면 새로 붙을 수가 없었다
      Viewer* slot = nullptr;
      for (auto& v : viewers) if (!(v.c && v.c.connected())) { slot = &v; break; }
      if (!slot) {
        slot = &viewers[0];
        for (auto& v : viewers) if (v.since < slot->since) slot = &v;
        slot->c.stop();
      }
      slot->c = c;
      slot->c.setNoDelay(true);
      slot->pend = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\n"
                   "Access-Control-Allow-Origin: *\r\nConnection: keep-alive\r\n\r\ndata: hello\n\n";
      slot->since = millis();
      Serial.printf("[WIFI] 화면 붙음 · 신호 %d dBm\n", WiFi.RSSI());
    } else if (path.startsWith("/cmd?c=")) {
      Cmd m = {};
      String s = urlDecode(path.substring(7));
      strncpy(m.s, s.c_str(), sizeof(m.s) - 1);
      xQueueSend(cmd_q, &m, 0);
      reply(c, "ok");
    } else {
      reply(c, "FogCue\n");
    }
  }

  // 붙을 Wi-Fi 목록. wifi_secret.h에 WIFI_SSID2를 적으면 두 번째도 쓴다 (예: 휴대폰 핫스팟).
  // 안 붙으면 10초마다 다음 것으로 넘어간다 — 연구실에서는 공유기, 밖에서는 핫스팟에 알아서 붙는다
  struct Net { const char* ssid; const char* pass; };
  const Net nets[] = {
    { WIFI_SSID, WIFI_PASS },
#ifdef WIFI_SSID2
    { WIFI_SSID2, WIFI_PASS2 },
#endif
  };
  constexpr int N_NETS = sizeof(nets) / sizeof(nets[0]);

  // 쌓인 것을 기다리지 않고 들어가는 만큼 보낸다
  void flush(Viewer& v) {
    if (!v.pend.length()) return;
    int fd = v.c.fd();
    if (fd < 0) { v.c.stop(); v.pend = ""; return; }
    int r = send(fd, v.pend.c_str(), v.pend.length(), MSG_DONTWAIT);
    if (r > 0) v.pend.remove(0, r);
    else if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) { v.c.stop(); v.pend = ""; }
  }

  void task(void*) {
    uint32_t last_try = 0, last_beat = 0, last_rssi = 0;
    bool was_up = false, mdns = false;
    int net = 0;
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);                // 응답이 늦어지지 않게 (전류는 더 먹는다)
    WiFi.setAutoReconnect(true);
    WiFi.begin(nets[net].ssid, nets[net].pass);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);  // 가장 세게 (멀리서 쓰려고)
    last_try = millis();

    for (;;) {
      bool up = WiFi.status() == WL_CONNECTED;
      if (up && !was_up) {
        Serial.printf("[WIFI] 연결됨 · %s · IP %s · 신호 %d dBm\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
        if (!mdns) mdns = MDNS.begin("fogcue");
        server.begin();
      }
      if (!up && was_up) Serial.println("[WIFI] 끊김");
      was_up = up;
      if (!up) {
        if (millis() - last_try > 10000) {
          last_try = millis();
          net = (net + 1) % N_NETS;
          WiFi.disconnect();
          WiFi.begin(nets[net].ssid, nets[net].pass);
        }
        // 화면이 없으니 버퍼는 비운다 — 다시 붙었을 때 옛날 값이 한꺼번에 가지 않게
        portENTER_CRITICAL(&ring_mux); tail = head; portEXIT_CRITICAL(&ring_mux);
        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
      }

      acceptClient();

      String chunk = takeLines();
      if (millis() - last_beat > 5000) { last_beat = millis(); chunk += ": beat\n\n"; }
      // 신호 세기 — 1초마다 한 줄. w,<dBm>. Wi-Fi 화면에만 간다 (USB 줄에는 없다)
      if (millis() - last_rssi > 1000) { last_rssi = millis(); chunk += "data: w," + String(WiFi.RSSI()) + "\n\n"; }

      int n = 0;
      for (auto& v : viewers) {
        if (!(v.c && v.c.connected())) { v.pend = ""; continue; }
        n++;
        v.pend += chunk;
        flush(v);
        if (v.pend.length() > PEND_MAX) {
          Serial.println("[WIFI] 화면이 응답하지 않아 끊음");
          v.c.stop(); v.pend = "";
        }
      }
      n_viewers = n;
      vTaskDelay(pdMS_TO_TICKS(20));     // 20 ms마다 모아서 보낸다 (62.5 Hz 줄을 1~2줄씩)
    }
  }
}

namespace wlink {
  void begin() {
    cmd_q = xQueueCreate(8, sizeof(Cmd));
    xTaskCreatePinnedToCore(task, "link", 8192, nullptr, 1, nullptr, 0);
  }

  void write(const uint8_t* data, size_t n) {
    portENTER_CRITICAL(&ring_mux);
    for (size_t i = 0; i < n; i++) {
      size_t next = (head + 1) % RING;
      if (next == tail) break;           // 가득 찼다 — 버린다
      ring[head] = data[i];
      head = next;
    }
    portEXIT_CRITICAL(&ring_mux);
  }

  bool readCommand(char* outp, size_t n) {
    Cmd m;
    if (!cmd_q || xQueueReceive(cmd_q, &m, 0) != pdTRUE) return false;
    strncpy(outp, m.s, n - 1);
    outp[n - 1] = '\0';
    return true;
  }

  int viewers() { return n_viewers; }
}
#endif
