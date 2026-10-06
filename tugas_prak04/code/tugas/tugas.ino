#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>

const char* ssid = "Infinix GT 30 Pro";
const char* password = "murahcpz";

// Konfigurasi Pin
const byte dhtPin = 2;        // D4 (GPIO 2)
const byte buttonPin = 4;     // D2 (GPIO 4)
const byte ledPin = 12;       // D6 (GPIO 12)
const byte pwmPin = 5;        // D1 (GPIO 5) untuk LED dimmer

DHT dht(dhtPin, DHT11);

// Variabel Pelacak Status
bool ledState = false;
String currentTemp = "--";
String currentHum = "--";
int pwmValue = 0;             // nilai PWM saat ini (0-1023)
int buttonState = LOW;
int lastButtonState = LOW;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
unsigned long lastTime = 0;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ---------------- HTML & JAVASCRIPT (FRONT-END) ----------------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Real-Time IoT Web</title>
  <style>
    body { font-family: Arial; text-align: center; }
    .card { background: #f0f0f0; margin: 20px auto; padding: 20px; max-width: 300px; border-radius: 10px; }
    button { padding: 15px 30px; font-size: 20px; border-radius: 5px; cursor: pointer; color: white;}
    .btn-on { background-color: #4CAF50; }
    .btn-off { background-color: #f44336; }
    input[type=range] { width: 100%; }
  </style>
</head>
<body>
  <h1>Smart Room</h1>

  <div class="card">
    <h2>Suhu: <span id="tempValue">--</span> Celcius</h2>
  </div>

  <div class="card">
    <h2>Kelembapan: <span id="humValue">--</span> %</h2>
  </div>

  <div class="card">
    <h2>LED: <span id="ledStatus">OFF</span></h2>
    <button id="toggleBtn" class="btn-off" onclick="toggleLed()">Turn ON</button>
  </div>

  <!-- BARU: kartu slider dimmer -->
  <div class="card">
    <h2>Dimmer: <span id="pwmValue">0</span></h2>
    <input type="range" min="0" max="1023" value="0" id="pwmSlider"
           oninput="sendPWM(this.value)">
  </div>

  <script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;

    window.addEventListener('load', onLoad);
    function onLoad(event) { initWebSocket(); }

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen    = onOpen;
      websocket.onclose   = onClose;
      websocket.onmessage = onMessage;
    }

    function onOpen(event) { console.log('WebSocket Terkoneksi'); }
    function onClose(event) { setTimeout(initWebSocket, 2000); }

    function toggleLed(){
      websocket.send('toggle');
    }

    // BARU: kirim nilai slider ke NodeMCU, format "pwm,512"
    function sendPWM(value){
      document.getElementById('pwmValue').innerHTML = value;
      if (websocket.readyState === WebSocket.OPEN) {
        websocket.send('pwm,' + value);
      }
    }

    function onMessage(event) {
      var dataObj = JSON.parse(event.data);

      if(dataObj.suhu !== undefined) {
         document.getElementById('tempValue').innerHTML = dataObj.suhu;
      }

      if(dataObj.hum !== undefined) {
         document.getElementById('humValue').innerHTML = dataObj.hum;
      }

      // BARU: sinkronkan posisi slider dengan nilai dari NodeMCU
      if(dataObj.pwm !== undefined) {
         document.getElementById('pwmSlider').value = dataObj.pwm;
         document.getElementById('pwmValue').innerHTML = dataObj.pwm;
      }

      if(dataObj.led !== undefined) {
         var btn = document.getElementById('toggleBtn');
         var status = document.getElementById('ledStatus');
         if(dataObj.led == "1"){
           status.innerHTML = "ON";
           btn.innerHTML = "Turn OFF";
           btn.className = "btn-on";
         } else {
           status.innerHTML = "OFF";
           btn.innerHTML = "Turn ON";
           btn.className = "btn-off";
         }
      }
    }
  </script>
</body>
</html>
)rawliteral";

// ---------------- BACK-END & WEBSOCKET LOGIC ----------------

void notifyClients() {
  String jsonString = "{\"led\":\"" + String(ledState ? 1 : 0) + "\", ";
  jsonString += "\"suhu\":\"" + currentTemp + "\", ";
  jsonString += "\"hum\":\"" + currentHum + "\", ";
  jsonString += "\"pwm\":\"" + String(pwmValue) + "\"}";   // BARU: kirim nilai PWM
  ws.textAll(jsonString);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {

    // Ubah data mentah menjadi String agar mudah diperiksa
    String message = "";
    for (size_t i = 0; i < len; i++) {
      message += (char)data[i];
    }

    if (message == "toggle") {
      ledState = !ledState;
      notifyClients();
    }
    // BARU: cek apakah pesan diawali "pwm,"
    else if (message.startsWith("pwm,")) {
      int nilai = message.substring(4).toInt();   // ambil angka setelah "pwm,"
      pwmValue = constrain(nilai, 0, 1023);       // batasi 0-1023
      analogWrite(pwmPin, pwmValue);              // atur kecerahan LED
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("Client WebSocket #%u terhubung\n", client->id());
      notifyClients();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("Client WebSocket #%u terputus\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(pwmPin, OUTPUT);          // BARU
  digitalWrite(ledPin, LOW);
  analogWriteRange(1023);           // BARU: rentang PWM 0-1023
  analogWrite(pwmPin, 0);           // BARU: LED dimmer mulai mati
  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nIP Address: " + WiFi.localIP().toString());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();
}

void loop() {
  ws.cleanupClients();

  // 1. LED on/off mengikuti status
  digitalWrite(ledPin, ledState ? HIGH : LOW);

  // 2. Tombol fisik (debounce)
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == HIGH) {
        ledState = !ledState;
        notifyClients();
      }
    }
  }
  lastButtonState = reading;

  // 3. Baca suhu + kelembapan tiap 3 detik
  if ((millis() - lastTime) > 3000) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t)) {
      currentTemp = String(t, 1);
    }
    if (!isnan(h)) {
      currentHum = String(h, 1);
    }
    if (!isnan(t) || !isnan(h)) {
      notifyClients();
    }
    lastTime = millis();
  }
}