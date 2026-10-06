#include <painlessMesh.h>
#include <DHT.h>
#include <ArduinoJson.h>

#define MESH_PREFIX   "LabIoTMesh"
#define MESH_PASSWORD "iotmeshpassword"
#define MESH_PORT     5555

#define DHTPIN 2       // Pin D4 (GPIO 2)
#define DHTTYPE DHT22  // Sesuaikan jika pakai DHT22

DHT dht(DHTPIN, DHTTYPE);

Scheduler userScheduler;
painlessMesh mesh;

void sendMessage();
Task taskSendMessage(TASK_SECOND * 3, TASK_FOREVER, &sendMessage);

void sendMessage() {
  float suhu = dht.readTemperature();
  float kelembapan = dht.readHumidity();

  if (isnan(suhu) || isnan(kelembapan)) {
    Serial.println("[Node 1] Gagal baca DHT!");
    return;
  }

  // Format JSON sesuai permintaan tugas
  StaticJsonDocument<200> doc;
  doc["tipe"] = "suhu_node";
  doc["suhu"] = suhu;
  doc["kelembapan"] = kelembapan;

  String msg;
  serializeJson(doc, msg);
  mesh.sendBroadcast(msg);
  
  Serial.print("[Node 1 KIRIM] ");
  Serial.println(msg);
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  
  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
}

void loop() {
  mesh.update();
}