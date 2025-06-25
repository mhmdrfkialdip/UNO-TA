#include <Arduino.h>

#define DIR_PIN_TWIST 6
#define ENA_PIN_TWIST 5
#define PUL_PIN_TWIST 7

#define DIR_PIN_BEND 11
#define ENA_PIN_BEND 12
#define PUL_PIN_BEND 10

const int stepsPerRevolution = 400;
const float degreesPerStep = 360.0 / stepsPerRevolution;

String inputData = "";
bool cancelRequested = false;

// Status dan state untuk twist dan bending dipisah
bool isRunningTwist = false;
float twistLeftCycles = 0;
float twistRightCycles = 0;

int twistLeftStepIndex = 0;
int twistRightStepIndex = 0;
bool twistRightStarted = false;

bool isRunningBend = false;
float bendTargetCycles = 0;
int bendCurrentCycle = 0;
float currentAngle = 90.0;

enum BendState { B_IDLE, B_TO_0, B_TO_180, B_TO_90, B_DONE };
BendState bendState = B_IDLE;

unsigned long lastInputTime = 0; // untuk timeout input

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);

  pinMode(DIR_PIN_TWIST, OUTPUT);
  pinMode(ENA_PIN_TWIST, OUTPUT);
  pinMode(PUL_PIN_TWIST, OUTPUT);
  digitalWrite(ENA_PIN_TWIST, LOW);

  pinMode(DIR_PIN_BEND, OUTPUT);
  pinMode(ENA_PIN_BEND, OUTPUT);
  pinMode(PUL_PIN_BEND, OUTPUT);
  digitalWrite(ENA_PIN_BEND, LOW);

  Serial.println("ESP2 siap menerima perintah...");

  // Bersihkan buffer serial
  while (Serial2.available()) Serial2.read();
}

void loop() {
  while (Serial2.available()) {
    char c = Serial2.read();
    
    // Abaikan karakter non-printable kecuali newline (untuk akhiri input)
    if (isPrintable(c) || c == '\n') {
      if (c != '\n') {
        inputData += c;
        lastInputTime = millis();
      } else {
        // Input lengkap diterima
        inputData.trim();
        Serial.print("Data mentah diterima: ");
        Serial.println(inputData);

        // Validasi input ketat
        if (isValidCommand(inputData)) {
          if (inputData.startsWith("RUN:") || inputData == "IDLE" || inputData == "CANCEL") {
            Serial.println("Perintah diterima: " + inputData);
            parseCommand(inputData);
          } else {
            Serial.println("Data tidak dikenal: " + inputData);
          }
        } else {
          Serial.println("Data tidak valid, diabaikan.");
        }
        inputData = "";
      }
    } else {
      // Abaikan karakter non-printable lain, reset buffer jika ada data partial
      if (inputData.length() > 0) {
        Serial.println("Karakter aneh terdeteksi, reset buffer input.");
        inputData = "";
      }
    }
  }

  // Timeout reset buffer jika input tidak lengkap dalam 2 detik
  if (inputData.length() > 0 && millis() - lastInputTime > 2000) {
    Serial.println("Timeout input, reset buffer.");
    inputData = "";
  }

  // Jalankan twist jika sedang aktif
  if (isRunningTwist) {
    runTwistStep();
  }

  // Jalankan bending jika sedang aktif
  if (isRunningBend) {
    runBendStep();
  }

  if (cancelRequested) {
    cancelRequested = false;
    if (isRunningTwist) {
      isRunningTwist = false;
      Serial2.println("CANCELLED");
      Serial.println("Twist dibatalkan.");
    }
    if (isRunningBend) {
      isRunningBend = false;
      bendState = B_IDLE;
      moveToAngle(90);
      Serial2.println("CANCELLED");
      Serial.println("Bending dibatalkan.");
    }
  }
}

bool isValidCommand(String cmd) {
  // Perintah valid harus dimulai dengan RUN:, IDLE, atau CANCEL (case sensitive)
  if (cmd.startsWith("RUN:") || cmd == "IDLE" || cmd == "CANCEL") return true;
  return false;
}

