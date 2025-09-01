#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WiFiClient.h>
// #include <SoftwareSerial.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <iomanip>

#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <cmath>
// Declare LCD object globally
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define ULTRA_SONIC_TRIG 18 
#define ULTRA_SONIC_ECHO 17 

#define SERVO_PIN 19

Servo myServo;

/* ========== Temperature Measure ========== */ 
// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 5 // Temperature sensors

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
/* ======================================== */


/*========== TDS Sensor ==========*/
// --- ค่าคงที่และการตั้งค่า ---
#define TdsSensorPin 34      // กำหนดขาเซ็นเซอร์ TDS คือ GPIO34 (ขา VP บนบอร์ด)
#define VREF 3.3             // แรงดันอ้างอิงของ ESP32 คือ 3.3V
#define SCOUNT 30            // จำนวนครั้งในการอ่านค่าเพื่อหาค่ามัธยฐาน (เพื่อความแม่นยำ)

// --- ประกาศฟังก์ชันที่จะใช้ ---
float getMedian(int arr[], int size); // ฟังก์ชันหาค่ามัธยฐาน
void readSensor(float* voltage, float* tds); // ฟังก์ชันอ่านค่าและแปลงผล

/*==============================*/

/* RT TX  */
// #define TxPin 22
// #define RxPin 23
// SoftwareSerial anotherSerial(RxPin, TxPin);

void pins_init() {
  // pinMode(WATER_SENSOR, INPUT);
  // pinMode(BUZZER, OUTPUT);
  pinMode(ULTRA_SONIC_TRIG, OUTPUT);
  pinMode(ULTRA_SONIC_ECHO, INPUT);

  pinMode(SERVO_PIN, OUTPUT);
	myServo.attach(SERVO_PIN);
  myServo.write(90);

  // pinMode(MOTION_SENSOR, INPUT);
}

