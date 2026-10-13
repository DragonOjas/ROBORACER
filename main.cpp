#if defined(__has_include)
  #if __has_include("esp_arduino_version.h")
    #include "esp_arduino_version.h"
  #endif
#endif

#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Bluetooth is not enabled! Make sure to select an ESP32 board with Bluetooth support."
#endif

BluetoothSerial SerialBT;

// --- Pin Definitions ---
const int PWMA_PIN = 25; 
const int AIN1_PIN = 26; 
const int AIN2_PIN = 27; 

const int PWMB_PIN = 18; 
const int BIN1_PIN = 32; 
const int BIN2_PIN = 33; 

const int STBY_PIN = 19; 

// --- PWM Settings ---
const int PWM_FREQ = 20000; 
const int PWM_RES  = 8;     
const int PWM_CH_A = 0;     
const int PWM_CH_B = 1;     

// --- Motion & Race Variables ---
const int MAX_RACE_SPEED = 255;
int motorSpeed = MAX_RACE_SPEED; 

enum OperatingMode { MODE_SUB_AUTO, MODE_MANUAL };
OperatingMode currentMode = MODE_MANUAL;

enum PersistentDir { DIR_STOP, DIR_FORWARD, DIR_BACKWARD };
PersistentDir autoBaseDir = DIR_STOP;

enum SteeringState { STEER_NONE, STEER_LEFT, STEER_RIGHT };
SteeringState tempSteer = STEER_NONE;

bool wasConnected = false;
unsigned long lastPacketTime = 0;

// Watchdog disabled for drag/race reliability
const bool ENABLE_WATCHDOG = false; 
const unsigned long WATCHDOG_TIMEOUT_MS = 1500; 

int currentLeftSpeed  = -999;
int currentRightSpeed = -999;

// --- Function Prototypes ---
void setMotorSpeeds(int leftSpeed, int rightSpeed);
void stopMotors();
void processCommand(char cmd);
void updateSubAutoMotors();
void clearAllStates();

void setup() {
  Serial.begin(115200);

  pinMode(AIN1_PIN, OUTPUT);
  pinMode(AIN2_PIN, OUTPUT);
  pinMode(BIN1_PIN, OUTPUT);
  pinMode(BIN2_PIN, OUTPUT);
  pinMode(STBY_PIN, OUTPUT);

  digitalWrite(STBY_PIN, LOW);

  #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcAttach(PWMA_PIN, PWM_FREQ, PWM_RES);
    ledcAttach(PWMB_PIN, PWM_FREQ, PWM_RES);
  #else
    ledcSetup(PWM_CH_A, PWM_FREQ, PWM_RES);
    ledcAttachPin(PWMA_PIN, PWM_CH_A);
    ledcSetup(PWM_CH_B, PWM_FREQ, PWM_RES);
    ledcAttachPin(PWMB_PIN, PWM_CH_B);
  #endif

  stopMotors();
  digitalWrite(STBY_PIN, HIGH);

  SerialBT.begin("OLL_RoboRacer");
  Serial.println("System Ready! Bluetooth: OLL_RoboRacer");
  
  wasConnected = SerialBT.hasClient();
  lastPacketTime = millis();
}

void loop() {
  bool currentlyConnected = SerialBT.hasClient();

  if (!wasConnected && currentlyConnected) {
    lastPacketTime = millis();
    Serial.println("Controller paired and connected.");
  }

  // --- FAILS-SAFE RACE DASH ---
  // If connection drops, lock wheels straight forward at max speed
  if (wasConnected && !currentlyConnected) {
    Serial.println("Bluetooth lost! Engaging full-speed straight dash.");
    currentMode = MODE_SUB_AUTO;
    autoBaseDir = DIR_FORWARD;
    tempSteer = STEER_NONE;
    motorSpeed = MAX_RACE_SPEED;
    setMotorSpeeds(MAX_RACE_SPEED, MAX_RACE_SPEED);
  }
  wasConnected = currentlyConnected;

  while (SerialBT.available()) {
    char cmd = (char)SerialBT.read();
    if (cmd != '\n' && cmd != '\r') {
      lastPacketTime = millis();
      processCommand(cmd);
    }
  }

  if (ENABLE_WATCHDOG && currentlyConnected && currentMode == MODE_MANUAL) {
    if ((millis() - lastPacketTime > WATCHDOG_TIMEOUT_MS) && (currentLeftSpeed != 0 || currentRightSpeed != 0)) {
      stopMotors();
    }
  }

  if (currentMode == MODE_SUB_AUTO) {
    updateSubAutoMotors();
  }

  yield();
}

