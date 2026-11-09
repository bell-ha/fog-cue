// link_ble.cpp — 블루투스(BLE)로 웹 화면과 잇는다 (link.h에서 LINK_BLE 1일 때)
//
// 맥북 Chrome이 벨트에 직접 붙는다 (Web Bluetooth). 공유기가 없어도 되고, 맥북 가까이에서 가장 잘 된다.
// 꼴은 Nordic UART — 휴대폰 앱 'nRF Connect'로도 열어 볼 수 있다.
//   TX (알림)  벨트 → 화면. USB로 나가는 줄과 같은 글자를 이어서 보낸다. 한 알림이 줄 중간에서 끊길 수 있어
//              화면이 줄바꿈으로 다시 잇는다
//   RX (쓰기)  화면 → 벨트. 명령 한 줄 + 줄바꿈
// 신호 세기는 1초마다 w,<dBm> 한 줄로 끼워 보낸다 (USB 줄에는 없다).
#include "link.h"

#if LINK_BLE
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

namespace {
  const char* SVC_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  const char* RX_UUID  = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
  const char* TX_UUID  = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

  constexpr size_t RING = 8192;          // 약 2초 분량. loop(코어 1)가 넣고 작업(코어 0)이 꺼낸다
  uint8_t      ring[RING];
  size_t       head = 0, tail = 0;
  portMUX_TYPE ring_mux = portMUX_INITIALIZER_UNLOCKED;

  QueueHandle_t cmd_q;
  struct Cmd { char s[24]; };

  BLEServer*         server = nullptr;
  BLECharacteristic* tx = nullptr;
  volatile bool      connected = false;
  volatile uint16_t  conn_id = 0;
  esp_bd_addr_t      peer;
  volatile int       rssi = 0;

  constexpr size_t PEND_MAX = 16000;     // 약 5초 분량. 이만큼 밀리면 버린다

  void gapEvent(esp_gap_ble_cb_event_t e, esp_ble_gap_cb_param_t* p) {
    if (e == ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT && p->read_rssi_cmpl.status == ESP_BT_STATUS_SUCCESS)
      rssi = p->read_rssi_cmpl.rssi;
  }

  struct ServerCb : BLEServerCallbacks {
    void onConnect(BLEServer*, esp_ble_gatts_cb_param_t* p) override {
      memcpy(peer, p->connect.remote_bda, sizeof(peer));
      conn_id = p->connect.conn_id;
      rssi = 0;
      connected = true;
    }
    void onDisconnect(BLEServer*) override { connected = false; }
  };

  // 명령은 줄바꿈까지 모아 한 줄씩 넘긴다
  struct RxCb : BLECharacteristicCallbacks {
    char   line[24];
    size_t n = 0;
    void onWrite(BLECharacteristic* c) override {
      std::string v = c->getValue();
      for (char ch : v) {
        if (ch == '\n' || ch == '\r') {
          if (n) { Cmd m = {}; memcpy(m.s, line, n); xQueueSend(cmd_q, &m, 0); n = 0; }
        } else if (n < sizeof(line) - 1) {
          line[n++] = ch;
        }
      }
    }
  };

  // 버퍼에서 완성된 줄만 꺼낸다. 반쯤 쓰인 줄은 다음 번에 — 사이에 w 줄을 끼워도 섞이지 않게
  void takeLines(String& outbuf) {
    portENTER_CRITICAL(&ring_mux);
    size_t h = head;
    portEXIT_CRITICAL(&ring_mux);
    size_t end = tail;
    for (size_t i = tail; i != h; i = (i + 1) % RING) if (ring[i] == '\n') end = (i + 1) % RING;
    for (size_t i = tail; i != end; i = (i + 1) % RING) outbuf += (char)ring[i];
    portENTER_CRITICAL(&ring_mux);
    tail = end;
    portEXIT_CRITICAL(&ring_mux);
  }

  void dropRing() { portENTER_CRITICAL(&ring_mux); tail = head; portEXIT_CRITICAL(&ring_mux); }

  void task(void*) {
    String   pend;
    bool     was = false;
    uint32_t last_rssi = 0;
    for (;;) {
      bool c = connected;
      if (c && !was) {
        Serial.println("[BLE ] 화면 붙음");
        pend = "";
        dropRing();
      }
      if (!c && was) {
        Serial.println("[BLE ] 끊김 · 다시 기다림");
        vTaskDelay(pdMS_TO_TICKS(300));
        BLEDevice::startAdvertising();
      }
      was = c;
      if (!c) { dropRing(); vTaskDelay(pdMS_TO_TICKS(100)); continue; }

      takeLines(pend);
      if (millis() - last_rssi > 1000) {
        last_rssi = millis();
        if (rssi) pend += "w," + String(rssi) + "\n";
        esp_ble_gap_read_rssi(peer);             // 답은 gapEvent로 온다 — 다음 초에 보낸다
      }

      // 한 알림에 MTU-3 바이트까지. 20 ms마다 몇 개씩 — 62.5줄/초(약 3 KB/초)는 한두 개면 된다
      uint16_t mtu = server->getPeerMTU(conn_id);
      size_t   size = mtu > 23 ? mtu - 3 : 20;
      if (size > 180) size = 180;
      for (int k = 0; k < 6 && pend.length(); k++) {
        size_t n = pend.length() < size ? pend.length() : size;
        tx->setValue((uint8_t*)pend.c_str(), n);
        tx->notify();
        pend.remove(0, n);
      }
      if (pend.length() > PEND_MAX) pend = "";
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
}

namespace wlink {
  void begin() {
    cmd_q = xQueueCreate(8, sizeof(Cmd));
    BLEDevice::init("FogCue");
    BLEDevice::setMTU(185);
    BLEDevice::setCustomGapHandler(gapEvent);
    server = BLEDevice::createServer();
    server->setCallbacks(new ServerCb());
    BLEService* svc = server->createService(SVC_UUID);
    tx = svc->createCharacteristic(TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    tx->addDescriptor(new BLE2902());
    BLECharacteristic* rx = svc->createCharacteristic(
        RX_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    rx->setCallbacks(new RxCb());
    svc->start();
    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(SVC_UUID);
    adv->setScanResponse(true);
    BLEDevice::startAdvertising();
    Serial.println("[BLE ] 기다리는 중 · 이름 FogCue");
    xTaskCreatePinnedToCore(task, "link", 6144, nullptr, 1, nullptr, 0);
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

  int viewers() { return connected ? 1 : 0; }
}
#endif
