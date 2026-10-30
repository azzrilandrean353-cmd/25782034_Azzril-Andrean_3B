/*
 * ==============================================================================
 * PRAKTIKUM 7: SIMPUL SENSOR-AKTUATOR IOT UNTUK BROKER LOKAL
 * VERSI DIAGNOSTIK: ada pemindaian Wi-Fi, timeout koneksi, dan pesan status
 * ==============================================================================
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// 1. Kredensial Wi-Fi (hotspot HARUS 2.4 GHz, keamanan WPA2)
const char* ssid     = "Es";
const char* password = "121506123";

// 2. Konfigurasi Broker MQTT Lokal (IPv4 laptop dari 'ipconfig')
const char* mqttServer = "10.89.221.211";
const int   mqttPort   = 1883;

// 3. Identitas Simpul & Topik
const char* clientId      = "ESP8266_Local_Node01";
const char* topicPubTele  = "kampus/labiot/telemetri";
const char* topicSubCmd   = "kampus/labiot/kendali";

// 4. Alokasi Pin
const byte dhtPin = 2; // D4 (GPIO 2)
const byte ledPin = 5; // D1 (GPIO 5)

#define DHTTYPE DHT22 // Ganti DHT11 jika sensor berwarna biru

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTeleTime = 0;
const unsigned long teleInterval = 4000;
const unsigned long wifiTimeoutMs = 20000; // batas waktu satu percobaan konek

// ------------------------------------------------------------------------------
// Fungsi bantu diagnostik
// ------------------------------------------------------------------------------
const char* encName(uint8_t t) {
  switch (t) {
    case ENC_TYPE_WEP:  return "WEP";
    case ENC_TYPE_TKIP: return "WPA-TKIP";
    case ENC_TYPE_CCMP: return "WPA2-CCMP";
    case ENC_TYPE_NONE: return "Terbuka";
    case ENC_TYPE_AUTO: return "WPA/WPA2";
    default:            return "?";
  }
}

const char* wifiStatusName(wl_status_t s) {
  switch (s) {
    case WL_IDLE_STATUS:     return "IDLE";
    case WL_NO_SSID_AVAIL:   return "SSID TIDAK DITEMUKAN";
    case WL_CONNECT_FAILED:  return "GAGAL (password/keamanan tidak cocok)";
    case WL_WRONG_PASSWORD:  return "PASSWORD SALAH";
    case WL_CONNECTION_LOST: return "KONEKSI TERPUTUS";
    case WL_DISCONNECTED:    return "BELUM TERHUBUNG";
    case WL_CONNECTED:       return "TERHUBUNG";
    default:                 return "TIDAK DIKENAL";
  }
}

const char* mqttStateName(int rc) {
  switch (rc) {
    case -4: return "timeout, broker tidak merespons";
    case -3: return "koneksi terputus";
    case -2: return "gagal konek (cek IP broker, firewall port 1883, subnet)";
    case -1: return "terputus";
    case  0: return "terhubung";
    case  1: return "protokol ditolak";
    case  2: return "client ID ditolak";
    case  3: return "broker tidak tersedia";
    case  4: return "username/password salah";
    case  5: return "tidak diizinkan (butuh kredensial)";
    default: return "tidak dikenal";
  }
}

// Memindai jaringan sekitar dan menandai SSID target. Return true jika ketemu.
bool scanWiFi() {
  Serial.println("\n[SCAN] Memindai jaringan Wi-Fi 2.4 GHz...");
  int n = WiFi.scanNetworks();

  if (n <= 0) {
    Serial.println("[SCAN] Tidak ada jaringan yang terdeteksi.");
    return false;
  }

  bool found = false;
  Serial.printf("[SCAN] Ditemukan %d jaringan:\n", n);
  for (int i = 0; i < n; i++) {
    bool match = (WiFi.SSID(i) == String(ssid));
    Serial.printf("  %2d. %-24s RSSI %4d dBm | CH %2d | %s%s\n",
                  i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                  encName(WiFi.encryptionType(i)),
                  match ? "   <== TARGET" : "");
    if (match) found = true;
  }
  WiFi.scanDelete();
  return found;
}

// ------------------------------------------------------------------------------
// Penanganan paket masuk dari Node-RED
// ------------------------------------------------------------------------------
void onMessageCallback(char* topic, byte* payload, unsigned int length) {
  char incomingBuffer[length + 1];
  memcpy(incomingBuffer, payload, length);
  incomingBuffer[length] = '\0';

  Serial.println("\n----------------------------------------");
  Serial.printf("[INSTRUKSI MASUK] Topik: %s\n", topic);
  Serial.printf("[PAYLOAD]         : %s\n", incomingBuffer);

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, incomingBuffer);

  if (err) {
    Serial.printf("[ERROR] Gagal parsing JSON instruksi: %s\n", err.c_str());
    return;
  }

  if (doc.containsKey("led")) {
    int stateLED = doc["led"];
    digitalWrite(ledPin, stateLED == 1 ? HIGH : LOW);
    Serial.printf("--> [AKTUATOR] Status LED diubah: %s\n", stateLED == 1 ? "MENYALA (ON)" : "PADAM (OFF)");
  } else {
    Serial.println("[WARNING] Kunci 'led' tidak ditemukan dalam payload!");
  }
  Serial.println("----------------------------------------");
}

// ------------------------------------------------------------------------------
// Koneksi Wi-Fi: scan dulu, lalu konek dengan timeout, ulangi sampai berhasil
// ------------------------------------------------------------------------------
void connectToWiFi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  while (WiFi.status() != WL_CONNECTED) {
    if (!scanWiFi()) {
      Serial.printf("[Wi-Fi] SSID \"%s\" TIDAK terlihat oleh NodeMCU.\n", ssid);
      Serial.println("        Cek: hotspot harus 2.4 GHz, nama persis sama (huruf besar/kecil), ponsel dekat NodeMCU.");
      delay(5000);
      continue;
    }

    Serial.printf("[Wi-Fi] Menghubungkan ke \"%s\"", ssid);
    WiFi.begin(ssid, password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < wifiTimeoutMs) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
      Serial.printf("[Wi-Fi] Gagal: %s (kode %d). Mencoba lagi...\n",
                    wifiStatusName(WiFi.status()), WiFi.status());
      WiFi.disconnect();
      delay(1000);
    }
  }

  Serial.println("[Wi-Fi] Berhasil Terhubung!");
  Serial.print("[Wi-Fi] IP NodeMCU : "); Serial.println(WiFi.localIP());
  Serial.print("[Wi-Fi] Gateway    : "); Serial.println(WiFi.gatewayIP());
  Serial.printf("[Wi-Fi] Sinyal     : %d dBm\n", WiFi.RSSI());

  // Peringatan jika broker berada di subnet berbeda dari NodeMCU
  IPAddress broker;
  if (broker.fromString(mqttServer)) {
    uint32_t mask = (uint32_t)WiFi.subnetMask();
    if (((uint32_t)WiFi.localIP() & mask) != ((uint32_t)broker & mask)) {
      Serial.println("[PERINGATAN] IP broker berada di subnet berbeda dari NodeMCU!");
      Serial.println("             Periksa kembali 'mqttServer' dengan 'ipconfig' di laptop.");
    }
  }
}

// ------------------------------------------------------------------------------
// Koneksi MQTT
// ------------------------------------------------------------------------------
void reconnectMQTT() {
  while (!client.connected()) {
    if (WiFi.status() != WL_CONNECTED) return; // Wi-Fi putus, kembali ke loop()

    Serial.printf("[MQTT] Menghubungkan ke Broker Lokal (%s:%d)...", mqttServer, mqttPort);

    if (client.connect(clientId, "admin", "rahasia123")) {
      Serial.println(" Terhubung dengan Autentikasi Sah!");
      client.subscribe(topicSubCmd);
      Serial.printf("[MQTT] Berlangganan ke topik: %s\n", topicSubCmd);
    } else {
      int rc = client.state();
      Serial.printf(" Gagal (rc=%d: %s). Coba lagi dalam 4 detik...\n", rc, mqttStateName(rc));
      delay(4000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== Praktikum 7: Node ESP8266 (Diagnostik) ===");

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  dht.begin();
  connectToWiFi();

  client.setServer(mqttServer, mqttPort);
  client.setCallback(onMessageCallback);
  client.setBufferSize(512);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastTeleTime >= teleInterval) {
    lastTeleTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      StaticJsonDocument<256> docOut;
      docOut["node_id"]    = clientId;
      docOut["suhu"]       = serialized(String(t, 2));
      docOut["kelembapan"] = serialized(String(h, 2));
      docOut["status_led"] = digitalRead(ledPin);

      char teleBuffer[256];
      serializeJson(docOut, teleBuffer);

      bool success = client.publish(topicPubTele, teleBuffer);
      if (success) {
        Serial.printf("[TELEMETRI TERKIRIM] %s\n", teleBuffer);
      } else {
        Serial.println("[ERROR] Gagal mengirim paket telemetri ke broker lokal!");
      }
    } else {
      Serial.println("[ERROR] Gagal membaca data dari sensor DHT fisik!");
    }
  }
}
