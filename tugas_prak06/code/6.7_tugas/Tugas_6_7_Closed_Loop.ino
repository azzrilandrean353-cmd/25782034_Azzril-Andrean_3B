#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

const char* ssid     = "MACHMUDALHASAN";      
const char* password = "Dodol Garut";   

const char* mqttServer = "broker.hivemq.com";
const int   mqttPort   = 1883;
const char* clientId   = "ESP8266_LabIoT_25782034_Tugas";  // harus unik

// Topik (ganti 25782034 dengan NIM Anda jika berbeda)
const char* topicTelemetri = "kampus/labiot/25782034/telemetri";
const char* topicKendali   = "kampus/labiot/25782034/kendali";
const char* topicAlert     = "kampus/labiot/25782034/alert";
const char* topicStatus    = "kampus/labiot/25782034/status";

// Pesan status / LWT
const char* msgOnline  = "{\"status\":\"ONLINE\",\"keterangan\":\"Perangkat Beroperasi Normal\"}";
const char* msgOffline = "{\"status\":\"OFFLINE\",\"keterangan\":\"Perangkat Terputus Tak Terduga\"}";

// ===================== PIN & PARAMETER =====================
const byte dhtPin = 2;   // D4 (GPIO 2)
const byte ledPin = 5;   // D1 (GPIO 5)
#define DHTTYPE DHT22    // ubah ke DHT11 jika memakai DHT11

const float SUHU_BATAS = 31.0;                  // ambang bahaya (derajat C)
const unsigned long telemetryInterval = 4000;   // kirim telemetri tiap 4 detik
const unsigned long reconnectInterval = 5000;   // jeda percobaan sambung ulang

// ===================== OBJEK & STATE =====================
DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTelemetryTime = 0;
unsigned long lastWifiDot       = 0;
unsigned long lastReconnectTry  = 0;

bool dangerLatched = false;   // true = alert bahaya sudah dikirim (state latch)
bool wifiReported  = false;   // agar IP hanya dicetak sekali

// ===================== CALLBACK: KENDALI MANUAL =====================
void callback(char* topic, byte* payload, unsigned int length) {
  char buf[length + 1];
  memcpy(buf, payload, length);
  buf[length] = '\0';

  Serial.printf("[KENDALI DITERIMA] %s | %s\n", topic, buf);

  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, buf);
  if (err) {
    Serial.println("Gagal membedah payload JSON!");
    return;
  }

  if (!doc.containsKey("led")) return;

  // Saat BAHAYA, LED dikunci ON oleh sistem proteksi; perintah manual ditolak
  if (dangerLatched) {
    Serial.println("--> DITOLAK: kondisi BAHAYA, LED dikunci ON oleh sistem proteksi");
    return;
  }

  int val = doc["led"];
  digitalWrite(ledPin, val == 1 ? HIGH : LOW);
  Serial.printf("--> [MANUAL] LED %s\n", val == 1 ? "MENYALA" : "PADAM");
}

// ===================== SAMBUNG ULANG MQTT (NON-BLOCKING) =====================
bool connectMQTT() {
  Serial.print("Menghubungkan ke MQTT Broker...");

  // connect(id, user, pass, willTopic, willQoS, willRetain, willMessage)
  if (client.connect(clientId, NULL, NULL, topicStatus, 1, true, msgOffline)) {
    Serial.println(" Sukses!");
    client.publish(topicStatus, msgOnline, true);   // pesan ONLINE (retained)
    client.subscribe(topicKendali);
    Serial.printf("Subskripsi: %s\n", topicKendali);
    return true;
  }

  Serial.printf(" Gagal (rc=%d). Coba lagi dalam 5 detik.\n", client.state());
  return false;
}

// ===================== PUBLISH ALERT =====================
void publishAlert(const char* status, const char* parameter, float suhu) {
  StaticJsonDocument<128> doc;
  doc["status"]     = status;
  doc["parameter"]  = parameter;
  doc["nilai_suhu"] = serialized(String(suhu, 1));

  char buf[128];
  serializeJson(doc, buf);

  if (client.publish(topicAlert, buf)) {
    Serial.printf("[ALERT TERKIRIM] %s\n", buf);
  } else {
    Serial.println("[ERROR] Gagal mengirim alert!");
  }
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("\nMenghubungkan ke Wi-Fi: %s\n", ssid);

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  client.setBufferSize(512);
}

// ===================== LOOP =====================
void loop() {
  unsigned long now = millis();

  // ---- 1. Pastikan Wi-Fi tersambung (tanpa blocking) ----
  if (WiFi.status() != WL_CONNECTED) {
    wifiReported = false;
    if (now - lastWifiDot >= 500) {
      lastWifiDot = now;
      Serial.print(".");
    }
    return;
  }
  if (!wifiReported) {
    wifiReported = true;
    Serial.print("\nWi-Fi Terkoneksi! IP: ");
    Serial.println(WiFi.localIP());
  }

  // ---- 2. Pastikan MQTT tersambung (retry berbasis timer) ----
  if (!client.connected()) {
    if (now - lastReconnectTry >= reconnectInterval) {
      lastReconnectTry = now;
      connectMQTT();
    }
    return;
  }
  client.loop();   // keep-alive + callback

  // ---- 3. Siklus telemetri & pengawasan suhu ----
  if (now - lastTelemetryTime >= telemetryInterval) {
    lastTelemetryTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
      Serial.println("Peringatan: Gagal membaca data DHT!");
      return;
    }

    bool panas = (t > SUHU_BATAS);

    // --- Edge safety engine dengan state latch ---
    if (panas && !dangerLatched) {
      // Transisi AMAN -> BAHAYA: kirim alert SATU KALI, LED otomatis ON
      dangerLatched = true;
      digitalWrite(ledPin, HIGH);
      publishAlert("BAHAYA", "OVERHEAT", t);
    } else if (!panas && dangerLatched) {
      // Transisi BAHAYA -> AMAN: lepas latch, LED OFF, kirim info pulih
      dangerLatched = false;
      digitalWrite(ledPin, LOW);
      publishAlert("NORMAL", "OVERHEAT", t);
    }

    // --- Telemetri rutin ---
    StaticJsonDocument<256> doc;
    doc["node_id"]      = clientId;
    doc["suhu"]         = serialized(String(t, 2));
    doc["kelembapan"]   = serialized(String(h, 2));
    doc["status_led"]   = digitalRead(ledPin);
    doc["kondisi_suhu"] = panas ? "PANAS" : "NORMAL";

    char buf[256];
    serializeJson(doc, buf);

    if (client.publish(topicTelemetri, buf)) {
      Serial.printf("[TELEMETRI TERKIRIM] %s\n", buf);
    } else {
      Serial.println("[ERROR] Gagal memublikasikan telemetri!");
    }
  }
}
