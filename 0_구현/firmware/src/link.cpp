#include "link.h"

TeeOut out;

#if !LINK_WIFI
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
  WiFiClient viewers[MAX_VIEWERS];
  volatile int n_viewers = 0;

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
      for (auto& v : viewers) {
        if (v && v.connected()) continue;
        v = c;
        v.print("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\n"
                "Access-Control-Allow-Origin: *\r\nConnection: keep-alive\r\n\r\n");
        v.print("data: hello\n\n");
        Serial.println("[WIFI] 화면 붙음");
        return;
      }
      reply(c, "busy");
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

  void task(void*) {
    uint32_t last_try = 0, last_beat = 0;
    bool was_up = false, mdns = false;
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);                // 응답이 늦어지지 않게 (전류는 더 먹는다)
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    last_try = millis();

    for (;;) {
      bool up = WiFi.status() == WL_CONNECTED;
      if (up && !was_up) {
        Serial.printf("[WIFI] 연결됨 · %s · IP %s\n", WIFI_SSID, WiFi.localIP().toString().c_str());
        if (!mdns) mdns = MDNS.begin("fogcue");
        server.begin();
      }
      if (!up && was_up) Serial.println("[WIFI] 끊김");
      was_up = up;
      if (!up) {
        if (millis() - last_try > 10000) { last_try = millis(); WiFi.disconnect(); WiFi.begin(WIFI_SSID, WIFI_PASS); }
        // 화면이 없으니 버퍼는 비운다 — 다시 붙었을 때 옛날 값이 한꺼번에 가지 않게
        portENTER_CRITICAL(&ring_mux); tail = head; portEXIT_CRITICAL(&ring_mux);
        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
      }

      acceptClient();

      int n = 0;
      for (auto& v : viewers) if (v && v.connected()) n++;
      n_viewers = n;

      String chunk = takeLines();
      if (millis() - last_beat > 5000) { last_beat = millis(); chunk += ": beat\n\n"; }
      if (chunk.length()) {
        for (auto& v : viewers) {
          if (!v || !v.connected()) continue;
          if (v.write((const uint8_t*)chunk.c_str(), chunk.length()) != chunk.length()) v.stop();
        }
      }
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
