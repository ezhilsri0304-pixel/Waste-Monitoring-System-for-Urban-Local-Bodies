#include <Servo.h>
#include <EEPROM.h>

/* ---------- PINS ---------- */

// Sensors
#define MAIN_IR        2
#define METAL_SENSOR   3
#define COMP_IR        13
#define RAIN_SENSOR    5
#define MOISTURE_PIN   A0

// Servos
#define DROP_SERVO_PIN 9
#define RAIN_SERVO_PIN 7
#define COMP_SERVO_PIN 4

// Stepper
#define IN1 8
#define IN2 10
#define IN3 11
#define IN4 12

/* ---------- OBJECTS ---------- */
Servo dropServo;
Servo rainServo;
Servo compServo;

/* ---------- BIN ---------- */
#define BIN_METAL 1
#define BIN_WET   2
#define BIN_DRY   3

int currentBin = 0;

/* ---------- SETTINGS ---------- */
int stepsPerBin = 170;
int moistureThreshold = 450;

/* ---------- STEPPER SEQUENCE ---------- */
int seq[8][4] = {
  {1,0,0,0},
  {1,1,0,0},
  {0,1,0,0},
  {0,1,1,0},
  {0,0,1,0},
  {0,0,1,1},
  {0,0,0,1},
  {1,0,0,1}
};

/* ---------- EEPROM ---------- */
void saveBin(int bin) {
  EEPROM.update(0, bin);
}

void loadBin() {
  currentBin = EEPROM.read(0);
  if(currentBin < 0 || currentBin > 3)
    currentBin = 0;

  Serial.print("Loaded Bin: ");
  Serial.println(currentBin);
}

/* ---------- MOISTURE ---------- */
int readMoisture() {
  int total = 0;
  for(int i=0;i<15;i++) {
    total += analogRead(MOISTURE_PIN);
    delay(20);
  }
  return total / 15;
}

/* ---------- METAL ---------- */
bool isMetal() {
  int count = 0;
  for(int i=0;i<10;i++) {
    if(digitalRead(METAL_SENSOR) == LOW)
      count++;
    delay(20);
  }
  return (count > 5);
}

/* ---------- STEPPER ---------- */
void stepMotor(int steps) {
  for(int i=0;i<steps;i++) {
    for(int j=0;j<8;j++) {
      digitalWrite(IN1, seq[j][0]);
      digitalWrite(IN2, seq[j][1]);
      digitalWrite(IN3, seq[j][2]);
      digitalWrite(IN4, seq[j][3]);
      delay(2);
    }
  }

  digitalWrite(IN1,0);
  digitalWrite(IN2,0);
  digitalWrite(IN3,0);
  digitalWrite(IN4,0);
}

/* ---------- ROTATE ---------- */
void rotateToBin(int target) {
  if(target == currentBin) return;

  int steps = (target - currentBin) * stepsPerBin;

  if(steps < 0)
    steps += stepsPerBin * 3;

  Serial.print("Rotating Steps: ");
  Serial.println(steps);

  stepMotor(steps);

  currentBin = target;
  saveBin(currentBin);
}

/* ---------- DROP ---------- */
void dropWaste() {
  Serial.println("Dropping waste");

  dropServo.write(180);
  delay(1200);

  dropServo.write(0);
  delay(800);
}

/* ---------- LID ---------- */
void handleLid() {
  if(digitalRead(RAIN_SENSOR) == LOW)
    rainServo.write(180);   // Close
  else
    rainServo.write(0);     // Open
}

/* ---------- COMPRESSION ---------- */
void handleCompression() {

  if(digitalRead(COMP_IR) == LOW) {

    Serial.println("Compression Triggered");

    // Slow push (better force)
    for(int pos = 0; pos <= 120; pos += 5) {
      compServo.write(pos);
      delay(20);
    }

    delay(200); // small hold

    // Return
    for(int pos = 120; pos >= 0; pos -= 5) {
      compServo.write(pos);
      delay(20);
    }

    // Prevent retrigger
    while(digitalRead(COMP_IR) == LOW) {
      delay(10);
    }
  }
}

/* ---------- SETUP ---------- */
void setup() {
  Serial.begin(9600);

  pinMode(MAIN_IR, INPUT);
  pinMode(METAL_SENSOR, INPUT);
  pinMode(COMP_IR, INPUT);
  pinMode(RAIN_SENSOR, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  dropServo.attach(DROP_SERVO_PIN);
  rainServo.attach(RAIN_SERVO_PIN);
  compServo.attach(COMP_SERVO_PIN);

  dropServo.write(0);
  rainServo.write(0);
  compServo.write(0);

  loadBin();

  Serial.println("SMART BIN READY");
}

/* ---------- LOOP ---------- */
void loop() {

  handleLid();
  handleCompression();

  if(digitalRead(MAIN_IR) == LOW) {

    Serial.println("Waste detected");

    delay(300);  // stable detection

    if(digitalRead(MAIN_IR) == LOW) {

      delay(700);  // allow waste to settle

      int moisture = readMoisture();
      bool metal = isMetal();

      Serial.print("Moisture: ");
      Serial.println(moisture);

      Serial.print("Metal: ");
      Serial.println(metal ? "YES" : "NO");

      if(moisture < moistureThreshold) {
        Serial.println("Wet waste");
        rotateToBin(BIN_WET);
      }
      else if(metal) {
        Serial.println("Metal waste");
        rotateToBin(BIN_METAL);
      }
      else {
        Serial.println("Dry waste");
        rotateToBin(BIN_DRY);
      }

      dropWaste();

      while(digitalRead(MAIN_IR) == LOW) {
        delay(10);
      }

      delay(800);
    }
  }
}