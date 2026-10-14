#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ==================================================
// ROBO RACER - ESP8266 NODEMCU / ESP-12E + L298N
// ==================================================
// ENA -> D7 (GPIO13)
// IN1 -> D1 (GPIO5)
// IN2 -> D2 (GPIO4)
// ENB -> D8 (GPIO15) - remove ENB jumper
// IN3 -> D5 (GPIO14)
// IN4 -> D6 (GPIO12)
//
// L298N GND and ESP8266 GND must be connected.
// Use a suitable external supply for the motors.
// The controller uses /drive?l=...&r=... for differential steering.

#define ENA 13
#define IN1 5
#define IN2 4

#define ENB 15
#define IN3 14
#define IN4 12

// ---------------- WIFI ACCESS POINT ----------------
const char* ssid = "OLL_RoboRacer";
const char* password = "robostudios@121321##@@";

ESP8266WebServer server(80);

// ---------------- MOTOR STATE ----------------------
int motorSpeed = 170;
char currentCommand = 'S';

bool moving = false;

unsigned long lastCommandTime = 0;
const unsigned long TIMEOUT_MS = 1200;

// ---------------- STOP MOTORS ----------------------
void stopMotors() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  moving = false;
}

// ---------------- SET MOTOR SPEED/DIRECTION --------
void setMotors(int left, int right) {
  left = constrain(left, -255, 255);
  right = constrain(right, -255, 255);

  // Disable PWM before changing direction.
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  digitalWrite(IN1, left > 0 ? HIGH : LOW);
  digitalWrite(IN2, left < 0 ? HIGH : LOW);

  digitalWrite(IN3, right > 0 ? HIGH : LOW);
  digitalWrite(IN4, right < 0 ? HIGH : LOW);

  analogWrite(ENA, abs(left));
  analogWrite(ENB, abs(right));

  moving = (left != 0 || right != 0);
}

// ---------------- APPLY COMMAND --------------------
void applyCommand() {
  if (motorSpeed == 0 || currentCommand == 'S') {
    stopMotors();
    return;
  }

  int s = constrain(motorSpeed, 0, 255);

  switch (currentCommand) {
    case 'F': setMotors(s, s); break;
    case 'B': setMotors(-s, -s); break;
    case 'L': setMotors(-s, s); break;
    case 'R': setMotors(s, -s); break;

    // Direct differential drive uses /drive.
    case 'D': break;

    default:
      currentCommand = 'S';
      stopMotors();
      break;
  }
}

// ---------------- COMMAND HANDLER ------------------
void handleCommand() {
  if (!server.hasArg("c")) {
    server.send(400, "text/plain", "Missing command");
    return;
  }

  String value = server.arg("c");

  if (value.length() != 1) {
    server.send(400, "text/plain", "Invalid command");
    return;
  }

  char c = value.charAt(0);

  if (c != 'F' && c != 'B' &&
      c != 'L' && c != 'R' && c != 'S') {
    server.send(400, "text/plain", "Unknown command");
    return;
  }

  currentCommand = c;
  lastCommandTime = millis();

  if (c == 'S') {
    stopMotors();
  } else {
    applyCommand();
  }

  server.send(200, "text/plain", "OK");
}

// ---------------- SPEED HANDLER --------------------
void handleSpeed() {
  if (!server.hasArg("v")) {
    server.send(400, "text/plain", "Missing speed");
    return;
  }

  String value = server.arg("v");

  if (value.length() == 0 || value.length() > 3) {
    server.send(400, "text/plain", "Invalid speed");
    return;
  }

  for (unsigned int i = 0; i < value.length(); i++) {
    if (value.charAt(i) < '0' ||
        value.charAt(i) > '9') {
      server.send(400, "text/plain", "Invalid speed");
      return;
    }
  }

  int newSpeed = value.toInt();

  if (newSpeed < 0 || newSpeed > 255) {
    server.send(400, "text/plain", "Speed must be 0-255");
    return;
  }

  motorSpeed = newSpeed;

  // Apply speed immediately to the current direction.
  applyCommand();

  server.send(200, "text/plain", "Speed updated");
}

// ---------------- DIRECT DIFFERENTIAL DRIVE --------
void handleDrive() {
  if (!server.hasArg("l") || !server.hasArg("r")) {
    server.send(400, "text/plain", "Missing motor output");
    return;
  }

  String leftValue = server.arg("l");
  String rightValue = server.arg("r");

  if (leftValue.length() == 0 || rightValue.length() == 0 ||
      leftValue.length() > 4 || rightValue.length() > 4) {
    server.send(400, "text/plain", "Invalid motor output");
    return;
  }

  // Validate optional minus sign and digits.
  auto isIntegerText = [](const String& value) -> bool {
    unsigned int start =
      (value.length() > 0 && value.charAt(0) == '-') ? 1 : 0;

    if (start == value.length()) return false;

    for (unsigned int i = start; i < value.length(); i++) {
      if (value.charAt(i) < '0' ||
          value.charAt(i) > '9') {
        return false;
      }
    }

    return true;
  };

  if (!isIntegerText(leftValue) ||
      !isIntegerText(rightValue)) {
    server.send(400, "text/plain", "Motor outputs must be integers");
    return;
  }

  int left = leftValue.toInt();
  int right = rightValue.toInt();

  if (left < -255 || left > 255 ||
      right < -255 || right > 255) {
    server.send(400, "text/plain",
                "Motor outputs must be -255..255");
    return;
  }

  setMotors(left, right);

  currentCommand =
    (left == 0 && right == 0) ? 'S' : 'D';

  lastCommandTime = millis();

  server.send(200, "text/plain", "Drive updated");
}