// Parsing command dan set state mode
void parseCommand(String cmd) {
  if (cmd.startsWith("RUN:")) {
    String params = cmd.substring(4);
    int sepIndex = params.indexOf(':');
    if (sepIndex != -1) {
      twistLeftCycles = params.substring(0, sepIndex).toFloat();
      twistRightCycles = params.substring(sepIndex + 1).toFloat();

      // Reset indeks langkah twist dan flag mulai kanan
      twistLeftStepIndex = 0;
      twistRightStepIndex = 0;
      twistRightStarted = false;

      // Set mode twist aktif, hentikan mode bending
      isRunningTwist = true;
      isRunningBend = false;
      Serial.println("Menjalankan twist...");
    } else {
      // Handle bending mode
      bendTargetCycles = params.toFloat();
      bendCurrentCycle = 0;
      bendState = B_TO_0;
      isRunningBend = true;
      isRunningTwist = false;
      Serial.println("Menjalankan bending...");
    }
    cancelRequested = false;
  } else if (cmd == "IDLE") {
    isRunningBend = false;
    isRunningTwist = false;
    bendState = B_IDLE;
    moveToAngle(90);
  } else if (cmd == "CANCEL") {
    cancelRequested = true;
    Serial.println("Permintaan CANCEL diterima.");
  }
}

// Fungsi langkah step untuk twist (blocking dibagi langkah kecil atau tunjukkan progress)
void runTwistStep() {
  int totalLeftSteps = (int)(twistLeftCycles * stepsPerRevolution);
  int totalRightSteps = (int)(twistRightCycles * stepsPerRevolution);

  // Jika twist kiri belum selesai
  if (twistLeftStepIndex < totalLeftSteps) {
    moveSingleStepTwist(LOW);
    twistLeftStepIndex++;
    if (twistLeftStepIndex % stepsPerRevolution == 0 || twistLeftStepIndex == totalLeftSteps) {
      Serial2.printf("PROG:LEFT:%.2f\n", (float)twistLeftStepIndex / stepsPerRevolution);
    }
    return;
  }

  // Setelah selesai kiri, beri jeda 1 detik sebelum mulai kanan
  if (!twistRightStarted) {
    twistRightStarted = true;
    twistRightStepIndex = 0;
    delay(1000);  // Jeda 1 detik sebelum mulai putaran kanan
  }

  if (twistRightStepIndex < totalRightSteps) {
    moveSingleStepTwist(HIGH);
    twistRightStepIndex++;
    if (twistRightStepIndex % stepsPerRevolution == 0 || twistRightStepIndex == totalRightSteps) {
      Serial2.printf("PROG:RIGHT:%.2f\n", (float)twistRightStepIndex / stepsPerRevolution);
    }
    return;
  }

  // Telah selesai putaran kiri dan kanan
  isRunningTwist = false;
  twistRightStarted = false;
  twistLeftStepIndex = 0;
  twistRightStepIndex = 0;
  Serial2.println("DONE");
}

void moveSingleStepTwist(int direction) {
  digitalWrite(DIR_PIN_TWIST, direction);
  digitalWrite(PUL_PIN_TWIST, HIGH);
  delayMicroseconds(5000);
  digitalWrite(PUL_PIN_TWIST, LOW);
  delayMicroseconds(5000);
}

// Fungsi langkah step untuk bending (berdasarkan state bending)
void runBendStep() {
  switch (bendState) {
    case B_TO_0:
      moveToAngle(0);
      bendState = B_TO_180;
      delay(300);
      break;
    case B_TO_180:
      moveToAngle(180);
      bendState = B_TO_90;
      delay(300);
      break;
    case B_TO_90:
      moveToAngle(90);
      bendCurrentCycle++;
      Serial2.printf("PROG:%d/%.2f\n", bendCurrentCycle, bendTargetCycles);
      if (bendCurrentCycle >= (int)bendTargetCycles) {
        bendState = B_DONE;
      } else {
        bendState = B_TO_0;
      }
      delay(500);
      break;
    case B_DONE:
      isRunningBend = false;
      bendState = B_IDLE;
      Serial2.println("DONE");
      break;
    default:
      break;
  }
}

void moveToAngle(float targetAngle) {
  if (targetAngle != 0 && targetAngle != 90 && targetAngle != 180) return;

  float delta = targetAngle - currentAngle;
  if (abs(delta) < 0.5) return;

  int direction = delta > 0 ? HIGH : LOW;
  int stepsToMove = abs(delta) / degreesPerStep;

  digitalWrite(DIR_PIN_BEND, direction);
  for (int i = 0; i < stepsToMove; i++) {
    if (cancelRequested) return;
    digitalWrite(PUL_PIN_BEND, HIGH);
    delayMicroseconds(2000);
    digitalWrite(PUL_PIN_BEND, LOW);
    delayMicroseconds(5000);
  }

  currentAngle = targetAngle;
}
