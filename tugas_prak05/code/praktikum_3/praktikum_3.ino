#include <ESP8266WiFi.h>
#include <espnow.h>

typedef struct struct_sensor {
  int id;
  float pembacaan1;
  float pembacaan2;
} struct_sensor;

struct_sensor dataTerima;

// Array penampung data terakhir: Index 0 untuk Node 1, Index 1 untuk Node 2
struct_sensor records[2];

void OnDataRecv(uint8_t *mac_addr, uint8_t *incomingData, uint8_t len) {
  memcpy(&dataTerima, incomingData, sizeof(dataTerima));

  // Validasi ID agar tidak terjadi error index array out-of-bounds
  if (dataTerima.id >= 1 && dataTerima.id <= 2) {
    int index = dataTerima.id - 1;
    records[index].id = dataTerima.id;
    records[index].pembacaan1 = dataTerima.pembacaan1;
    records[index].pembacaan2 = dataTerima.pembacaan2;

    Serial.println("========================================");
    Serial.printf("DATA TERBARU DARI NODE #%d\n", dataTerima.id);
    Serial.printf("Parameter 1 : %.2f\n", records[index].pembacaan1);
    Serial.printf("Parameter 2 : %.2f\n", records[index].pembacaan2);
    Serial.println("========================================");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    Serial.println("Gagal Inisialisasi ESP-NOW Hub!");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("Hub Konsentrator Siap Menerima Data Sensor...");
}

void loop() {
  // Loop bebas menangani tugas lain
}