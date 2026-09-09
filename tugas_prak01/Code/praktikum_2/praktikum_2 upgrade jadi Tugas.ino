const int buttonPin = 4;
const int ledPin = 5;

int buttonState = 0;          
int lastButtonState = 0;      
bool ledStatus = false;      

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);
  
  
  digitalWrite(ledPin, LOW);
}

void loop() {
 
  buttonState = digitalRead(buttonPin);

  if (buttonState == HIGH && lastButtonState == LOW) {

    ledStatus = !ledStatus;
    
    // Terapkan status baru ke LED
    if (ledStatus == true) {
      digitalWrite(ledPin, HIGH);
      Serial.println("Tombol ditekan -> LED NYALA (ON)");
    } else {
      digitalWrite(ledPin, LOW);
      Serial.println("Tombol ditekan -> LED MATI (OFF)");
    }

    
    delay(200);
  }


  lastButtonState = buttonState;
}