void connectWifi() {

  char ssid[] = "404"; // Dami 14
  char password[] = "N0tFr33Wifi"; // BigMi1414

  /* Connecting to WIFI */
	Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password); // เริ่มต้นเชื่อมต่อ WiFi

  while (WiFi.status() != WL_CONNECTED) { // วนลูปหากยังเชื่อมต่อ WiFi ไม่สำเร็จ
      delay(500);
      Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

float readWaterTemperature() {
    sensors.requestTemperatures(); // Send the command to get temperatures
    float waterTemp = sensors.getTempCByIndex(0); // Read temperature in Celsius from first sensor
    waterTemp = roundf(waterTemp * 100) / 100.0;       // Round to 2 decimal places
    return waterTemp;
}

float mesureWaterLevel() {
	// Ultra Sonic Sensor
	digitalWrite(ULTRA_SONIC_TRIG, HIGH);
	delayMicroseconds(10);
	digitalWrite(ULTRA_SONIC_TRIG, LOW);

  const float EMPTY_BOTTLE_DEPTH = 14.5; // Bottle Depth When Empty 

	// measure duration of pulse from ECHO pin
	float duration_us = pulseIn(ULTRA_SONIC_ECHO, HIGH);
	float distance_cm = 0.017 * duration_us; // Distance from Ultrasonic to Water Surface

	Serial.println("Distance:" +  String(distance_cm) + " cm");

  float waterLevel = ( (EMPTY_BOTTLE_DEPTH - distance_cm) / EMPTY_BOTTLE_DEPTH ) * 100; // Water Level in Percent
  waterLevel = fmax(waterLevel, 100);
  waterLevel = roundf(waterLevel * 100) / 100.0;   // Round to 2 decimal places

  return waterLevel;
}

void openDoor() {
	Serial.println("Door automatically opened.");
	myServo.write(180);  // เปิดประตู
}

void closeDoor() {
	Serial.println("Door automatically closed.");
	myServo.write(0);  // ปิดประตู
}

void testServo() {
  Serial.println("Opening door...");
  openDoor();
  delay(5000);

  Serial.println("Closing door...");
  closeDoor();
}

void lcdSetup() {
  lcd.init();
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.setCursor(0, 0); // กำหนดตำแหน่งเคอร์เซอร์ที่ แถวที่ 0 บรรทัดที่ 0
  // lcd.print(""); //พิมพ์ข้อความ
  // lcd.setCursor(2, 1); // กำหนดตำแหน่งเคอร์เซอร์ที่ แถวที่ 2 บรรทัดที่ 1
}


void displayData(float temp, float waterLevel, float tds) {
  String waterQuality;
  if (tds <= 300) {
    waterQuality = "Good";
  } else if (tds <= 500) {
    waterQuality = "Fair";
  } else {
    waterQuality = "Bad";
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Lv:");
  lcd.print(int(waterLevel));
  lcd.print("% Tmp:");
  lcd.print(int(temp));
  lcd.print("C");

  String row2 = "Quality:" + waterQuality;
  lcd.setCursor(0, 1);
  lcd.print(row2);
}

/**
 * @brief อ่านค่าจากเซ็นเซอร์, แปลงเป็น Voltage และ TDS
 * @param voltage ตัวแปรสำหรับเก็บค่า Voltage ที่คำนวณได้
 * @param tds ตัวแปรสำหรับเก็บค่า TDS ที่คำนวณได้
 */
void readSensor(float* voltage, float* tds) {
  int sensorReadings[SCOUNT]; // อาเรย์สำหรับเก็บค่าที่อ่านได้ 30 ครั้ง
  
  // อ่านค่าจากเซ็นเซอร์ 30 ครั้ง
  for (int i = 0; i < SCOUNT; i++) {
    sensorReadings[i] = analogRead(TdsSensorPin);
    delay(40); // หน่วงเวลาเล็กน้อยระหว่างการอ่านแต่ละครั้ง
  }

  // หาค่ามัธยฐาน (Median) เพื่อตัดค่ารบกวน (noise) ออก
  float medianAnalog = getMedian(sensorReadings, SCOUNT);

  // แปลงค่า Analog (0-4095) เป็นแรงดันไฟฟ้า (0-3.3V)
  *voltage = medianAnalog * VREF / 4095.0;

  // --- สูตรแปลงค่า Voltage เป็น TDS (ppm) ---
  // สูตรนี้เป็นค่าประมาณทั่วไป และแม่นยำที่สุดที่อุณหภูมิ 25°C
  // หากต้องการความแม่นยำสูง ควรมีการชดเชยอุณหภูมิเพิ่มเติม
  float temperatureCompensation = 1.0 + 0.02 * (25.0 - 25.0); // สมมติว่าอุณหภูมิคือ 25°C
  float compensatedVoltage = *voltage / temperatureCompensation;

  *tds = (133.42 * pow(compensatedVoltage, 3) - 255.86 * pow(compensatedVoltage, 2) + 857.39 * compensatedVoltage) * 0.5;
}

/**
 * @brief ฟังก์ชันสำหรับหาค่ามัธยฐานจากอาเรย์ของตัวเลข
 * @param arr อาเรย์ของตัวเลข
 * @param size ขนาดของอาเรย์
 * @return ค่ามัธยฐาน (Median)
 */
float getMedian(int arr[], int size) {
  // เรียงลำดับตัวเลขในอาเรย์จากน้อยไปมาก (Bubble Sort)
  for (int i = 0; i < size - 1; i++) {
    for (int j = 0; j < size - i - 1; j++) {
      if (arr[j] > arr[j + 1]) {
        int temp = arr[j];
        arr[j] = arr[j + 1];
        arr[j + 1] = temp;
      }
    }
  }
  // คืนค่าตัวเลขที่อยู่ตรงกลาง
  return arr[size / 2];
}

void setup() {
  Serial.begin(9600);
  sensors.begin();
  pins_init();
  closeDoor();
  lcdSetup();
}

void loop() {
  // Create JSON
  StaticJsonDocument<128> sensorData;

  // Prepare Sensor Data to JSON
  float waterLevel = mesureWaterLevel();
  float waterTemp = readWaterTemperature();
  float voltageValue;
  float tdsValue;

  readSensor(&voltageValue, &tdsValue); // เรียกฟังก์ชันเพื่ออ่านค่าจากเซ็นเซอร์

  // แสดงผลลัพธ์ออกทาง Serial Monitor
  // Serial.print("Voltage: ");
  // Serial.print(voltageValue, 2); // แสดงทศนิยม 2 ตำแหน่ง
  // Serial.print("V   ");
  // Serial.print("TDS: ");
  // Serial.print(tdsValue, 0); // แสดงเป็นเลขจำนวนเต็ม
  // Serial.println(" ppm");

  // delay(1000); // หน่วงเวลา 1 วินาทีก่อนอ่านค่าครั้งต่อไป
  displayData(waterTemp, waterLevel, tdsValue);

  sensorData["temperature"] = waterTemp;
  sensorData["waterLevel"] = waterLevel;
  sensorData["voltage"] = voltageValue;
  sensorData["tdsValue"] = tdsValue;

  // Send JSON over serial
  serializeJson(sensorData, Serial);
  Serial.println();

  // Example of opening and closing the door every 5 seconds
  // testServo();
  delay(1000);
}
