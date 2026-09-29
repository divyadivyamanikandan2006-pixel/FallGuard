#define BLYNK_TEMPLATE_ID "TMPL32LS2mIZT"
#define BLYNK_TEMPLATE_NAME "Fall Alert System"
#define BLYNK_AUTH_TOKEN "MFAzatT64uHP3ec2akC5ta_4GF1Ox8aW"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <Wire.h>
#include <BlynkSimpleEsp32.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

char ssid[] = "redmi";
char pass[] = "1234567890";

#define SDA_PIN 21
#define SCL_PIN 22
#define BUZZER_PIN 25
#define RESET_BUTTON 27

#define MPU_ADDR 0x68
#define LCD_ADDR 0x27

#define PWR_MGMT_1 0x6B
#define ACCEL_CONFIG 0x1C
#define ACCEL_XOUT_H 0x3B

#define IMPACT_THRESHOLD 2.5
#define ORIENTATION_THRESHOLD 60.0
#define CONFIRM_TIME 3000
#define BUZZER_TIME 5000
#define ALERT_COOLDOWN 15000
#define DEBOUNCE_TIME 200

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
BlynkTimer timer;

float ax, ay, az;
float totalAcceleration, pitch, roll;

bool possibleFall = false;
bool fallConfirmed = false;

unsigned long possibleFallTime = 0;
unsigned long fallStartTime = 0;
unsigned long lastAlertTime = -ALERT_COOLDOWN;
unsigned long lastButtonTime = 0;

bool lastButtonState = HIGH;

void writeRegister(byte reg, byte value)
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

int16_t read16()
{
  int16_t value = Wire.read() << 8;
  value |= Wire.read();
  return value;
}

bool readMPU()
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(ACCEL_XOUT_H);

  if (Wire.endTransmission(false) != 0)
    return false;

  if (Wire.requestFrom(MPU_ADDR, 6) != 6)
    return false;

  int16_t rawX = read16();
  int16_t rawY = read16();
  int16_t rawZ = read16();

  ax = rawX / 16384.0;
  ay = rawY / 16384.0;
  az = rawZ / 16384.0;

  return true;
}

void calculateMotion()
{
  totalAcceleration = sqrt(ax * ax + ay * ay + az * az);

  pitch = atan2(ax, sqrt(ay * ay + az * az))
           * 180.0 / PI;

  roll = atan2(ay, az) * 180.0 / PI;
}

void safeLCD()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("STATUS: SAFE");
  lcd.setCursor(0, 1);
  lcd.print("Monitoring...");
}

void checkingLCD()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("POSSIBLE FALL!");
  lcd.setCursor(0, 1);
  lcd.print("Checking...");
}

void fallLCD()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("!! FALL ALERT !!");
  lcd.setCursor(0, 1);
  lcd.print("PRESS RESET");
}

void sensorLCD()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ACC:");
  lcd.print(totalAcceleration, 2);
  lcd.print("G");

  lcd.setCursor(0, 1);
  lcd.print("P:");
  lcd.print(pitch, 0);
  lcd.print(" R:");
  lcd.print(roll, 0);
}

void resetFall()
{
  possibleFall = false;
  fallConfirmed = false;
  possibleFallTime = 0;
  fallStartTime = 0;

  digitalWrite(BUZZER_PIN, LOW);

  Blynk.virtualWrite(V3, "SAFE");
  Blynk.virtualWrite(V4, 0);

  lastAlertTime = -ALERT_COOLDOWN;

  safeLCD();

  Serial.println("System Reset - SAFE");
}

void checkButton()
{
  bool state = digitalRead(RESET_BUTTON);

  if (state == LOW && lastButtonState == HIGH)
  {
    if (millis() - lastButtonTime > DEBOUNCE_TIME)
    {
      lastButtonTime = millis();
      resetFall();
    }
  }

  lastButtonState = state;
}

void sendData()
{
  if (!readMPU())
  {
    Serial.println("MPU READ ERROR");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MPU ERROR!");
    lcd.setCursor(0, 1);
    lcd.print("Check Sensor");
    return;
  }

  calculateMotion();

  Blynk.virtualWrite(V0, totalAcceleration);
  Blynk.virtualWrite(V1, pitch);
  Blynk.virtualWrite(V2, roll);

  Serial.print("ACC: ");
  Serial.print(totalAcceleration, 2);
  Serial.print("G | Pitch: ");
  Serial.print(pitch, 1);
  Serial.print(" | Roll: ");
  Serial.println(roll, 1);

  bool impact = totalAcceleration >= IMPACT_THRESHOLD;

  bool orientation =
    abs(pitch) >= ORIENTATION_THRESHOLD ||
    abs(roll) >= ORIENTATION_THRESHOLD;

  if (!fallConfirmed && impact && orientation && !possibleFall)
  {
    possibleFall = true;
    possibleFallTime = millis();

    Serial.println("Possible Fall Detected");

    checkingLCD();
    Blynk.virtualWrite(V3, "CHECKING");
  }

  if (possibleFall && !fallConfirmed)
  {
    if (millis() - possibleFallTime >= CONFIRM_TIME)
    {
      fallConfirmed = true;
      fallStartTime = millis();

      Serial.println("FALL CONFIRMED");

      digitalWrite(BUZZER_PIN, HIGH);
      fallLCD();

      Blynk.virtualWrite(V3, "FALL DETECTED");
      Blynk.virtualWrite(V4, 1);

      if (millis() - lastAlertTime >= ALERT_COOLDOWN)
      {
        Blynk.logEvent(
          "fall_detected",
          "FALL DETECTED! Please check the person."
        );

        lastAlertTime = millis();
        Serial.println("Blynk Alert Sent");
      }
    }
  }

  if (possibleFall && !fallConfirmed)
  {
    if (millis() - possibleFallTime > 5000)
    {
      possibleFall = false;

      Blynk.virtualWrite(V3, "SAFE");
      Blynk.virtualWrite(V4, 0);

      safeLCD();

      Serial.println("False Alarm - SAFE");
    }
  }

  if (fallConfirmed)
  {
    if (millis() - fallStartTime >= BUZZER_TIME)
      digitalWrite(BUZZER_PIN, LOW);
  }

  if (!possibleFall && !fallConfirmed)
    sensorLCD();
}

void setup()
{
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(RESET_BUTTON, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);

  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("FALL ALERT");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(1500);

  Wire.beginTransmission(MPU_ADDR);
  byte error = Wire.endTransmission();

  if (error != 0)
  {
    lcd.clear();
    lcd.print("MPU NOT FOUND!");
    Serial.println("MPU NOT FOUND");

    while (1)
      delay(1000);
  }

  writeRegister(PWR_MGMT_1, 0x00);
  delay(100);

  writeRegister(ACCEL_CONFIG, 0x00);
  delay(100);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  Blynk.virtualWrite(V3, "SAFE");
  Blynk.virtualWrite(V4, 0);

  safeLCD();

  timer.setInterval(200L, sendData);

  Serial.println("FALL ALERT SYSTEM READY");
}

void loop()
{
  Blynk.run();
  timer.run();
  checkButton();
}