// ==================================================
// WEB CONTROLLER
// ==================================================

const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="theme-color" content="#0b0e15">
<title>RoboRacer Controller</title>

<style>
:root {
  --bg: #0b0e15;
  --panel: #111722;
  --border: #263248;
  --blue: #1678ff;
  --blue2: #75b4ff;
  --text: #f5f7ff;
  --muted: #9baac1;
  --red: #f21c3b;
  --green: #3ddc97;
}

* {
  box-sizing: border-box;
  -webkit-tap-highlight-color: transparent;
  user-select: none;
  -webkit-user-select: none;
  -webkit-touch-callout: none;
}

html, body {
  margin: 0;
  min-height: 100%;
  background: var(--bg);
  overscroll-behavior: none;
}

body {
  min-height: 100vh;
  min-height: 100dvh;
  background: radial-gradient(ellipse at 50% 0%, #172131, transparent 65%), var(--bg);
  color: var(--text);
  font-family: Arial, Helvetica, sans-serif;
  display: flex;
  align-items: center;
  justify-content: center;
}

button, input {
  font: inherit;
}

button {
  cursor: pointer;
}

button:focus-visible, input:focus-visible {
  outline: 2px solid var(--blue2);
  outline-offset: 3px;
}

.app {
  width: 100%;
  max-width: 960px;
  margin: auto;
  padding: 8px;
}

.controller {
  width: 100%;
  overflow: hidden;
  border: 1px solid var(--border);
  border-radius: 20px;
  background: linear-gradient(145deg, #151c28, #0e131d);
  box-shadow: 0 16px 50px #0006, inset 0 1px 0 #ffffff08;
}

.topbar {
  padding: 12px 14px;
  border-bottom: 1px solid var(--border);
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  flex-wrap: wrap;
}

.brand {
  display: flex;
  align-items: center;
  gap: 8px;
}

.car-icon {
  font-size: 24px;
}

.brand h1 {
  margin: 0;
  font-size: 22px;
  font-weight: 900;
  letter-spacing: -1px;
}

.brand h1 span {
  color: var(--blue);
}

.top-actions {
  display: flex;
  align-items: center;
  gap: 10px;
}

.connection {
  display: flex;
  align-items: center;
  gap: 7px;
  color: var(--muted);
  font-size: 11px;
  font-weight: 800;
  letter-spacing: .5px;
}

.connection-dot {
  width: 9px;
  height: 9px;
  flex-shrink: 0;
  border-radius: 50%;
  background: #8792a5;
}

.connection.online .connection-dot {
  background: var(--green);
  box-shadow: 0 0 10px #3ddc9780;
}

.connection.error .connection-dot {
  background: var(--red);
  box-shadow: 0 0 10px #f21c3b88;
}

.icon-button {
  height: 38px;
  padding: 0 11px;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 7px;
  border: 1px solid #344158;
  border-radius: 11px;
  background: #1a2434;
  color: #e8efff;
  font-size: 13px;
  font-weight: 800;
}

.status-strip {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 8px;
  padding: 10px 12px 0;
}

.stat {
  min-width: 0;
  background: #101722;
  border: 1px solid #202b3d;
  border-radius: 11px;
  padding: 9px 10px;
}

.stat-label {
  font-size: 9px;
  letter-spacing: .8px;
  font-weight: 900;
  color: #8494ad;
}

.stat-value {
  margin-top: 4px;
  font-size: 15px;
  font-weight: 900;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.stat-value.blue {
  color: var(--blue2);
}

.mode-row {
  padding: 12px 12px 0;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
}

.section-label {
  font-size: 10px;
  font-weight: 900;
  letter-spacing: 1px;
  color: var(--muted);
}

.controls {
  padding: 16px 12px;
  display: grid;
  grid-template-columns: 1fr 1.35fr;
  align-items: center;
  justify-items: center;
  gap: 12px;
  touch-action: none;
}

.vertical-arrows {
  width: 100%;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 12px;
  touch-action: none;
}

.horizontal-arrows {
  width: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 10px;
  touch-action: none;
}

.arrow {
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0;
  color: #e9efff;
  background: linear-gradient(145deg, #202c40, #121a28);
  border: 2px solid #070b12;
  border-radius: 16px;
  box-shadow: 0 5px 0 #070b12, inset 0 1px 0 #ffffff12;
  touch-action: none;
  transition: background .06s, color .06s, transform .06s;
}

.arrow-up, .arrow-down {
  width: 92px;
  height: 82px;
  font-size: 46px;
}

.arrow-left, .arrow-right {
  width: 96px;
  height: 82px;
  font-size: 46px;
}

.arrow.active {
  color: white;
  background: linear-gradient(145deg, #56a1ff, #1269f2);
  border-color: var(--blue);
  box-shadow: 0 0 0 3px #1678ff25, 0 0 25px #1678ff4a;
  transform: translateY(3px);
}

.bottom-bar {
  border-top: 1px solid var(--border);
  padding: 14px 16px;
  display: grid;
  grid-template-columns: 1fr;
  align-items: center;
  gap: 14px;
}

.stop-button {
  min-height: 52px;
  width: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 10px;
  padding: 10px;
  border: 0;
  border-radius: 40px;
  background: linear-gradient(120deg, #ff2144, #e9002b);
  color: white;
  font-size: 18px;
  font-weight: 900;
  touch-action: manipulation;
}

.stop-square {
  width: 15px;
  height: 15px;
  background: white;
  border-radius: 3px;
}

.speed-area {
  min-width: 0;
  width: 100%;
}

.speed-heading {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 10px;
  margin-bottom: 6px;
}

.speed-title {
  color: var(--muted);
  font-size: 12px;
  font-weight: 900;
  letter-spacing: .6px;
}

.speed-number {
  color: var(--blue2);
  font-size: 22px;
  font-weight: 900;
}

.slider-wrap {
  padding: 4px 0 6px;
}

#speedSlider {
  display: block;
  width: 100%;
  height: 12px;
  margin: 0;
  border-radius: 20px;
  background: #253248;
  appearance: none;
  -webkit-appearance: none;
  cursor: pointer;
  touch-action: pan-y;
}

#speedSlider::-webkit-slider-thumb {
  appearance: none;
  -webkit-appearance: none;
  width: 27px;
  height: 27px;
  border-radius: 50%;
  background: var(--blue);
  border: 2px solid #4095ff;
}

#speedSlider::-moz-range-thumb {
  width: 25px;
  height: 25px;
  border-radius: 50%;
  background: var(--blue);
  border: 2px solid #4095ff;
}

.speed-labels {
  display: flex;
  justify-content: space-between;
  margin-top: 6px;
  color: #8594ab;
  font-size: 10px;
}

.presets {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 8px;
  margin-top: 10px;
}

.preset {
  min-height: 36px;
  border: 1px solid #344158;
  border-radius: 9px;
  background: #1a2434;
  color: #d8e3f5;
  font-size: 12px;
  font-weight: 900;
}

.preset.selected {
  background: #155ac0;
  border-color: #3288ff;
  color: white;
}

.footer {
  padding: 0 12px 10px;
  text-align: center;
  color: #65758d;
  font-size: 11px;
}

/* Joystick layouts */

.joystick-layout {
  display: none;
  padding: 16px 12px 20px;
  gap: 20px;
  align-items: center;
  justify-content: space-evenly;
  min-height: 250px;
  touch-action: none;
}

.joystick-layout.visible {
  display: flex;
}

.stick-column {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 10px;
}

.stick-label {
  color: var(--muted);
  font-size: 10px;
  font-weight: 900;
  letter-spacing: 1px;
}

.stick-pad {
  width: clamp(145px, 35vw, 220px);
  height: clamp(145px, 35vw, 220px);
  border-radius: 50%;
  border: 2px solid #344158;
  background: radial-gradient(circle at 35% 25%, #202c40, #101722 70%);
  box-shadow: inset 0 0 25px #0008, 0 8px 22px #0004;
  position: relative;
  touch-action: none;
  overflow: hidden;
}

.stick-pad::before, .stick-pad::after {
  content: "";
  position: absolute;
  background: #34415855;
}

.stick-pad::before {
  width: 1px;
  height: 100%;
  left: 50%;
  top: 0;
}

.stick-pad::after {
  height: 1px;
  width: 100%;
  top: 50%;
  left: 0;
}

.stick-knob {
  width: 34%;
  height: 34%;
  position: absolute;
  left: 50%;
  top: 50%;
  transform: translate(-50%, -50%);
  border-radius: 50%;
  border: 2px solid #75b4ff;
  background: linear-gradient(145deg, #348cff, #1052b5);
  box-shadow: 0 0 22px #1678ff66;
  pointer-events: none;
}

.stick-hint {
  font-size: 10px;
  color: #75859c;
  text-align: center;
}

.layout-hidden {
  display: none !important;
}

/* Settings */

.modal-backdrop {
  position: fixed;
  inset: 0;
  z-index: 50;
  background: #03060bd9;
  display: none;
  align-items: center;
  justify-content: center;
  padding: 16px;
  overflow: auto;
}

.modal-backdrop.open {
  display: flex;
}

.settings-panel {
  width: 100%;
  max-width: 490px;
  max-height: calc(100dvh - 32px);
  overflow: auto;
  border: 1px solid #2b3950;
  border-radius: 20px;
  background: linear-gradient(145deg, #151d2a, #0c111a);
  box-shadow: 0 25px 80px #000a;
  padding: 18px;
}

.settings-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  margin-bottom: 5px;
}

.settings-header h2 {
  font-size: 21px;
  margin: 0;
}

.settings-subtitle {
  font-size: 12px;
  color: var(--muted);
  margin: 0 0 17px;
}

.close-button {
  width: 34px;
  height: 34px;
  border: 1px solid #344158;
  border-radius: 10px;
  background: #202a3a;
  color: #e8efff;
  font-size: 21px;
}

.setting-block {
  border: 1px solid #263248;
  border-radius: 13px;
  background: #101722;
  padding: 13px;
  margin-top: 11px;
}

.setting-title {
  font-size: 12px;
  font-weight: 900;
  color: #eaf0ff;
  margin-bottom: 4px;
}

.setting-description {
  font-size: 11px;
  line-height: 1.45;
  color: #91a0b7;
  margin-bottom: 10px;
}

.layout-options {
  display: grid;
  grid-template-columns: 1fr;
  gap: 7px;
}

.layout-option {
  display: flex;
  align-items: center;
  gap: 10px;
  border: 1px solid #344158;
  border-radius: 10px;
  padding: 10px;
  background: #182231;
  color: #dce6f7;
  font-size: 12px;
  font-weight: 700;
}

.layout-option input {
  accent-color: var(--blue);
  width: 16px;
  height: 16px;
}

.sensitivity-row {
  display: grid;
  grid-template-columns: 1fr auto;
  align-items: center;
  gap: 10px;
  margin-top: 12px;
}

.sensitivity-row label {
  font-size: 12px;
  color: #dce6f7;
  font-weight: 800;
}

.sensitivity-value {
  font-size: 12px;
  color: var(--blue2);
  font-weight: 900;
}

.sensitivity-slider {
  grid-column: 1 / -1;
  width: 100%;
  accent-color: var(--blue);
  touch-action: pan-y;
}

.setting-actions {
  display: flex;
  gap: 8px;
  margin-top: 14px;
}

.setting-actions button {
  flex: 1;
  min-height: 40px;
  border-radius: 10px;
  border: 1px solid #344158;
  background: #1a2434;
  color: #e8efff;
  font-size: 12px;
  font-weight: 900;
}

.setting-actions .primary {
  background: #155ac0;
  border-color: #3288ff;
}

.always-on {
  display: flex;
  align-items: center;
  gap: 10px;
}

.always-on .checkmark {
  width: 23px;
  height: 23px;
  flex-shrink: 0;
  display: grid;
  place-items: center;
  background: #155ac0;
  border-radius: 7px;
  color: #fff;
  font-weight: 900;
}

.always-on strong {
  font-size: 12px;
  display: block;
}

.always-on small {
  display: block;
  color: #91a0b7;
  font-size: 11px;
  line-height: 1.4;
  margin-top: 3px;
}

.modal-note {
  font-size: 10px;
  line-height: 1.5;
  color: #75859c;
  margin: 12px 2px 0;
}

@media (max-width: 370px) {
  .controls {
    padding: 14px 8px;
    gap: 8px;
  }

  .arrow-up, .arrow-down {
    width: 76px;
    height: 72px;
    font-size: 40px;
  }

  .arrow-left, .arrow-right {
    width: 78px;
    height: 72px;
    font-size: 40px;
  }

  .horizontal-arrows {
    gap: 7px;
  }

  .status-strip {
    padding-left: 8px;
    padding-right: 8px;
    gap: 5px;
  }

  .stat {
    padding: 8px;
  }

  .stat-value {
    font-size: 13px;
  }
}

@media (min-width: 650px) {
  .app {
    padding: 20px;
  }

  .controls {
    padding: 28px 40px;
    gap: 30px;
  }

  .arrow-up, .arrow-down {
    width: 125px;
    height: 112px;
    font-size: 64px;
  }

  .arrow-left, .arrow-right {
    width: 145px;
    height: 112px;
    font-size: 64px;
  }

  .bottom-bar {
    grid-template-columns: 210px 1fr;
    gap: 28px;
    padding: 20px 32px;
  }

  .joystick-layout {
    padding: 25px;
  }
}
</style>
</head>

<body>
<div class="app">
<div class="controller">

  <header class="topbar">
    <div class="brand">
      <div class="car-icon">🏎️</div>
      <h1>Robo<span>Racer</span></h1>
    </div>

    <div class="top-actions">
      <div class="connection" id="connection">
        <span class="connection-dot"></span>
        <span id="statusText">READY</span>
      </div>

      <button type="button" class="icon-button" id="settingsButton">
        ⚙ <span>SETTINGS</span>
      </button>
    </div>
  </header>

  <section class="status-strip">
    <div class="stat">
      <div class="stat-label">DRIVE STATE</div>
      <div class="stat-value" id="driveState">STOPPED</div>
    </div>

    <div class="stat">
      <div class="stat-label">OUTPUT</div>
      <div class="stat-value blue" id="outputValue">0 / 255</div>
    </div>

    <div class="stat">
      <div class="stat-label">CONTROL MODE</div>
      <div class="stat-value" id="currentLayout">ARROWS</div>
    </div>
  </section>

  <div class="mode-row">
    <span class="section-label">QUICK SPEED</span>
    <span class="section-label" id="sensitivitySummary">
      STEERING L 100% · R 100%
    </span>
  </div>

  <main class="controls" id="classicControls">
    <div class="vertical-arrows">
      <button type="button" class="arrow arrow-up" data-c="F" id="btnF">↑</button>
      <button type="button" class="arrow arrow-down" data-c="B" id="btnB">↓</button>
    </div>

    <div class="horizontal-arrows">
      <button type="button" class="arrow arrow-left" data-c="L" id="btnL">←</button>
      <button type="button" class="arrow arrow-right" data-c="R" id="btnR">→</button>
    </div>
  </main>

  <section class="joystick-layout" id="singleJoystickLayout">
    <div class="stick-column">
      <div class="stick-label">THROTTLE + STEERING</div>
      <div class="stick-pad" id="singlePad" data-stick="single">
        <div class="stick-knob" id="singleKnob"></div>
      </div>
      <div class="stick-hint">Drag to drive and steer</div>
    </div>
  </section>

  <section class="joystick-layout" id="dualJoystickLayout">
    <div class="stick-column">
      <div class="stick-label">THROTTLE</div>
      <div class="stick-pad" id="throttlePad" data-stick="throttle">
        <div class="stick-knob" id="throttleKnob"></div>
      </div>
      <div class="stick-hint">Up: forward · Down: reverse</div>
    </div>

    <div class="stick-column">
      <div class="stick-label">STEERING</div>
      <div class="stick-pad" id="steeringPad" data-stick="steering">
        <div class="stick-knob" id="steeringKnob"></div>
      </div>
      <div class="stick-hint">Left and right steering</div>
    </div>
  </section>

  <section class="bottom-bar">
    <button type="button" class="stop-button" id="stopButton">
      <span class="stop-square"></span> STOP
    </button>

    <div class="speed-area">
      <div class="speed-heading">
        <span class="speed-title">MOTOR OUTPUT LIMIT</span>
        <span class="speed-number" id="speedValue">170</span>
      </div>

      <div class="slider-wrap">
        <input id="speedSlider" type="range" min="0" max="255" value="170">
      </div>

      <div class="speed-labels">
        <span>SLOW</span><span>MEDIUM</span><span>FAST</span>
      </div>

      <div class="presets">
        <button type="button" class="preset" data-speed="100">LOW</button>
        <button type="button" class="preset selected" data-speed="170">MED</button>
        <button type="button" class="preset" data-speed="255">MAX</button>
      </div>
    </div>
  </section>

  <div class="footer">
    Hold controls to drive · Slide between arrows without lifting · Space = STOP
  </div>

</div>
</div>

<div class="modal-backdrop" id="settingsBackdrop" role="dialog" aria-modal="true">
  <section class="settings-panel">
    <div class="settings-header">
      <h2>Controller settings</h2>
      <button type="button" class="close-button" id="closeSettings">×</button>
    </div>

    <p class="settings-subtitle">Personalize the way RoboRacer responds.</p>

    <div class="setting-block">
      <div class="setting-title">CONTROL LAYOUT</div>
      <div class="setting-description">Choose your preferred control method.</div>

      <div class="layout-options">
        <label class="layout-option">
          <input type="radio" name="controlLayout" value="classic">
          <span>Classic arrows</span>
        </label>

        <label class="layout-option">
          <input type="radio" name="controlLayout" value="joystick">
          <span>Virtual joystick</span>
        </label>

        <label class="layout-option">
          <input type="radio" name="controlLayout" value="dual">
          <span>Dual-stick controller</span>
        </label>
      </div>
    </div>

    <div class="setting-block">
      <div class="setting-title">STEERING SENSITIVITY</div>
      <div class="setting-description">
        Adjust left and right steering independently.
      </div>

      <div class="sensitivity-row">
        <label for="leftSensitivity">Left turn (←)</label>
        <output class="sensitivity-value" id="leftSensitivityValue">100%</output>
        <input class="sensitivity-slider" id="leftSensitivity"
               type="range" min="25" max="150" step="5" value="100">
      </div>

      <div class="sensitivity-row">
        <label for="rightSensitivity">Right turn (→)</label>
        <output class="sensitivity-value" id="rightSensitivityValue">100%</output>
        <input class="sensitivity-slider" id="rightSensitivity"
               type="range" min="25" max="150" step="5" value="100">
      </div>
    </div>

    <div class="setting-block">
      <div class="always-on">
        <span class="checkmark">✓</span>
        <div>
          <strong>Slide between buttons — ALWAYS ON</strong>
          <small>
            Slide from one arrow to another without lifting your finger.
            This behavior cannot be disabled.
          </small>
        </div>
      </div>
    </div>

    <div class="setting-actions">
      <button type="button" id="resetSettings">RESET DEFAULTS</button>
      <button type="button" class="primary" id="doneSettings">DONE</button>
    </div>

    <p class="modal-note">
      Sensitivity changes software steering strength. It does not measure
      actual wheel angle or motor RPM.
    </p>
  </section>
</div>

<script>
"use strict";

const $ = id => document.getElementById(id);

const connection = $("connection");
const statusText = $("statusText");
const slider = $("speedSlider");
const speedValue = $("speedValue");
const driveState = $("driveState");
const outputValue = $("outputValue");
const currentLayoutLabel = $("currentLayout");
const sensitivitySummary = $("sensitivitySummary");

const btnMap = {
  F: $("btnF"),
  B: $("btnB"),
  L: $("btnL"),
  R: $("btnR")
};

const buttonState = { F: false, B: false, L: false, R: false };
const keyboardState = { F: false, B: false, L: false, R: false };
const pointerActions = new Map();

const stickState = {
  single: { x: 0, y: 0 },
  throttle: { x: 0, y: 0 },
  steering: { x: 0, y: 0 }
};

const DEFAULT_SETTINGS = {
  layout: "classic",
  leftSensitivity: 100,
  rightSensitivity: 100
};

let settings = { ...DEFAULT_SETTINGS };
let activeCommand = "S";
let heartbeat = null;
let speedTimer = null;
let speed = 170;
let requestChain = Promise.resolve();
let commandGeneration = 0;
let lastDrive = { l: 0, r: 0 };

function setStatus(state, text) {
  connection.classList.remove("online", "error");

  if (state === "online") connection.classList.add("online");
  if (state === "error") connection.classList.add("error");

  statusText.textContent = text;
}

function enqueueRequest(path, generation, allowStale = false) {
  const task = requestChain.then(async () => {
    if (!allowStale && generation !== commandGeneration) return null;

    const response = await fetch(path, {
      method: "GET",
      cache: "no-store"
    });

    if (!response.ok) throw new Error("HTTP " + response.status);

    return response;
  });

  requestChain = task.catch(() => {});
  return task;
}

function sendDrive(left, right, forceStop = false) {
  const generation = commandGeneration;

  const l = Math.max(-255, Math.min(255, Math.round(left)));
  const r = Math.max(-255, Math.min(255, Math.round(right)));

  lastDrive = { l, r };
  activeCommand = (l === 0 && r === 0) ? "S" : "D";

  driveState.textContent = activeCommand === "S" ? "STOPPED" : "MOVING";

  const path = "/drive?l=" + l + "&r=" + r;

  enqueueRequest(path, generation, forceStop)
    .then(response => {
      if (response && generation === commandGeneration) {
        setStatus("online", "CONNECTED");
      }
    })
    .catch(() => {
      if (generation === commandGeneration) {
        setStatus("error", "CONNECTION ERROR");
      }
    });
}

function startHeartbeat() {
  if (heartbeat !== null) return;

  heartbeat = setInterval(() => {
    if (activeCommand !== "S") {
      sendDrive(lastDrive.l, lastDrive.r);
    }
  }, 300);
}

function stopHeartbeat() {
  if (heartbeat !== null) {
    clearInterval(heartbeat);
    heartbeat = null;
  }
}

function normalizeAxes(throttle, steering) {
  throttle = Math.max(-1, Math.min(1, throttle));
  steering = Math.max(-1, Math.min(1, steering));

  if (steering < 0) {
    steering *= settings.leftSensitivity / 100;
  } else if (steering > 0) {
    steering *= settings.rightSensitivity / 100;
  }

  let left = throttle + steering;
  let right = throttle - steering;

  const peak = Math.max(1, Math.abs(left), Math.abs(right));

  left = left / peak * speed;
  right = right / peak * speed;

  return {
    l: Math.round(left),
    r: Math.round(right)
  };
}

function getButtonAxes() {
  const pressed = {
    F: buttonState.F || keyboardState.F,
    B: buttonState.B || keyboardState.B,
    L: buttonState.L || keyboardState.L,
    R: buttonState.R || keyboardState.R
  };

  return {
    throttle: pressed.F === pressed.B ? 0 : pressed.F ? 1 : -1,
    steering: pressed.R === pressed.L ? 0 : pressed.R ? 1 : -1
  };
}

function getKeyboardAxes() {
  return {
    throttle: keyboardState.F === keyboardState.B
      ? 0 : keyboardState.F ? 1 : -1,

    steering: keyboardState.R === keyboardState.L
      ? 0 : keyboardState.R ? 1 : -1
  };
}

function getAxes() {
  if (settings.layout === "classic") {
    return getButtonAxes();
  }

  const base = settings.layout === "joystick"
    ? {
        throttle: -stickState.single.y,
        steering: stickState.single.x
      }
    : {
        throttle: -stickState.throttle.y,
        steering: stickState.steering.x
      };

  const keys = getKeyboardAxes();

  return {
    throttle: Math.max(-1, Math.min(1, base.throttle + keys.throttle)),
    steering: Math.max(-1, Math.min(1, base.steering + keys.steering))
  };
}

function renderButtonStates() {
  const pressed = {
    F: buttonState.F || keyboardState.F,
    B: buttonState.B || keyboardState.B,
    L: buttonState.L || keyboardState.L,
    R: buttonState.R || keyboardState.R
  };

  Object.keys(btnMap).forEach(key => {
    btnMap[key].classList.toggle("active", pressed[key]);
  });
}

function evaluateControls() {
  renderButtonStates();

  const axes = getAxes();
  const output = normalizeAxes(axes.throttle, axes.steering);

  sendDrive(output.l, output.r);

  if (output.l === 0 && output.r === 0) {
    stopHeartbeat();
  } else {
    startHeartbeat();
  }

  outputValue.textContent =
    Math.max(Math.abs(output.l), Math.abs(output.r)) + " / 255";
}

function clearInputs() {
  Object.keys(buttonState).forEach(key => buttonState[key] = false);
  Object.keys(keyboardState).forEach(key => keyboardState[key] = false);

  pointerActions.clear();

  Object.keys(stickState).forEach(key => {
    stickState[key].x = 0;
    stickState[key].y = 0;
  });

  ["singleKnob", "throttleKnob", "steeringKnob"].forEach(id => {
    const knob = $(id);
    if (knob) knob.style.transform = "translate(-50%,-50%)";
  });

  renderButtonStates();
}

function stopAll() {
  commandGeneration++;
  clearInputs();

  activeCommand = "S";
  lastDrive = { l: 0, r: 0 };

  stopHeartbeat();

  driveState.textContent = "STOPPED";
  outputValue.textContent = "0 / 255";

  sendDrive(0, 0, true);
}

function buttonUnderPointer(event) {
  const element = document.elementFromPoint(event.clientX, event.clientY);
  const button = element && element.closest
    ? element.closest("[data-c]")
    : null;

  return button && button.isConnected ? button.dataset.c : null;
}

function recomputePointerButtons() {
  Object.keys(buttonState).forEach(key => buttonState[key] = false);

  for (const action of pointerActions.values()) {
    if (action.type === "button" && action.dir) {
      buttonState[action.dir] = true;
    }
  }
}

function setPointerButton(pointerId, direction) {
  const old = pointerActions.get(pointerId);

  if (old && old.type === "button" && old.dir === direction) return;

  // Keep tracking while the finger slides through the gap between buttons.
  pointerActions.set(pointerId, {
    type: "button",
    dir: direction || null
  });

  recomputePointerButtons();
  evaluateControls();
}

Object.values(btnMap).forEach(button => {
  button.addEventListener("pointerdown", event => {
    event.preventDefault();

    try {
      button.setPointerCapture(event.pointerId);
    } catch (_) {}

    setPointerButton(event.pointerId, button.dataset.c);
  });

  button.addEventListener("contextmenu", event => event.preventDefault());
});

document.addEventListener("pointermove", event => {
  const action = pointerActions.get(event.pointerId);
  if (!action) return;

  if (action.type === "button") {
    const next = buttonUnderPointer(event);

    if (next !== action.dir) {
      setPointerButton(event.pointerId, next);
    }
  } else if (action.type === "stick") {
    updateStickFromPointer(action.stick, event.clientX, event.clientY);
  }
});

function releasePointer(event) {
  const action = pointerActions.get(event.pointerId);
  if (!action) return;

  pointerActions.delete(event.pointerId);
  recomputePointerButtons();

  if (action.type === "stick") {
    stickState[action.stick].x = 0;
    stickState[action.stick].y = 0;
    renderStick(action.stick);
  }

  evaluateControls();
}

document.addEventListener("pointerup", releasePointer);
document.addEventListener("pointercancel", releasePointer);

function renderStick(name) {
  const ids = {
    single: "singleKnob",
    throttle: "throttleKnob",
    steering: "steeringKnob"
  };

  const knob = $(ids[name]);
  if (!knob) return;

  const state = stickState[name];
  const pad = knob.parentElement;

  const maxX = pad.clientWidth * 0.32;
  const maxY = pad.clientHeight * 0.32;

  knob.style.transform =
    "translate(calc(-50% + " + state.x * maxX +
    "px), calc(-50% + " + state.y * maxY + "px))";
}

function updateStickFromPointer(name, x, y) {
  const ids = {
    single: "singlePad",
    throttle: "throttlePad",
    steering: "steeringPad"
  };

  const pad = $(ids[name]);
  const rect = pad.getBoundingClientRect();

  let nx = (x - (rect.left + rect.width / 2)) / (rect.width * 0.34);
  let ny = (y - (rect.top + rect.height / 2)) / (rect.height * 0.34);

  nx = Math.max(-1, Math.min(1, nx));
  ny = Math.max(-1, Math.min(1, ny));

  if (name === "throttle") nx = 0;
  if (name === "steering") ny = 0;

  stickState[name] = { x: nx, y: ny };

  renderStick(name);
  evaluateControls();
}

["singlePad", "throttlePad", "steeringPad"].forEach(id => {
  const pad = $(id);
  if (!pad) return;

  pad.addEventListener("pointerdown", event => {
    event.preventDefault();

    const name = pad.dataset.stick;

    try {
      pad.setPointerCapture(event.pointerId);
    } catch (_) {}

    pointerActions.set(event.pointerId, {
      type: "stick",
      stick: name
    });

    updateStickFromPointer(name, event.clientX, event.clientY);
  });
});

const keyToDir = {
  ArrowUp: "F",
  KeyW: "F",
  ArrowDown: "B",
  KeyS: "B",
  ArrowLeft: "L",
  KeyA: "L",
  ArrowRight: "R",
  KeyD: "R"
};

window.addEventListener("keydown", event => {
  if (event.code === "Space") {
    event.preventDefault();
    stopAll();
    return;
  }

  // FIX: keyboard driving is disabled while Settings is open.
  if ($("settingsBackdrop").classList.contains("open")) return;

  const direction = keyToDir[event.code];

  if (direction && !keyboardState[direction]) {
    event.preventDefault();
    keyboardState[direction] = true;
    evaluateControls();
  }
});

window.addEventListener("keyup", event => {
  const direction = keyToDir[event.code];

  if (direction && keyboardState[direction]) {
    event.preventDefault();
    keyboardState[direction] = false;

    if (!$("settingsBackdrop").classList.contains("open")) {
      evaluateControls();
    }
  }
});

$("stopButton").addEventListener("pointerdown", event => {
  event.preventDefault();
  stopAll();
});

$("stopButton").addEventListener("click", event => {
  event.preventDefault();
  stopAll();
});

function updateSpeed(value) {
  speed = Math.max(0, Math.min(255, Number(value) || 0));
  speedValue.textContent = speed;

  document.querySelectorAll("[data-speed]").forEach(button => {
    button.classList.toggle("selected", Number(button.dataset.speed) === speed);
  });

  if (speedTimer !== null) clearTimeout(speedTimer);

  speedTimer = setTimeout(() => {
    const generation = commandGeneration;

    enqueueRequest("/speed?v=" + speed, generation)
      .then(response => {
        if (response && generation === commandGeneration) {
          setStatus("online", "CONNECTED");
        }
      })
      .catch(() => {
        if (generation === commandGeneration) {
          setStatus("error", "SPEED ERROR");
        }
      });

    evaluateControls();
    speedTimer = null;
  }, 80);
}

slider.addEventListener("input", event => updateSpeed(event.target.value));

document.querySelectorAll("[data-speed]").forEach(button => {
  button.addEventListener("click", () => {
    slider.value = button.dataset.speed;
    updateSpeed(button.dataset.speed);
  });
});

function clampSensitivity(value) {
  const number = Number(value);

  return Number.isFinite(number)
    ? Math.max(25, Math.min(150, Math.round(number / 5) * 5))
    : 100;
}

function saveSettings() {
  try {
    localStorage.setItem("roboracer-settings-v2", JSON.stringify(settings));
  } catch (_) {}
}

function loadSettings() {
  try {
    const raw = localStorage.getItem("roboracer-settings-v2");
    if (!raw) return;

    const parsed = JSON.parse(raw);

    if (parsed && ["classic", "joystick", "dual"].includes(parsed.layout)) {
      settings.layout = parsed.layout;
      settings.leftSensitivity = clampSensitivity(parsed.leftSensitivity);
      settings.rightSensitivity = clampSensitivity(parsed.rightSensitivity);
    }
  } catch (_) {
    settings = { ...DEFAULT_SETTINGS };
  }
}

function renderSettings() {
  document.querySelectorAll('input[name="controlLayout"]').forEach(input => {
    input.checked = input.value === settings.layout;
  });

  $("leftSensitivity").value = settings.leftSensitivity;
  $("rightSensitivity").value = settings.rightSensitivity;

  $("leftSensitivityValue").textContent = settings.leftSensitivity + "%";
  $("rightSensitivityValue").textContent = settings.rightSensitivity + "%";

  sensitivitySummary.textContent =
    "STEERING L " + settings.leftSensitivity +
    "% · R " + settings.rightSensitivity + "%";

  $("classicControls").classList.toggle(
    "layout-hidden", settings.layout !== "classic"
  );

  $("singleJoystickLayout").classList.toggle(
    "visible", settings.layout === "joystick"
  );

  $("dualJoystickLayout").classList.toggle(
    "visible", settings.layout === "dual"
  );

  currentLayoutLabel.textContent = {
    classic: "ARROWS",
    joystick: "JOYSTICK",
    dual: "DUAL STICK"
  }[settings.layout];

  if (settings.layout !== "classic") {
    Object.keys(buttonState).forEach(key => buttonState[key] = false);

    Object.keys(stickState).forEach(key => {
      stickState[key].x = 0;
      stickState[key].y = 0;
    });

    ["singleKnob", "throttleKnob", "steeringKnob"].forEach(id => {
      const knob = $(id);
      if (knob) knob.style.transform = "translate(-50%,-50%)";
    });

    renderButtonStates();
  }
}

function openSettings() {
  stopAll();
  renderSettings();
  $("settingsBackdrop").classList.add("open");
}

function closeSettings() {
  $("settingsBackdrop").classList.remove("open");
}

$("settingsButton").addEventListener("click", openSettings);
$("closeSettings").addEventListener("click", closeSettings);
$("doneSettings").addEventListener("click", closeSettings);

$("settingsBackdrop").addEventListener("click", event => {
  if (event.target === $("settingsBackdrop")) closeSettings();
});

document.addEventListener("keydown", event => {
  if (event.key === "Escape" &&
      $("settingsBackdrop").classList.contains("open")) {
    closeSettings();
  }
});

document.querySelectorAll('input[name="controlLayout"]').forEach(input => {
  input.addEventListener("change", () => {
    stopAll();
    settings.layout = input.value;
    saveSettings();
    renderSettings();
  });
});

$("leftSensitivity").addEventListener("input", event => {
  settings.leftSensitivity = clampSensitivity(event.target.value);
  renderSettings();
  saveSettings();
});

$("rightSensitivity").addEventListener("input", event => {
  settings.rightSensitivity = clampSensitivity(event.target.value);
  renderSettings();
  saveSettings();
});

$("resetSettings").addEventListener("click", () => {
  stopAll();
  settings = { ...DEFAULT_SETTINGS };
  saveSettings();
  renderSettings();
});

window.addEventListener("blur", stopAll);

document.addEventListener("visibilitychange", () => {
  if (document.hidden) stopAll();
});

window.addEventListener("pagehide", stopAll);

loadSettings();
renderSettings();
setStatus("", "READY");
</script>
</body>
</html>
)rawliteral";

// ==================================================
// ESP8266 SETUP
// ==================================================

void setup() {
  Serial.begin(115200);

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  analogWriteRange(255);
  analogWriteFreq(1000);

  currentCommand = 'S';
  stopMotors();

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  bool apStarted = WiFi.softAP(ssid, password);

  if (apStarted) {
    Serial.println();
    Serial.println("RoboRacer Wi-Fi started");
    Serial.print("Network: ");
    Serial.println(ssid);

    Serial.print("Open browser at: http://");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Wi-Fi startup failed!");
  }

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", webpage);
  });

  server.on("/cmd", HTTP_GET, handleCommand);
  server.on("/speed", HTTP_GET, handleSpeed);
  server.on("/drive", HTTP_GET, handleDrive);

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.begin();

  lastCommandTime = millis();

  Serial.println("RoboRacer web server ready");
}

// ==================================================
// MAIN LOOP
// ==================================================

void loop() {
  server.handleClient();

  // Safety stop if movement heartbeats disappear.
  if (currentCommand != 'S' &&
      millis() - lastCommandTime >= TIMEOUT_MS) {
    currentCommand = 'S';
    stopMotors();
  }

  yield();
}
