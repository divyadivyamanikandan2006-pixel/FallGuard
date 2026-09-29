#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

#define BUZZER_PIN 4
#define LED_PIN 5
#define BUTTON_PIN 14

// You can tune this value after testing
const float FALL_THRESHOLD = 2.5;   // g
const unsigned long INACTIVITY_TIME = 1500; // ms

unsigned long fallStartTime = 0;
bool possibleFall = false;
bool fallDetected = false;

void setup() {
Serial.begin(115200);

Wire.begin(21, 22);

pinMode(BUZZER_PIN, OUTPUT);
pinMode(LED_PIN, OUTPUT);
pinMode(BUTTON_PIN, INPUT_PULLUP);

digitalWrite(BUZZER_PIN, LOW);
digitalWrite(LED_PIN, LOW);

if (!mpu.begin()) {
Serial.println("MPU6050 NOT FOUND!");
while (1);
}

Serial.println("MPU6050 Connected");
Serial.println("Smart Fall Detection Ready");
}

void loop() {

// Reset alarm using push button
if (digitalRead(BUTTON_PIN) == LOW) {
fallDetected = false;
possibleFall = false;

digitalWrite(BUZZER_PIN, LOW);  
digitalWrite(LED_PIN, LOW);  

Serial.println("Alarm Reset");  
delay(500);

}

sensors_event_t a, g, temp;
mpu.getEvent(&a, &g, &temp);

// Acceleration magnitude
float acceleration = sqrt(
a.acceleration.x * a.acceleration.x +
a.acceleration.y * a.acceleration.y +
a.acceleration.z * a.acceleration.z
);

// Convert m/s² to g
float accelerationG = acceleration / 9.81;

Serial.print("Acceleration: ");
Serial.print(accelerationG);
Serial.println(" g");

// Detect sudden movement
if (accelerationG > FALL_THRESHOLD || accelerationG < 0.5) {

if (!possibleFall) {  
  possibleFall = true;  
  fallStartTime = millis();  

  Serial.println("Possible Fall Detected!");  
}

}

// Confirm fall if inactivity continues
if (possibleFall &&
millis() - fallStartTime > INACTIVITY_TIME) {

fallDetected = true;  
possibleFall = false;  

Serial.println("***** FALL DETECTED *****");

}

// Alarm
if (fallDetected) {

digitalWrite(LED_PIN, HIGH);  

tone(BUZZER_PIN, 1000);

} else {

digitalWrite(LED_PIN, LOW);  

noTone(BUZZER_PIN);

}

delay(100);
}
