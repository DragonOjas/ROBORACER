#include <WiFi.h>
#include <WebServer.h>

// ---------------- BTS7960 PIN DEFINITIONS ----------
#define L_RPWM 25  // Left BTS7960 RPWM (Forward)
#define L_LPWM 26  // Left BTS7960 LPWM (Reverse)
#define R_RPWM 27  // Right BTS7960 RPWM (Forward)
#define R_LPWM 14  // Right BTS7960 LPWM (Reverse)

// ---------------- PWM HARDWARE CONFIG --------------
#define PWM_FREQ 5000
#define PWM_RES  8

// ---------------- WIFI ACCESS POINT ----------------
const char* ssid = "OLL_RoboRacer";
const char* password = "robostudios@121321##@@";

WebServer server(80);

// ---------------- MOTOR & RAMP STATE ---------------
int motorSpeed = 170;
char currentCommand = 'S';
bool moving = false;

int targetLeft = 0;
int targetRight = 0;
int currentLeft = 0;
int currentRight = 0;

const unsigned long RAMP_INTERVAL_MS = 15;
const int RAMP_STEP_ACCEL = 12;
const int RAMP_STEP_DECEL = 28;
unsigned long lastRampTime = 0;

unsigned long lastCommandTime = 0;
const unsigned long TIMEOUT_MS = 600;

// ---------------- UNIVERSAL ESP32 PWM SETUP --------
// Compiles cleanly on ESP32 Arduino Core v1.x, v2.x, and v3.x
void setupPWM() {
#if defined(ESP_ARDUINO_VERSION) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
  ledcAttach(L_RPWM, PWM_FREQ, PWM_RES);
  ledcAttach(L_LPWM, PWM_FREQ, PWM_RES);
  ledcAttach(R_RPWM, PWM_FREQ, PWM_RES);
  ledcAttach(R_LPWM, PWM_FREQ, PWM_RES);
#else
  ledcSetup(0, PWM_FREQ, PWM_RES);
  ledcSetup(1, PWM_FREQ, PWM_RES);
  ledcSetup(2, PWM_FREQ, PWM_RES);
  ledcSetup(3, PWM_FREQ, PWM_RES);
  ledcAttachPin(L_RPWM, 0);
  ledcAttachPin(L_LPWM, 1);
  ledcAttachPin(R_RPWM, 2);
  ledcAttachPin(R_LPWM, 3);
#endif
}

void writePWM(int pin, int channel, int duty) {
  duty = constrain(duty, 0, 255);
#if defined(ESP_ARDUINO_VERSION) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
  ledcWrite(pin, duty);
#else
  ledcWrite(channel, duty);
#endif
}

// ---------------- HARDWARE PWM OUTPUT --------------
void writeMotorHardware(int left, int right) {
  left = constrain(left, -255, 255);
  right = constrain(right, -255, 255);

  // Left BTS7960 Driver (Channels 0 & 1)
  if (left > 0) {
    writePWM(L_LPWM, 1, 0);
    writePWM(L_RPWM, 0, left);
  } else if (left < 0) {
    writePWM(L_RPWM, 0, 0);
    writePWM(L_LPWM, 1, -left);
  } else {
    writePWM(L_RPWM, 0, 0);
    writePWM(L_LPWM, 1, 0);
  }

  // Right BTS7960 Driver (Channels 2 & 3)
  if (right > 0) {
    writePWM(R_LPWM, 3, 0);
    writePWM(R_RPWM, 2, right);
  } else if (right < 0) {
    writePWM(R_RPWM, 2, 0);
    writePWM(R_LPWM, 3, -right);
  } else {
    writePWM(R_RPWM, 2, 0);
    writePWM(R_LPWM, 3, 0);
  }

  moving = (left != 0 || right != 0);
}

// ---------------- SINGLE-CHANNEL RAMP HELPER -------
int stepValue(int current, int target) {
  if (current == target) return current;

  bool reversing = (current > 0 && target < 0) || (current < 0 && target > 0);
  bool decelerating = (abs(target) < abs(current)) || reversing;
  int step = decelerating ? RAMP_STEP_DECEL : RAMP_STEP_ACCEL;

  // Zero-crossing protection: pause at 0 before reversing polarity
  if (reversing && abs(current) <= step) {
    return 0;
  }

  if (target > current) {
    int next = current + step;
    return (next > target) ? target : next;
  } else {
    int next = current - step;
    return (next < target) ? target : next;
  }
}

