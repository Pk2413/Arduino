
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <Servo.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include <WiFiUdp.h>
#include <NTPClient.h>

#define WIFI_SSID "POCO X3 Pro"
#define WIFI_PASSWORD "12345687"
#define API_KEY "AIzaSyBTLo5aqO8jXxi_kxnaxsw261WTzqD8Ny8"
#define DATABASE_URL "https://aq-farm-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define DATABASE_SECRET "JvSE2eWLysRG92vEWJ4fObEytKk9VSFkQzrDSESE"

// Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

//servo pin
#define SERVO 16

// suhu
#define SUHU 0
OneWire oneWire(SUHU);
DallasTemperature sensors(&oneWire);

//amoniak
#define AMONIAK A0

// Ultrasonic sensor pins
#define TRIG 14
#define ECHO 12

// I2C pins for LCD
#define SDA_PIN 4                    // D2
#define SCL_PIN 5                    // D1
LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD setup
Servo servoMotor;

// Inisialisasi UDP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);


void setup() {
  Serial.begin(115200);
  servoMotor.attach(SERVO);  // Attach servo to GPIO 16
  sensors.begin();

  // Initialize LCD with I2C pins
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Connecting...");

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("connecting..");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wifi ");
    lcd.setCursor(0, 1);
    lcd.print("Connected");
    delay(300);
  }
  Serial.println("\nWiFi connected!");

  // Configure Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Sign up anonymously
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Sign-up successful!");
    signupOK = true;
  } else {
    Serial.printf("Sign-up failed: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;  // Firebase token status callback
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Inisialisasi NTP Client
  timeClient.begin();
}

void loop() {
  // Update LCD display
  lcd.clear();
  timeClient.update();
  // Perbarui waktu dari server NTP

  // Dapatkan waktu dalam format HH:MM:SS
  String currentTime = timeClient.getFormattedTime();


  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0 )) {
    sendDataPrevMillis = millis();

    sensors.requestTemperatures();

    int suhu = sensors.getTempCByIndex(0);  // Replace with actual fish count data if available
    int amoniak = analogRead(AMONIAK);      // Replace with actual ammonia sensor data if available
    int distance = readDistance();

    // Send data to Firebase
    if (Firebase.setInt(fbdo, "/data/suhu", suhu)) {
      Serial.print("Temperature sent: ");
      Serial.println(suhu);
    } else {
      Serial.printf("Failed to send temperature: %s\n", fbdo.errorReason().c_str());
    }

    if (Firebase.setFloat(fbdo, "/data/ketinggianAir", distance)) {
      Serial.print("Distance sent: ");
      Serial.println(distance);
    } else {
      Serial.printf("Failed to send distance: %s\n", fbdo.errorReason().c_str());
    }

    if (Firebase.setInt(fbdo, "/data/amoniak", amoniak)) {
      Serial.print("Ammonia level sent: ");
      Serial.println(amoniak);
    } else {
      Serial.printf("Failed to send ammonia level: %s\n", fbdo.errorReason().c_str());
    }


    lcd.setCursor(0, 0);
    lcd.print(distance);
    lcd.setCursor(6, 0);
    lcd.print(suhu);
    lcd.setCursor(0, 1);
    lcd.print(amoniak);

    // unsigned long epochTime = timeClient.getEpochTime();
    // struct tm *ptm = gmtime((time_t *)&epochTime);

    // Format elemen tm ke dalam format string

    // lcd.print("/");
    // lcd.print(ptm->tm_mon + 1);  // Bulan dimulai dari 0, jadi tambahkan 1
    // lcd.print("/");
    // lcd.print(ptm->tm_year + 1900);  // Tahun dihitung sejak 1900

    // Control servo based on suhu value
    if (distance < 30) {
      servoMotor.write(0);  // Set servo to 0 degrees
      Serial.println("Servo set to 0 degrees");
    } else {
      servoMotor.write(180);  // Set servo to 180 degrees
      Serial.println("Servo set to 180 degrees");
    }
  }
  lcd.setCursor(6, 1);
  lcd.print(currentTime);
  delay(500);
}

float readDistance() {
  
  // Kedalaman kolam
  const float depthOfPool = 30.0; // cm

  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  float duration = pulseIn(ECHO, HIGH);
  float distance = (duration * 0.034) / 2;  // Hitung jarak dalam cm

  float waterHeight = depthOfPool - distance;

  // Pastikan ketinggian air tidak kurang dari 0 cm dan tidak lebih dari 20 cm
  if (waterHeight < 0) {
    waterHeight = 0; // Air habis
  } else if (waterHeight > 20) {
    waterHeight = 20; // Kolam penuh
  }

  return waterHeight; // Mengembalikan ketinggian air
}
