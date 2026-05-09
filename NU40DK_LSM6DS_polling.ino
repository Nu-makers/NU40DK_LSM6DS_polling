#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_LSM6DS3.h>

// --- 핀 설정 ---
#define PIN_SENSOR_SCL   34  // P1.02
#define PIN_SENSOR_SDA   35  // P1.03
#define PIN_SENSOR_PWR   32  // P1.00
#define PIN_OLED_SCL     21  // P0.21
#define PIN_OLED_SDA     22  // P0.22

#define BTN_START        31  // P0.31
#define BTN_STOP         39  // P1.07 (32 + 7)

#define SD_CLK           27  // P0.27
#define SD_MISO          4   // P0.04
#define SD_MOSI          26  // P0.26
#define SD_CS            23  // P0.23
#define SD_DET           47  // P1.15 (32 + 15)

// --- 객체 설정 ---
TwoWire Wire1(NRF_TWIM1, NRF_TWIS1, SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQn, PIN_OLED_SDA, PIN_OLED_SCL);
Adafruit_SSD1306 display(128, 32, &Wire1, -1);
Adafruit_LSM6DS3 lsm6ds3;

File dataFile;
char fileName[20];
bool isLogging = false;
unsigned long lastSampleTime = 0;
const int sampleInterval = 16; // 1000ms / 62.5Hz = 16ms

void setup() {
  // 1. 버튼 설정 (내부 풀업)
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_STOP, INPUT_PULLUP);
  pinMode(SD_DET, INPUT_PULLUP);

  // 2. 센서 전원 및 I2C 초기화
  pinMode(PIN_SENSOR_PWR, OUTPUT);
  digitalWrite(PIN_SENSOR_PWR, HIGH);
  delay(100);
  
  Wire.setPins(PIN_SENSOR_SDA, PIN_SENSOR_SCL);
  Wire.begin();
  Wire1.begin();

  // 3. 디스플레이 초기화
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // 4. 센서 및 SD 카드 확인
  bool sensorOk = lsm6ds3.begin_I2C(0x6A, &Wire);
  
  // SPI 핀 설정 (BARAM 코어 기준)
  SPI.setPins(SD_MISO, SD_CLK, SD_MOSI);
  bool sdOk = SD.begin(SD_CS);

  if (!sensorOk || !sdOk) {
    display.setCursor(0, 0);
    if(!sensorOk) display.println("Sensor Fail!");
    if(!sdOk) display.println("SD Card Fail!");
    display.display();
    while(1);
  }

  // 5. 센서 데이터 레이트 설정 (12.5Hz, 26Hz, 52Hz, 104Hz, 208Hz)
  lsm6ds3.setAccelDataRate(LSM6DS_RATE_104_HZ);

  display.println("System Ready");
  display.display();
  delay(1000);
}

void loop() {
  sensors_event_t accel, gyro, temp;
  lsm6ds3.getEvent(&accel, &gyro, &temp);

  // --- 버튼 처리 ---
  if (digitalRead(BTN_START) == LOW && !isLogging) {
    startRecording();
  }
  
  if (digitalRead(BTN_STOP) == LOW && isLogging) {
    stopRecording();
  }

  // --- 데이터 기록 모드 ---
  if (isLogging) {
    unsigned long currentTime = millis();
    if (currentTime - lastSampleTime >= sampleInterval) {
      lastSampleTime = currentTime;
      
      // CSV 기록: 시간(ms), accX, accY, accZ
      dataFile.print(currentTime);
      dataFile.print(",");
      dataFile.print(accel.acceleration.x);
      dataFile.print(",");
      dataFile.print(accel.acceleration.y);
      dataFile.print(",");
      dataFile.println(accel.acceleration.z);
    }
  } 
  // --- 대기 모드 (폴링 상태 출력) ---
  else {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("X:"); display.print(accel.acceleration.x, 1);
    display.print(" Y:"); display.print(accel.acceleration.y, 1);
    display.print(" Z:"); display.println(accel.acceleration.z, 1);
    display.print("Temp: "); display.print(temp.temperature, 1); display.println(" C");
    display.println("Ready to Record...");
    display.display();
  }
}

void startRecording() {
  // 파일명 찾기 (record1.csv부터 시작)
  int fileNum = 1;
  while (true) {
    sprintf(fileName, "record%d.csv", fileNum);
    if (!SD.exists(fileName)) break;
    fileNum++;
  }

  dataFile = SD.open(fileName, FILE_WRITE);
  if (dataFile) {
    // CSV 헤더 작성
    dataFile.println("timestamp_ms,accX,accY,accZ");
    isLogging = true;
    lastSampleTime = millis();
    
    display.clearDisplay();
    display.setCursor(0, 10);
    display.setTextSize(1);
    display.println("Capture Motion");
    display.print("File: "); display.println(fileName);
    display.display();
    delay(500); // 버튼 디바운스 대기
  }
}

void stopRecording() {
  isLogging = false;
  dataFile.close();
  
  display.clearDisplay();
  display.setCursor(0, 10);
  display.setTextSize(1);
  display.println("Capture Complete");
  display.print("Saved: "); display.println(fileName);
  display.display();
  delay(2000); // 결과 확인을 위한 대기
}