void processCommand(char cmd) {
  if (cmd == '0') { motorSpeed = MAX_RACE_SPEED; return; }
  if (cmd >= '1' && cmd <= '9') {
    motorSpeed = map(cmd - '0', 1, 9, 120, 255);
    return;
  }

  // Instant launch trigger
  if (cmd == 'X') {
    clearAllStates();
    currentMode = MODE_SUB_AUTO;
    motorSpeed = MAX_RACE_SPEED;
    autoBaseDir = DIR_FORWARD;
    tempSteer = STEER_NONE;
    updateSubAutoMotors();
    return;
  }

  if (cmd == 'M') { clearAllStates(); currentMode = MODE_MANUAL; return; }
  if (cmd == 'A') { clearAllStates(); currentMode = MODE_SUB_AUTO; return; }
  if (cmd == 'S') { clearAllStates(); return; }

  if (currentMode == MODE_MANUAL) {
    int innerTurnSpeed = (int)(motorSpeed * 0.35);
    switch (cmd) {
      case 'F': setMotorSpeeds(motorSpeed, motorSpeed); break;
      case 'B': setMotorSpeeds(-motorSpeed, -motorSpeed); break;
      case 'L': setMotorSpeeds(innerTurnSpeed, motorSpeed); break;
      case 'R': setMotorSpeeds(motorSpeed, innerTurnSpeed); break;
      case 'C': stopMotors(); break;
      default:  break;
    }
  } 
  else if (currentMode == MODE_SUB_AUTO) {
    switch (cmd) {
      case 'F': autoBaseDir = DIR_FORWARD; tempSteer = STEER_NONE; break;
      case 'B': autoBaseDir = DIR_BACKWARD; tempSteer = STEER_NONE; break;
      case 'L': tempSteer = STEER_LEFT; break;
      case 'R': tempSteer = STEER_RIGHT; break;
      case 'C': tempSteer = STEER_NONE; break;
      default:  break;
    }
    updateSubAutoMotors();
  }
}

void updateSubAutoMotors() {
  if (autoBaseDir == DIR_STOP) {
    stopMotors();
    return;
  }

  int speedSign = (autoBaseDir == DIR_FORWARD) ? 1 : -1;
  int full = speedSign * motorSpeed;
  int highSweep = speedSign * (int)(motorSpeed * 0.65);

  if (tempSteer == STEER_NONE) {
    setMotorSpeeds(full, full);
  } else if (tempSteer == STEER_LEFT) {
    setMotorSpeeds(highSweep, full);
  } else if (tempSteer == STEER_RIGHT) {
    setMotorSpeeds(full, highSweep);
  }
}

void clearAllStates() {
  autoBaseDir = DIR_STOP;
  tempSteer = STEER_NONE;
  currentMode = MODE_MANUAL;
  stopMotors();
}

void setMotorSpeeds(int leftSpeed, int rightSpeed) {
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  if (leftSpeed == currentLeftSpeed && rightSpeed == currentRightSpeed) {
    return;
  }

  currentLeftSpeed = leftSpeed;
  currentRightSpeed = rightSpeed;

  int pwmValA = abs(leftSpeed);
  if (leftSpeed == 0) {
    digitalWrite(AIN1_PIN, HIGH);
    digitalWrite(AIN2_PIN, HIGH);
    pwmValA = 255;
  } else {
    digitalWrite(AIN1_PIN, leftSpeed > 0 ? HIGH : LOW);
    digitalWrite(AIN2_PIN, leftSpeed < 0 ? HIGH : LOW);
  }

  int pwmValB = abs(rightSpeed);
  if (rightSpeed == 0) {
    digitalWrite(BIN1_PIN, HIGH);
    digitalWrite(BIN2_PIN, HIGH);
    pwmValB = 255;
  } else {
    digitalWrite(BIN1_PIN, rightSpeed > 0 ? HIGH : LOW);
    digitalWrite(BIN2_PIN, rightSpeed < 0 ? HIGH : LOW);
  }

  #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWrite(PWMA_PIN, pwmValA);
    ledcWrite(PWMB_PIN, pwmValB);
  #else
    ledcWrite(PWM_CH_A, pwmValA);
    ledcWrite(PWM_CH_B, pwmValB);
  #endif
}

void stopMotors() {
  setMotorSpeeds(0, 0);
}
