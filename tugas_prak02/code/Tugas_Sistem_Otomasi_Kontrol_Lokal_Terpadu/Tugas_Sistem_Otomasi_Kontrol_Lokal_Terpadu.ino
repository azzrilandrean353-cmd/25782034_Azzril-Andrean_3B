#include "DHT.h"

#define DHTPIN 4          
#define DHTTYPE DHT22      
#define LDRPIN 32          
#define RELAYPIN 21        

const float AMBANG_SUHU = 30.0;    
const int AMBANG_CAHAYA = 2000;    

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  
  
  dht.begin();
  
  
  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN, LOW); 
  
  Serial.println("Sistem Rule Engine Aktuator Dimulai...");
}

void loop() {
  
  float suhu = dht.readTemperature();
  float kelembapan = dht.readHumidity();
  int nilaiLDR = analogRead(LDRPIN);

  
  if (isnan(suhu) || isnan(kelembapan)) {
    Serial.println("Gagal membaca data dari sensor DHT22!");
    delay(2000);
    return;
  }

  
  Serial.println("==================================");
  Serial.print("Suhu: ");
  Serial.print(suhu);
  Serial.print(" °C | Kelembapan: ");
  Serial.print(kelembapan);
  Serial.println(" %");
  
  Serial.print("Nilai Cahaya (LDR): ");
  Serial.println(nilaiLDR);

  
  if (suhu > AMBANG_SUHU || nilaiLDR < AMBANG_CAHAYA) {
    digitalWrite(RELAYPIN, HIGH); 
    Serial.println("Status Relay: AKTIF (ON)");
  } else {
    digitalWrite(RELAYPIN, LOW);  // Matikan Relay
    Serial.println("Status Relay: NONAKTIF (OFF)");
  }

  delay(2000); 
}