// ---------------- NON-BLOCKING RAMP UPDATER --------
void updateMotorRamp() {
  unsigned long now = millis();
  if (now - lastRampTime < RAMP_INTERVAL_MS) return;
  lastRampTime = now;

  if (currentLeft != targetLeft || currentRight != targetRight) {
    currentLeft = stepValue(currentLeft, targetLeft);
    currentRight = stepValue(currentRight, targetRight);
    writeMotorHardware(currentLeft, currentRight);
  }
}

// ---------------- IMMEDIATE EMERGENCY STOP ---------
void stopMotorsImmediate() {
  targetLeft = 0;
  targetRight = 0;
  currentLeft = 0;
  currentRight = 0;
  writeMotorHardware(0, 0);
}

// ---------------- SET TARGET MOTOR SPEEDS ----------
void setTargetMotors(int left, int right) {
  targetLeft = constrain(left, -255, 255);
  targetRight = constrain(right, -255, 255);
}

// ---------------- APPLY COMMAND --------------------
void applyCommand() {
  if (motorSpeed == 0 || currentCommand == 'S') {
    stopMotorsImmediate();
    return;
  }

  int s = constrain(motorSpeed, 0, 255);

  switch (currentCommand) {
    case 'F':
      setTargetMotors(s, s);
      break;
    case 'B':
      setTargetMotors(-s, -s);
      break;
    case 'L':
      setTargetMotors(-s, s);
      break;
    case 'R':
      setTargetMotors(s, -s);
      break;
    default:
      currentCommand = 'S';
      stopMotorsImmediate();
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
  if (c != 'F' && c != 'B' && c != 'L' && c != 'R' && c != 'S') {
    server.send(400, "text/plain", "Unknown command");
    return;
  }

  currentCommand = c;
  lastCommandTime = millis();

  if (c == 'S') {
    stopMotorsImmediate();
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
    if (value.charAt(i) < '0' || value.charAt(i) > '9') {
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
  applyCommand();

  server.send(200, "text/plain", "Speed updated");
}

// ---------------- STATUS HANDLER -------------------
void handleStatus() {
  String json = "{\"cmd\":\"";
  json += currentCommand;
  json += "\",\"speed\":";
  json += String(motorSpeed);
  json += ",\"currentLeft\":";
  json += String(currentLeft);
  json += ",\"currentRight\":";
  json += String(currentRight);
  json += ",\"moving\":";
  json += (moving ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

// ---------------- WEBPAGE --------------------------
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="theme-color" content="#0b0e15">
<title>RoboRacer BTS7960 Controller</title>

<style>
:root {
  --bg:#0b0e15;
  --panel:#111722;
  --border:#202b3d;
  --blue:#1678ff;
  --blue-light:#75b4ff;
  --green:#00e676;
  --text:#f5f7ff;
  --muted:#9baac1;
  --red:#f21c3b;
}

* {
  box-sizing:border-box;
  -webkit-tap-highlight-color:transparent;
  user-select:none;
  -webkit-user-select:none;
}

html,body {
  margin:0;
  min-height:100%;
  background:var(--bg);
}

body {
  min-height:100vh;
  min-height:100dvh;
  background:radial-gradient(ellipse at 50% 0%,#172131,transparent 65%),var(--bg);
  color:var(--text);
  font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;
}

button,input {font:inherit}
button {cursor:pointer}

.app {
  width:100%;
  max-width:1400px;
  min-height:100vh;
  min-height:100dvh;
  margin:auto;
  padding:clamp(8px,2.5vw,32px);
  display:flex;
  align-items:center;
  justify-content:center;
}

.controller {
  width:100%;
  overflow:hidden;
  border:1px solid var(--border);
  border-radius:26px;
  background:linear-gradient(145deg,#151c28,#0e131d);
  box-shadow:0 24px 80px #0008,inset 0 1px 0 #ffffff0a;
}

.topbar {
  min-height:84px;
  padding:16px 26px;
  border-bottom:1px solid #1a2333;
  display:flex;
  align-items:center;
  justify-content:space-between;
  gap:16px;
  flex-wrap:wrap;
}

.brand {
  display:flex;
  align-items:center;
  gap:12px;
}

.car-icon {
  font-size:32px;
  filter:drop-shadow(0 0 10px #1678ff55);
}

.brand h1 {
  margin:0;
  font-size:clamp(22px,3vw,34px);
  font-weight:900;
  letter-spacing:-1px;
}

.brand h1 span {color:var(--blue)}

.telemetry {
  display:flex;
  align-items:center;
  gap:10px;
  flex-wrap:wrap;
}

.badge {
  padding:6px 12px;
  border-radius:999px;
  background:#182232;
  border:1px solid #28364f;
  color:var(--muted);
  font-size:12px;
  font-weight:800;
  letter-spacing:.6px;
}

.badge strong {color:var(--text)}
.badge.softstart {border-color:#1e4631;color:#6ee7b7}

.connection {
  display:flex;
  align-items:center;
  gap:9px;
  padding:6px 13px;
  border-radius:999px;
  background:#151d2b;
  border:1px solid #243047;
  color:var(--muted);
  font-size:12px;
  font-weight:800;
  letter-spacing:.7px;
}

.connection-dot {
  width:10px;
  height:10px;
  flex-shrink:0;
  border-radius:50%;
  background:#8792a5;
}

.connection.online .connection-dot {
  background:var(--green);
  box-shadow:0 0 12px #00e67699;
}

.connection.error .connection-dot {
  background:var(--red);
  box-shadow:0 0 12px #f21c3b88;
}

.controls {
  padding:clamp(20px,4.5vw,52px) clamp(16px,5vw,80px);
  display:grid;
  grid-template-columns:1fr 1.6fr;
  align-items:center;
  justify-items:center;
  gap:clamp(16px,4vw,55px);
}

.vertical-arrows {
  width:100%;
  display:flex;
  flex-direction:column;
  align-items:center;
  gap:clamp(14px,3vw,26px);
}

.horizontal-arrows {
  width:100%;
  display:flex;
  align-items:center;
  justify-content:center;
  gap:clamp(14px,4vw,44px);
}

.arrow {
  display:flex;
  flex-direction:column;
  align-items:center;
  justify-content:center;
  padding:0;
  color:#e9efff;
  background:linear-gradient(145deg,#202c40,#121a28);
  border:2px solid #070b12;
  border-radius:18px;
  box-shadow:0 6px 0 #070b12,inset 0 1px 0 #ffffff14;
  touch-action:none;
  transition:background .08s,color .08s,transform .08s,box-shadow .08s;
}

.arrow small {
  font-size:11px;
  font-weight:800;
  letter-spacing:1px;
  color:var(--muted);
  margin-top:4px;
}

.arrow-up,.arrow-down {
  width:clamp(88px,17vw,148px);
  height:clamp(88px,17vw,148px);
  font-size:clamp(46px,8vw,82px);
  line-height:1;
}

.arrow-left,.arrow-right {
  width:clamp(104px,21vw,210px);
  height:clamp(82px,14vw,132px);
  font-size:clamp(46px,8vw,82px);
  line-height:1;
}

.arrow:active,.arrow.active {
  color:white;
  background:linear-gradient(145deg,#56a1ff,#1269f2);
  border-color:var(--blue);
  box-shadow:0 0 0 3px #1678ff35,0 0 30px #1678ff55;
  transform:translateY(3px);
}

.arrow:active small,.arrow.active small {color:#e0eeff}

.bottom-bar {
  border-top:1px solid var(--border);
  padding:22px clamp(16px,4.5vw,56px);
  display:grid;
  grid-template-columns:minmax(140px,.8fr) 1px minmax(220px,1.2fr);
  align-items:center;
  gap:clamp(18px,4vw,40px);
}

.stop-button {
  min-height:74px;
  width:100%;
  max-width:260px;
  justify-self:center;
  display:flex;
  align-items:center;
  justify-content:center;
  gap:12px;
  padding:14px;
  border:0;
  border-radius:50px;
  background:linear-gradient(120deg,#ff2144,#e9002b);
  color:white;
  font-size:clamp(18px,2.4vw,25px);
  font-weight:900;
  box-shadow:0 7px 22px #ed153533,inset 0 1px 0 #ffffff35;
  touch-action:manipulation;
}

.stop-button:active {
  transform:scale(.97);
  background:#c90025;
}

.stop-square {
  width:18px;
  height:18px;
  background:white;
  border-radius:3px;
}

.divider {
  width:1px;
  height:75px;
  background:#293448;
}

.speed-area {min-width:0;width:100%}

.speed-heading {
  display:flex;
  justify-content:space-between;
  align-items:baseline;
  gap:10px;
  margin-bottom:10px;
}

.speed-title {
  color:var(--muted);
  font-size:14px;
  font-weight:900;
  letter-spacing:.7px;
}

.speed-number {
  color:var(--blue-light);
  font-size:clamp(22px,3vw,32px);
  font-weight:900;
  font-variant-numeric:tabular-nums;
}

.speed-pct {
  font-size:14px;
  color:var(--muted);
  margin-left:4px;
}

.slider-wrap {padding:6px 0 10px}

#speedSlider {
  display:block;
  width:100%;
  height:12px;
  margin:0;
  border-radius:20px;
  outline:none;
  background:#253248;
  appearance:none;
  -webkit-appearance:none;
  cursor:pointer;
  touch-action:pan-y;
}

#speedSlider::-webkit-slider-thumb {
  appearance:none;
  -webkit-appearance:none;
  width:30px;
  height:30px;
  border-radius:50%;
  background:var(--blue);
  border:2px solid #75b4ff;
  box-shadow:0 0 16px #1678ff77;
}

#speedSlider::-moz-range-thumb {
  width:26px;
  height:26px;
  border-radius:50%;
  background:var(--blue);
  border:2px solid #75b4ff;
}

.speed-labels {
  display:flex;
  justify-content:space-between;
  margin-top:8px;
  color:#8594ab;
  font-size:11px;
  font-weight:700;
}

.presets {
  display:grid;
  grid-template-columns:repeat(3,1fr);
  gap:8px;
  margin-top:14px;
}

.preset {
  min-height:38px;
  border:1px solid #344158;
  border-radius:9px;
  background:#1a2434;
  color:#d8e3f5;
  font-size:12px;
  font-weight:900;
  touch-action:manipulation;
}

.preset.selected {
  background:#155ac0;
  border-color:#3288ff;
  color:white;
}

.preset:active {transform:scale(.97)}

.footer {
  padding:0 16px 14px;
  text-align:center;
  color:#65758d;
  font-size:11px;
}

@media(max-width:650px) {
  .app {padding:10px;align-items:flex-start}
  .controller {border-radius:20px}
  .topbar {min-height:70px;padding:14px 14px;justify-content:center}
  .controls {
    padding:22px 12px 26px;
    grid-template-columns:.85fr 1.15fr;
    gap:10px;
  }
  .vertical-arrows {gap:16px}
  .horizontal-arrows {gap:10px}
  .arrow-up,.arrow-down {
    width:clamp(74px,21vw,112px);
    height:clamp(76px,22vw,115px);
    font-size:clamp(40px,11vw,64px);
  }
  .arrow-left,.arrow-right {
    width:clamp(76px,22vw,120px);
    height:clamp(66px,18vw,100px);
    font-size:clamp(38px,10vw,64px);
  }
  .bottom-bar {
    padding:18px 15px;
    grid-template-columns:1fr;
    gap:16px;
  }
  .stop-button {min-height:56px;max-width:none}
  .divider {width:100%;height:1px}
}
</style>
</head>

<body>
<div class="app">
  <div class="controller">

    <header class="topbar">
      <div class="brand">
        <div class="car-icon">&#127950;</div>
        <h1>Robo<span>Racer</span></h1>
      </div>

      <div class="telemetry">
        <div class="badge softstart">SOFT-START: <strong>ON</strong></div>
        <div class="badge">STATE: <strong id="driveState">STOPPED</strong></div>
        <div class="connection" id="connection">
          <span class="connection-dot"></span>
          <span id="statusText">READY</span>
        </div>
      </div>
    </header>

    <main class="controls">
      <div class="vertical-arrows">
        <button class="arrow arrow-up" data-c="F" aria-label="Forward">
          &uarr;<small>FWD</small>
        </button>
        <button class="arrow arrow-down" data-c="B" aria-label="Backward">
          &darr;<small>REV</small>
        </button>
      </div>

      <div class="horizontal-arrows">
        <button class="arrow arrow-left" data-c="L" aria-label="Left">
          &larr;<small>LEFT</small>
        </button>
        <button class="arrow arrow-right" data-c="R" aria-label="Right">
          &rarr;<small>RIGHT</small>
        </button>
      </div>
    </main>

    <section class="bottom-bar">
      <button class="stop-button" id="stopButton">
        <span class="stop-square"></span>
        STOP
      </button>

      <div class="divider"></div>

      <div class="speed-area">
        <div class="speed-heading">
          <span class="speed-title">TARGET PWM SPEED</span>
          <div>
            <span class="speed-number" id="speedValue">170</span>
            <span class="speed-pct" id="speedPct">(67%)</span>
          </div>
        </div>

        <div class="slider-wrap">
          <input id="speedSlider" type="range" min="0" max="255" value="170" aria-label="Motor speed">
        </div>

        <div class="speed-labels">
          <span>0 (OFF)</span>
          <span>128 (50%)</span>
          <span>255 (100%)</span>
        </div>

        <div class="presets">
          <button class="preset" data-speed="100">LOW (100)</button>
          <button class="preset selected" data-speed="170">MED (170)</button>
          <button class="preset" data-speed="255">MAX (255)</button>
        </div>
      </div>
    </section>

    <div class="footer">
      Soft-Start Ramp Active &middot; Hold direction or WASD / Arrow Keys &middot; Release to stop
    </div>

  </div>
</div>

<script>
'use strict';

const connection = document.getElementById('connection');
const statusText = document.getElementById('statusText');
const driveState = document.getElementById('driveState');
const slider = document.getElementById('speedSlider');
const speedValue = document.getElementById('speedValue');
const speedPct = document.getElementById('speedPct');

const stateNames = {
  F: 'RAMPING FWD',
  B: 'RAMPING REV',
  L: 'SPIN LEFT',
  R: 'SPIN RIGHT',
  S: 'STOPPED'
};

let held = null;
let pointerId = null;
let heartbeat = null;
let speedTimer = null;
let speed = 170;
let stopped = true;
let requestChain = Promise.resolve();
let commandGeneration = 0;
let stopGeneration = 0;

function setStatus(state, text) {
  connection.classList.remove('online', 'error');
  if (state === 'online') connection.classList.add('online');
  if (state === 'error') connection.classList.add('error');
  statusText.textContent = text;
}

function enqueueRequest(path) {
  const task = requestChain.then(async () => {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 500);
    try {
      const response = await fetch(path, {
        method: 'GET',
        cache: 'no-store',
        signal: controller.signal
      });
      if (!response.ok) throw new Error('HTTP ' + response.status);
      return response;
    } finally {
      clearTimeout(timeout);
    }
  });

  requestChain = task.catch(() => {});
  return task;
}

function sendCommand(command) {
  const generation = commandGeneration;
  const thisStopGeneration = stopGeneration;
  driveState.textContent = stateNames[command] || 'STOPPED';

  enqueueRequest('/cmd?c=' + command)
    .then(() => {
      if (generation === commandGeneration && thisStopGeneration === stopGeneration) {
        setStatus('online', 'CONNECTED');
      }
    })
    .catch(() => {
      if (generation === commandGeneration && thisStopGeneration === stopGeneration) {
        setStatus('error', 'LINK LOST');
      }
    });
}

function sendSpeed(value) {
  const generation = commandGeneration;

  enqueueRequest('/speed?v=' + value)
    .then(() => {
      if (generation === commandGeneration) {
        setStatus('online', 'CONNECTED');
      }
    })
    .catch(() => {
      if (generation === commandGeneration) {
        setStatus('error', 'SPEED ERR');
      }
    });
}

function startHeartbeat() {
  stopHeartbeat();
  heartbeat = setInterval(() => {
    if (held !== null && !stopped) {
      sendCommand(held);
    }
  }, 240);
}

function stopHeartbeat() {
  if (heartbeat !== null) {
    clearInterval(heartbeat);
    heartbeat = null;
  }
}

function clearArrowHighlights() {
  document.querySelectorAll('[data-c]').forEach(button => {
    button.classList.remove('active');
  });
}

function stopAll() {
  commandGeneration++;
  stopGeneration++;

  held = null;
  pointerId = null;
  stopped = true;

  stopHeartbeat();
  clearArrowHighlights();
  sendCommand('S');
}

document.querySelectorAll('[data-c]').forEach(button => {
  button.addEventListener('pointerdown', event => {
    event.preventDefault();
    if (held !== null) return;

    commandGeneration++;
    held = button.dataset.c;
    pointerId = event.pointerId;
    stopped = false;

    button.classList.add('active');

    try {
      button.setPointerCapture(event.pointerId);
    } catch (e) {}

    sendCommand(held);
    startHeartbeat();
  });

  const releaseHandler = event => {
    if (event.pointerId !== pointerId) return;
    stopAll();
  };

  button.addEventListener('pointerup', releaseHandler);
  button.addEventListener('pointercancel', releaseHandler);
  button.addEventListener('lostpointercapture', releaseHandler);
});

window.addEventListener('pointerup', event => {
  if (pointerId !== null && event.pointerId === pointerId) stopAll();
});

window.addEventListener('pointercancel', event => {
  if (pointerId !== null && event.pointerId === pointerId) stopAll();
});

const keyMap = {
  ArrowUp: 'F', KeyW: 'F',
  ArrowDown: 'B', KeyS: 'B',
  ArrowLeft: 'L', KeyA: 'L',
  ArrowRight: 'R', KeyD: 'R'
};

window.addEventListener('keydown', event => {
  if (event.code === 'Space') {
    event.preventDefault();
    stopAll();
    return;
  }
  const cmd = keyMap[event.code];
  if (!cmd || event.repeat || held !== null) return;

  commandGeneration++;
  held = cmd;
  stopped = false;

  const btn = document.querySelector('[data-c="' + cmd + '"]');
  if (btn) btn.classList.add('active');

  sendCommand(held);
  startHeartbeat();
});

window.addEventListener('keyup', event => {
  const cmd = keyMap[event.code];
  if (cmd && cmd === held) stopAll();
});

document.getElementById('stopButton').addEventListener('click', stopAll);

function updateSpeed(value) {
  speed = Math.max(0, Math.min(255, Number(value)));
  speedValue.textContent = speed;
  speedPct.textContent = '(' + Math.round((speed / 255) * 100) + '%)';

  document.querySelectorAll('[data-speed]').forEach(button => {
    button.classList.toggle('selected', Number(button.dataset.speed) === speed);
  });

  if (speedTimer !== null) clearTimeout(speedTimer);

  speedTimer = setTimeout(() => {
    sendSpeed(speed);
    speedTimer = null;
  }, 50);
}

slider.addEventListener('input', event => {
  updateSpeed(event.target.value);
});

document.querySelectorAll('[data-speed]').forEach(button => {
  button.addEventListener('click', () => {
    slider.value = button.dataset.speed;
    updateSpeed(button.dataset.speed);
  });
});

window.addEventListener('blur', stopAll);
document.addEventListener('visibilitychange', () => {
  if (document.hidden) stopAll();
});
window.addEventListener('pagehide', stopAll);

setStatus('', 'READY');
</script>
</body>
</html>
)rawliteral";

// ---------------- SETUP ---------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("\n--- RoboRacer ESP32 + Dual BTS7960 (Soft-Start) ---");

  setupPWM();

  currentCommand = 'S';
  stopMotorsImmediate();

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  bool apStarted = WiFi.softAP(ssid, password);

  if (apStarted) {
    Serial.println("RoboRacer Wi-Fi AP started!");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("Controller URL: http://");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Wi-Fi AP startup failed!");
  }

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", webpage);
  });

  server.on("/cmd", HTTP_GET, handleCommand);
  server.on("/speed", HTTP_GET, handleSpeed);
  server.on("/status", HTTP_GET, handleStatus);

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  lastCommandTime = millis();
  lastRampTime = millis();

  Serial.println("Web server & soft-start controller ready.");
}

// ---------------- MAIN LOOP -------------------------
void loop() {
  server.handleClient();

  if (currentCommand != 'S' && (millis() - lastCommandTime >= TIMEOUT_MS)) {
    currentCommand = 'S';
    stopMotorsImmediate();
    Serial.println("Safety timeout: Motors stopped");
  }

  updateMotorRamp();
}
