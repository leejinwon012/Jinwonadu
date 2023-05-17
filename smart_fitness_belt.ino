// 자이로센서 라이브러리 -----------------------------------------------------------------------------
#include <Wire.h>
#include <I2Cdev.h>
#include <MPU6050.h>

// OLED 라이브러리 ---------------------------------------------------------------------------------
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// 자이로센서 변수 및 객체 선언 -----------------------------------------------------------------------
MPU6050 accelgyro; // 자이로센서 객체 생성

int16_t AcX, AcY, AcZ, GyX, GyY, GyZ;

const int MPU_ADDR = 0x68; // 자이로센서 주소
const double RADIAN_TO_DEGREE = 180 / 3.14159; // 라디안값 도로 바꾸기

double roll; // x축 기준 회전값
double pitch; // y축 기준 회전 값

// OLED 변수 및 객체 선언 --------------------------------------------------------------------------
const int SCREEN_WIDTH = 128; // OLED 가로 pixel
const int SCREEN_HEIGHT = 64; // OLED 세로 pixel
const int OLED_RESET = 4; // Reset pin # (or -1 if sharing Arduino reset pin)
const int SCREEN_ADDRESS = 0x3C; // OLED 주소

Adafruit_SSD1306 OLED(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // OLED 객체 선언

int cs = 0;

// 핀 번호 설정 -----------------------------------------------------------------------------------
int buzzerPin = 2;
int btnEnterPin = 11;
int btnNextPin = 12;

// 변수 및 상수 설정 -------------------------------------------------------------------------------
int state = 0; // 상태변수 (0: 시작 대기 상태, 1: Days 선택 상태, 2: 운동 중 상태, 3: 완료 상태)
int num = 0; // 날짜 계산 도와주는 변수
int Days = 1; // 운동한 날짜 (1일차 ~ 10일차)

int complete[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // 완료 횟수

int count[2] = {0, 0}; // 운동 개수
int numTarget[10][2] = {{10, 10}, // 1일차 목표 개수 (버피, 팔굽혀펴기)
                 {10, 20}, // 2일차 목표 개수 (데드리프트, 플랭크)
                 {15, 10}, // 3일차 목표 개수 (팔굽혀펴기, 스쿼트)
                 {15, 30}, // 4일차 목표 개수 (버피, 플랭크)
                 {15, 15}, // 5일차 목표 개수 (데드리프트, 스쿼트)
                 {20, 20}, // 6일차 목표 개수 (버피, 스쿼트)
                 {20, 20}, // 7일차 목표 개수 (데드리프트, 팔굽혀펴기)
                 {25, 25}, // 8일차 목표 개수 (버피, 데드리프트)
                 {25, 40}, // 9일차 목표 개수 (팔굽혀펴기, 플랭크)
                 {25, 50}}; // 10일차 목표 개수 (스쿼트, 플랭크)

int exerciseState = 0; // 운동 상태 (0: 운동 준비 상태, 1: 운동1 시작, 2: 운동2 시작)

// setup() 함수 ----------------------------------------------------------------------------------
void setup() {
  // 시리얼 모니터 시작
  Serial.begin(115200);

  // 자이로센서 setup ------------------------------------------
  Wire.begin();

  Serial.println("Initializing I2C devices...");
  accelgyro.initialize();

  Serial.println("Testing device connections...");
  Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");

  // OLED setup ----------------------------------------------
  // SSD1306 초기화 확인
  if (!OLED.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  OLED.cp437(true); // 코드 cp437로 설정
  OLED.setTextSize(1); // 글자 크기
  OLED.setTextColor(SSD1306_WHITE); // 글자 색깔

  // 화면 전체 가로 pixel : 128
  // 한 글자당 가로 pixel : 5, 10, 15 (TextSize 1, 2, 3)
  // 글자와 글자 사이 pixel : 1, 2, 3 (TextSize 1, 2, 3)
  // ex) BPLAB : 12 12 12 12 10 = 58 pixel

  // 핀 모드 설정 ----------------------------------------------
  pinMode(btnEnterPin, INPUT_PULLUP);
  pinMode(btnNextPin, INPUT_PULLUP);

  // 프로그램 시작 ---------------------------------------------
  state = 0;
  exerciseState = 0;
  printStart();
}

void loop() {
  if (state == 0) { // 시작 대기 상태
    if (digitalRead(btnEnterPin) == 0 || digitalRead(btnNextPin) == 0) {
      tone(buzzerPin, 600, 50);
      delay(80);
      
      OLED.clearDisplay();
      OLED.display();
      
      selectDays(1);
      num++;

      state = 1;
      delay(500);
    }
  }

  if (state == 1) { // Days 선택 상태
    if (digitalRead(btnEnterPin) == 0) {
      tone(buzzerPin, 600, 50);
      delay(80);
      
      OLED.clearDisplay();
      OLED.display();
      
      state = 2;
    }
    else if (digitalRead(btnNextPin) == 0) {
      tone(buzzerPin, 500, 50);
      delay(80);
      
      num++;
      Days = (num-1)%10 + 1;
      selectDays(Days);
    }
  }

  if (state == 2) { // 운동 중 상태
    exercise(Days);
  }

  if (state == 3) { // 완료 상태
    OLED.clearDisplay();
    
    cs = centerAlignment(2, 8);
    OLED.setCursor(cs, 24);
    OLED.setTextSize(2);
    OLED.println(F("Complete!"));
    OLED.display();

    tone(buzzerPin, 500, 40);
    delay(60);
    tone(buzzerPin, 600, 50);
    delay(80);

    delay(4000);

    state = 1;
    selectDays(Days);
  }
}

// OLED 글자 중앙 정렬 함수
int centerAlignment(int fontsize, int amountChar) {
  int a = 6 * fontsize * amountChar - fontsize;
  int b = (128 - a)/2;
  return b;
}

// 처음 화면 출력 함수
void printStart() {
  OLED.clearDisplay();

  cs = centerAlignment(1, 5);
  OLED.setCursor(cs, 0);
  OLED.println(F("BPLAB"));
  OLED.display();
  delay(700);

  cs = centerAlignment(1, 13);
  OLED.setCursor(cs, 16);
  OLED.println(F("Smart Fitness"));
  OLED.display();
  delay(700);

  cs = centerAlignment(1, 7);
  OLED.setCursor(cs, 24);
  OLED.println(F("Trainer"));
  OLED.display();
  delay(700);

  OLED.println();
  OLED.println();
  OLED.println(F("Press any button"));
  OLED.setCursor(80, 56);
  OLED.println(F("to start"));
  OLED.display();
  delay(1000);

  tone(buzzerPin, 500, 40);
  delay(60);
  tone(buzzerPin, 600, 50);
  delay(80);
}

// 날짜 선택 함수
void selectDays(int d) {
  OLED.clearDisplay();

  cs = centerAlignment(1, 7);
  OLED.setCursor(cs, 0);
  OLED.setTextSize(1);
  OLED.print(F("<Day "));
  OLED.print(d);
  OLED.print(F(">"));

  if (d == 1) { // 1일차
    cs = centerAlignment(2, 6);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Burpee")); // 버피

    cs = centerAlignment(2, 7);
    OLED.setCursor(cs, 30);
    OLED.println(F("Push-up")); // 팔굽혀펴기
  }
  else if (d == 2) { // 2일차
    cs = centerAlignment(2, 8);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Deadlift")); // 데드리프트

    cs = centerAlignment(2, 5);
    OLED.setCursor(cs, 30);
    OLED.println(F("Plank")); // 플랭크
  }
  else if (d == 3) { // 3일차
    cs = centerAlignment(2, 7);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Push-up")); // 팔굽혀펴기

    cs = centerAlignment(2, 5);
    OLED.setCursor(cs, 30);
    OLED.println(F("Squat")); // 스쿼트
  }
  else if (d == 4) { // 4일차
    cs = centerAlignment(2, 6);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Burpee")); // 버피

    cs = centerAlignment(2, 5);
    OLED.setCursor(cs, 30);
    OLED.println(F("Plank")); // 플랭크
  }
  else if (d == 5) { // 5일차
    cs = centerAlignment(2, 8);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Deadlift")); // 데드리프트

    cs = centerAlignment(2, 5);
    OLED.setCursor(cs, 30);
    OLED.println(F("Squat")); // 스쿼트
  }
  else if (d == 6) { // 6일차
    cs = centerAlignment(2, 6);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Burpee")); // 버피

    cs = centerAlignment(2, 5);
    OLED.setCursor(cs, 30);
    OLED.println(F("Squat")); // 스쿼트
  }
  else if (d == 7) { // 7일차
    cs = centerAlignment(2, 8);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Deadlift")); // 데드리프트

    cs = centerAlignment(2, 7);
    OLED.setCursor(cs, 30);
    OLED.println(F("Push-up")); // 팔굽혀펴기
  }
  else if (d == 8) { // 8일차
    cs = centerAlignment(2, 6);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Burpee")); // 버피

    cs = centerAlignment(2, 8);
    OLED.setCursor(cs, 30);
    OLED.println(F("Deadlift")); // 데드리프트
  }
  else if (d == 9) { // 9일차
    cs = centerAlignment(2, 7);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Push-up")); // 팔굽혀펴기

    cs = centerAlignment(2, 5);
    OLED.setCursor(cs, 30);
    OLED.println(F("Plank")); // 플랭크
  }
  else if (d == 10) { // 10일차
    cs = centerAlignment(2, 5);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Squat")); // 스쿼트

    cs = centerAlignment(2, 5);
    OLED.setCursor(cs, 30);
    OLED.println(F("Plank")); // 플랭크
  }

  cs = centerAlignment(1, 12);
  OLED.setCursor(cs, 56);
  OLED.setTextSize(1);
  OLED.print(F("complete : "));
  OLED.print(complete[d-1]);

  OLED.display();

  delay(500);
}

// 운동 실행 함수
void exercise(int d) {
  if (exerciseState == 0) { // 운동 준비 상태
    count[0] = 0;
    count[1] = 0;
    exerciseState = 1;
  }
  
  // 화면 출력
  OLED.clearDisplay();

  cs = centerAlignment(1, 7);
  OLED.setCursor(cs, 0);
  OLED.setTextSize(1);
  OLED.print(F("<Day "));
  OLED.print(d);
  OLED.print(F(">"));

  OLED.setCursor(44, 31);
  OLED.setTextSize(1);
  OLED.print(count[0]); // 운동1 개수

  OLED.setCursor(44, 57);
  OLED.print(count[1]); // 운동2 개수

  OLED.setCursor(56, 31);
  OLED.print(" / ");
  OLED.print(numTarget[d-1][0]); // 운동1 목표 개수

  OLED.setCursor(56, 57);
  OLED.print(" / ");
  OLED.print(numTarget[d-1][1]); // 운동2 목표 개수
  
  if (d == 1) { // 1일차
    cs = centerAlignment(2, 6);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Burpee")); // 버피

    cs = centerAlignment(2, 7);
    OLED.setCursor(cs, 40);
    OLED.println(F("Push-up")); // 팔굽혀펴기

    OLED.display();

    if (exerciseState == 1) { // 운동1(버피) 시작
      count[0] += burpee();
      if (count[0] == numTarget[d-1][0]) {
        exerciseState = 2;
        delay(1000);
      }
    }
    else if (exerciseState == 2) { // 운동2(팔굽혀펴기) 시작
      count[1] += pushup();
      if (count[1] == numTarget[d-1][1]) {
        complete[d-1]++;
        exerciseState = 0;
        state = 3;
      }
    }
  }
  else if (d == 2) { // 2일차
    cs = centerAlignment(2, 8);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Deadlift")); // 데드리프트

    cs = centerAlignment(2, 5);
    OLED.setCursor(cs, 40);
    OLED.println(F("Plank")); // 플랭크

    OLED.display();

    if (exerciseState == 1) { // 운동1(데드리프트) 시작
      count[0] += deadlift();
      if (count[0] == numTarget[d-1][0]) {
        exerciseState = 2;
        delay(1000);
      }
    }
    else if (exerciseState == 2) { // 운동2(플랭크) 시작
      count[1] += plank();
      if (count[1] == numTarget[d-1][1]) {
        complete[d-1]++;
        exerciseState = 0;
        state = 3;
      }
    }
  }
  else if (d == 3) { // 3일차
    cs = centerAlignment(2, 7);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Push-up")); // 팔굽혀펴기

    cs = centerAlignment(2, 5);
    OLED.setCursor(cs, 40);
    OLED.println(F("Squat")); // 스쿼트

    OLED.display();

    if (exerciseState == 1) { // 운동1(팔굽혀펴기) 시작
      count[0] += pushup();
      if (count[0] == numTarget[d-1][0]) {
        exerciseState = 2;
        delay(1000);
      }
    }
    else if (exerciseState == 2) { // 운동2(스쿼트) 시작
      count[1] += squat();
      if (count[1] == numTarget[d-1][1]) {
        complete[d-1]++;
        exerciseState = 0;
        state = 3;
      }
    }
  }
  else if (d == 4) { // 4일차
    cs = centerAlignment(2, 6);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Burpee")); // 버피

    cs = centerAlignment(2, 5);
    OLED.setCursor(cs, 40);
    OLED.println(F("Plank")); // 플랭크

    OLED.display();

    if (exerciseState == 1) { // 운동1(버피) 시작
      count[0] += burpee();
      if (count[0] == numTarget[d-1][0]) {
        exerciseState = 2;
        delay(1000);
      }
    }
    else if (exerciseState == 2) { // 운동2(플랭크) 시작
      count[1] += plank();
      if (count[1] == numTarget[d-1][1]) {
        complete[d-1]++;
        exerciseState = 0;
        state = 3;
      }
    }
  }
  else if (d == 5) { // 5일차
    cs = centerAlignment(2, 8);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Deadlift")); // 데드리프트

    cs = centerAlignment(2, 5);
    OLED.setCursor(cs, 40);
    OLED.println(F("Squat")); // 스쿼트

    OLED.display();

    if (exerciseState == 1) { // 운동1(데드리프트) 시작
      count[0] += deadlift();
      if (count[0] == numTarget[d-1][0]) {
        exerciseState = 2;
        delay(1000);
      }
    }
    else if (exerciseState == 2) { // 운동2(스쿼트) 시작
      count[1] += squat();
      if (count[1] == numTarget[d-1][1]) {
        complete[d-1]++;
        exerciseState = 0;
        state = 3;
      }
    }
  }
  else if (d == 6) { // 6일차
    cs = centerAlignment(2, 6);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Burpee")); // 버피

    cs = centerAlignment(2, 5);
    OLED.setCursor(cs, 40);
    OLED.println(F("Squat")); // 스쿼트

    OLED.display();

    if (exerciseState == 1) { // 운동1(버피) 시작
      count[0] += burpee();
      if (count[0] == numTarget[d-1][0]) {
        exerciseState = 2;
        delay(1000);
      }
    }
    else if (exerciseState == 2) { // 운동2(스쿼트) 시작
      count[1] += squat();
      if (count[1] == numTarget[d-1][1]) {
        complete[d-1]++;
        exerciseState = 0;
        state = 3;
      }
    }
  }
  else if (d == 7) { // 7일차
    cs = centerAlignment(2, 8);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Deadlift")); // 데드리프트

    cs = centerAlignment(2, 7);
    OLED.setCursor(cs, 40);
    OLED.println(F("Push-up")); // 팔굽혀펴기

    OLED.display();

    if (exerciseState == 1) { // 운동1(데드리프트) 시작
      count[0] += deadlift();
      if (count[0] == numTarget[d-1][0]) {
        exerciseState = 2;
        delay(1000);
      }
    }
    else if (exerciseState == 2) { // 운동2(팔굽혀펴기) 시작
      count[1] += pushup();
      if (count[1] == numTarget[d-1][1]) {
        complete[d-1]++;
        exerciseState = 0;
        state = 3;
      }
    }
  }
  else if (d == 8) { // 8일차
    cs = centerAlignment(2, 6);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Burpee")); // 버피

    cs = centerAlignment(2, 8);
    OLED.setCursor(cs, 40);
    OLED.println("Deadlift"); // 데드리프트

    OLED.display();

    if (exerciseState == 1) { // 운동1(버피) 시작
      count[0] += burpee();
      if (count[0] == numTarget[d-1][0]) {
        exerciseState = 2;
        delay(1000);
      }
    }
    else if (exerciseState == 2) { // 운동2(데드리프트) 시작
      count[1] += deadlift();
      if (count[1] == numTarget[d-1][1]) {
        complete[d-1]++;
        exerciseState = 0;
        state = 3;
      }
    }
  }
  else if (d == 9) { // 9일차
    cs = centerAlignment(2, 7);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Push-up")); // 팔굽혀펴기

    cs = centerAlignment(2, 5);
    OLED.setCursor(cs, 40);
    OLED.println(F("Plank")); // 플랭크

    OLED.display();

    if (exerciseState == 1) { // 운동1(팔굽혀펴기) 시작
      count[0] += pushup();
      if (count[0] == numTarget[d-1][0]) {
        exerciseState = 2;
        delay(1000);
      }
    }
    else if (exerciseState == 2) { // 운동2(플랭크) 시작
      count[1] += plank();
      if (count[1] == numTarget[d-1][1]) {
        complete[d-1]++;
        exerciseState = 0;
        state = 3;
      }
    }
  }
  else if (d == 10) { // 10일차
    cs = centerAlignment(2, 5);
    OLED.setCursor(cs, 14);
    OLED.setTextSize(2);
    OLED.println(F("Squat")); // 스쿼트

    cs = centerAlignment(2, 8);
    OLED.setCursor(cs, 40);
    OLED.println(F("Deadlift")); // 데드리프트

    OLED.display();

    if (exerciseState == 1) { // 운동1(스쿼트) 시작
      count[0] += squat();
      if (count[0] == numTarget[d-1][0]) {
        exerciseState = 2;
        delay(1000);
      }
    }
    else if (exerciseState == 2) { // 운동2(데드리프트) 시작
      count[1] += deadlift();
      if (count[1] == numTarget[d-1][1]) {
        complete[d-1]++;
        exerciseState = 0;
        state = 3;
      }
    }
  }

  OLED.display();
  delay(50);
}

// 버피 측정 함수
int burpee() {
  OLED.display();

  while (true) {
    gyroGet(&roll, &pitch);
    Serial.print(F("Pitch : "));
    Serial.println(pitch);

    // 이상한 자세 감지
    if (pitch > -30 || pitch < -90) { // 이상한 자세 범위 설정
      tone(buzzerPin, 500, 40);
      delay(60);
      tone(buzzerPin, 500, 40); // 부저를 2번 울림
      delay(60);
      while (true) {
        gyroGet(&roll, &pitch);
        if (pitch > -90 && pitch < -80) { // 정상 자세로 돌아온 경우 범위 설정
          tone(buzzerPin, 600, 50); // 부저를 1번 울림
          delay(80);
          return 1;
        }
      }
    }
    delay(100);
  }
}

// 팔굽혀펴기 측정 함수
int pushup() {
  OLED.display();

  while (true) {
    gyroGet(&roll, &pitch);
    Serial.print(F("Pitch : "));
    Serial.println(pitch);

    // 이상한 자세 감지
    if (pitch > 3 || pitch < -3) { // 이상한 자세 범위 설정
      tone(buzzerPin, 500, 40);
      delay(60);
      tone(buzzerPin, 500, 40);
      delay(60);                    // 부저를 2번 울림
      while (true) {
        gyroGet(&roll, &pitch);
        if (pitch < 0.5 && pitch > -0.5) { // 정상 자세로 돌아온 경우 범위 설정
          tone(buzzerPin, 600, 50);       // 부저를 1번 울림
          delay(80);
          return 1;
        }
      }
    }
    delay(100);
  }
}

// 데드리프트 측정 함수
int deadlift() {
  OLED.display();

  while (true) {
    gyroGet(&roll, &pitch);
    Serial.print(F("Pitch : "));
    Serial.println(pitch);

    // 이상한 자세 감지
    if (pitch > -45 || pitch < -90) { // 이상한 자세 범위 설정
      tone(buzzerPin, 500, 40);
      delay(60);
      tone(buzzerPin, 500, 40);       // 부저를 2번 울림
      delay(60);
      while (true) {
        gyroGet(&roll, &pitch);
        if (pitch > -90 && pitch < -80) { // 정상 자세로 돌아온 경우 범위 설정
          tone(buzzerPin, 600, 50);       //부저를 1번 울림
          delay(80);
          return 1;
        }
      }
    }
    delay(100);
  }
}

// 플랭크 측정 함수
int plank() {
  OLED.display();

  while (true) {
    gyroGet(&roll, &pitch);
    Serial.print(F("roll : "));
    Serial.println(roll);

    // 이상한 자세 감지
    if (abs(roll) > 40 || abs(roll) < 0) { // 이상한 자세 범위 설정
      tone(buzzerPin, 500, 40);
      delay(60);
      tone(buzzerPin, 500, 40);           // 부저를 2번 울림
      delay(60);
      while (true) {
        gyroGet(&roll, &pitch);
        if (abs(roll) < 5) { // 정상 자세로 돌아온 경우 범위 설정
          tone(buzzerPin, 600, 50);       // 부저를 1번 울림
          delay(80);
          return 1;
        }
      }
    }
    delay(100);
  }
}

// 스쿼트 측정 함수
int squat() {
  OLED.display();

  while (true) {
    gyroGet(&roll, &pitch);
    Serial.print(F("Pitch : "));
    Serial.println(pitch);

    // 이상한 자세 감지
    if (pitch > -45 || pitch < -90) { // 이상한 자세 범위 설정
      tone(buzzerPin, 500, 40);
      delay(60);
      tone(buzzerPin, 500, 40);       // 부저를 2번 울림
      delay(60);
      while (true) {
        gyroGet(&roll, &pitch);
        if (pitch > -90 && pitch < -80) { // 정상 자세로 돌아온 경우 범위 설정
          tone(buzzerPin, 600, 50);     // 부저를 1번 울림
          delay(80);
          return 1;
        }
      }
    }
    delay(100);
  }
}

// 자이로 값 측정 함수
void gyroGet(double* ptrRoll, double* ptrPitch) {
  // 자이로 값 측정
  accelgyro.getMotion6(&AcX, &AcY, &AcZ, &GyX, &GyY, &GyZ);

  // 가속도센서 값 이용해서 roll, pitch 계산
  double angleAcX = atan(AcY / sqrt(pow(AcX, 2) + pow(AcZ, 2)));
  angleAcX *= RADIAN_TO_DEGREE;
  double angleAcY = atan(-AcX / sqrt(pow(AcY, 2) + pow(AcZ, 2)));
  angleAcY *= RADIAN_TO_DEGREE;

  // 출력
  *ptrRoll = angleAcX;
  *ptrPitch = angleAcY;
}
