
#include <SoftwareSerial.h>
#define sensorDigital A0
#define relay 9
#define buzzer 8
#define sensorAnalog A1
SoftwareSerial sim(3,2);      //rx,tx

void setup() {
    pinMode(sensorDigital, INPUT);
    pinMode(relay, OUTPUT);
    pinMode(buzzer, OUTPUT);
    sim.begin(9600);
    Serial.begin(9600);

    Serial.println("Initializing...");
    delay(1000);
    sim.println("AT");
    updateserial();
    sim.println("AT+CMGF=1");
    updateserial();
    sim.println("AT+CNMI=1,2,0,0,0");
    updateserial();
        }

void loop(){
      updateserial();
}

        
void updateserial() {
     bool digital = digitalRead(sensorDigital);
     int analog = analogRead(sensorAnalog);
        Serial.print("Analog value : ");
        Serial.print(analog);
        Serial.print("t");
        Serial.print("Digital value :");
        Serial.println(digital);
              
    if (digital == 0) {
      digitalWrite(relay, LOW);
      digitalWrite(buzzer, HIGH);

        sendSMS();
        delay(5000);
        digitalWrite(relay, HIGH);
        digitalWrite(buzzer, LOW);
            } 
    else {
        digitalWrite(relay, HIGH);
        digitalWrite(buzzer, LOW);
       }
}

void sendSMS(){
      sim.println("AT+CMGF=1");
      delay(1000);
      sim.println("AT+CMGS=\"+9128416007\"\r");
      delay(1000);
      sim.println("Gas is leakages Please Alert!");
      delay(1000);
      sim.write(26);
      delay(1000);
}
