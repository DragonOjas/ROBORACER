
/*
  OLL RoboRacer - ESP32 Wi-Fi RC Car
  Two BTS7960 / IBT-2 motor drivers
  Board: ESP32 DevKit V1 / ESP32-WROOM-32
  Arduino-ESP32 core: 2.x or 3.x

  Wi-Fi:
    SSID: IIT_KANPUR_ROBO_RACER
    Password: roboracer121321
    URL: http://192.168.4.1

  LATEST PIN MAPPING
    LEFT DRIVER:
      RPWM -> GPIO25
      LPWM -> GPIO26
      R_EN -> GPIO32
      L_EN -> GPIO33

    RIGHT DRIVER:
      RPWM -> GPIO27
      LPWM -> GPIO14
      R_EN -> GPIO18
      L_EN -> GPIO19

  IMPORTANT:
    ESP32 GND, both driver logic grounds, and battery negative
    must share a common ground.

    Connect motor battery power only to the driver's motor
    power terminals. Verify the logic VCC requirements of
    your actual BTS7960 module before connecting it.
*/

#include <WiFi.h>
#include <WebServer.h>

// ---------------- Wi-Fi settings ----------------

const char* AP_SSID = "IIT_KANPUR_ROBO_RACER";
const char* AP_PASSWORD = "roboracer121321";

// ---------------- LEFT BTS7960 ------------------

#define L_RPWM 25
#define L_LPWM 26
#define L_R_EN 32
#define L_L_EN 33

// ---------------- RIGHT BTS7960 -----------------

#define R_RPWM 27
#define R_LPWM 14
#define R_R_EN 18
#define R_L_EN 19

// Change to true if that motor's direction is reversed.
const bool INVERT_LEFT = false;
const bool INVERT_RIGHT = false;

// ---------------- PWM settings ------------------

const int PWM_FREQ = 5000;
const int PWM_RES = 8;

// ---------------- Safety settings ----------------

const unsigned long COMMAND_TIMEOUT_MS = 1200;

// ---------------- State --------------------------

WebServer server(80);

char currentDir = 'S';
int currentSpeed = 170;
unsigned long lastCommandTime = 0;

bool motorsReady = false;

// ---------------- PWM pin array -----------------

const int PWM_PINS[4] = {
  L_RPWM, L_LPWM, R_RPWM, R_LPWM
};

const int EN_PINS[4] = {
  L_R_EN, L_L_EN, R_R_EN, R_L_EN
};

bool pwmReady[4] = {
  false, false, false, false
};

// ---------------- HTML controller ----------------
// Original controller layout and features retained.
// JavaScript request handling is improved below.

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport"
 content="width=device-width,initial-scale=1,maximum-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0b0e15">
<title>RoboRacer Controller</title>

<style>
:root {
  --bg:#0b0e15;
  --panel:#111722;
  --border:#202b3d;
  --blue:#1678ff;
  --blue-light:#75b4ff;
  --text:#f5f7ff;
  --muted:#9baac1;
  --red:#f21c3b;
}
* {
  box-sizing:border-box;
  -webkit-tap-highlight-color:transparent;
  -webkit-user-select:none;
  user-select:none;
}
html,body {
  margin:0;
  min-height:100%;
  background:var(--bg);
  overscroll-behavior:none;
}
body {
  min-height:100vh;
  min-height:100dvh;
  background:radial-gradient(ellipse at 50% 0%,#172131,transparent 65%),var(--bg);
  color:var(--text);
  font-family:Arial,Helvetica,sans-serif;
}
button,input {font:inherit}
button {cursor:pointer}
.app {
  width:100%;
  max-width:1500px;
  min-height:100vh;
  min-height:100dvh;
  margin:auto;
  padding:clamp(10px,3vw,38px);
  display:flex;
  align-items:center;
  justify-content:center;
}
.controller {
  width:100%;
  overflow:hidden;
  border:1px solid var(--border);
  border-radius:28px;
  background:linear-gradient(145deg,#151c28,#0e131d);
  box-shadow:0 24px 80px #0006,inset 0 1px 0 #ffffff08;
}
.topbar {
  min-height:100px;
  padding:20px 28px;
  display:flex;
  align-items:center;
  justify-content:center;
  gap:clamp(12px,3vw,38px);
  flex-wrap:wrap;
}
.brand {
  display:flex;
  align-items:center;
  gap:12px;
}
.car-icon {
  font-size:33px;
  filter:drop-shadow(0 0 10px #1678ff55);
}
.brand h1 {
  margin:0;
  font-size:clamp(23px,3.3vw,39px);
  font-weight:900;
  letter-spacing:-1.4px;
}
.brand h1 span {color:var(--blue)}
.connection {
  display:flex;
  align-items:center;
  gap:10px;
  color:var(--muted);
  font-size:13px;
  font-weight:800;
  letter-spacing:.7px;
}
.connection-dot {
  width:12px;
  height:12px;
  flex-shrink:0;
  border-radius:50%;
  background:#8792a5;
}
.connection.online .connection-dot {
  background:var(--blue);
  box-shadow:0 0 15px #1678ff99;
}
.connection.error .connection-dot {
  background:var(--red);
  box-shadow:0 0 12px #f21c3b88;
}
.controls {
  padding:clamp(22px,5vw,58px) clamp(18px,6vw,100px);
  display:grid;
  grid-template-columns:1fr 1.8fr;
  align-items:center;
  justify-items:center;
  gap:clamp(16px,5vw,65px);
}
.vertical-arrows {
  width:100%;
  display:flex;
  flex-direction:column;
  align-items:center;
  gap:clamp(14px,3vw,28px);
}
.horizontal-arrows {
  width:100%;
  display:flex;
  align-items:center;
  justify-content:center;
  gap:clamp(14px,4vw,50px);
}
.arrow {
  display:flex;
  align-items:center;
  justify-content:center;
  padding:0;
  color:#e9efff;
  background:linear-gradient(145deg,#202c40,#121a28);
  border:2px solid #070b12;
  border-radius:14px;
  box-shadow:0 5px 0 #070b12,inset 0 1px 0 #ffffff12;
  touch-action:none;
  transition:background .08s,color .08s,transform .08s,box-shadow .08s;
}
.arrow-up,.arrow-down {
  width:clamp(86px,17vw,150px);
  height:clamp(86px,17vw,150px);
  font-size:clamp(54px,10vw,100px);
  line-height:1;
}
.arrow-left,.arrow-right {
  width:clamp(105px,22vw,220px);
  height:clamp(78px,14vw,135px);
  font-size:clamp(57px,10vw,98px);
  line-height:1;
}
.arrow:active,.arrow.active {
  color:white;
  background:linear-gradient(145deg,#56a1ff,#1269f2);
  border-color:var(--blue);
  box-shadow:0 0 0 3px #1678ff25,0 0 30px #1678ff4a;
  transform:translateY(3px);
}
.bottom-bar {
  border-top:1px solid var(--border);
  padding:25px clamp(16px,5vw,65px);
  display:grid;
  grid-template-columns:minmax(140px,.8fr) 1px minmax(220px,1.2fr);
  align-items:center;
  gap:clamp(18px,4vw,45px);
}
.stop-button {
  min-height:76px;
  width:100%;
  max-width:270px;
  justify-self:center;
  display:flex;
  align-items:center;
  justify-content:center;
  gap:13px;
  padding:15px;
  border:0;
  border-radius:50px;
  background:linear-gradient(120deg,#ff2144,#e9002b);
  color:white;
  font-size:clamp(18px,2.5vw,27px);
  font-weight:900;
  box-shadow:0 7px 20px #ed153526,inset 0 1px 0 #ffffff35;
  touch-action:manipulation;
}
.stop-button:active {
  transform:scale(.97);
  background:#c90025;
}
.stop-square {
  width:20px;
  height:20px;
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
  align-items:center;
  gap:10px;
  margin-bottom:12px;
}
.speed-title {
  color:var(--muted);
  font-size:15px;
  font-weight:900;
  letter-spacing:.6px;
}
.speed-number {
  color:var(--blue-light);
  font-size:clamp(25px,3.2vw,36px);
  font-weight:900;
  font-variant-numeric:tabular-nums;
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
  border:2px solid #4095ff;
  box-shadow:0 0 16px #1678ff55;
}
#speedSlider::-moz-range-thumb {
  width:26px;
  height:26px;
  border-radius:50%;
  background:var(--blue);
  border:2px solid #4095ff;
}
.speed-labels {
  display:flex;
  justify-content:space-between;
  margin-top:12px;
  color:#8594ab;
  font-size:11px;
}
.presets {
  display:grid;
  grid-template-columns:repeat(3,1fr);
  gap:8px;
  margin-top:17px;
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
  padding:0 16px 16px;
  text-align:center;
  color:#65758d;
  font-size:11px;
}
@media(max-width:650px) {
  .app {padding:10px;align-items:flex-start}
  .controller {border-radius:20px}
  .topbar {min-height:80px;padding:15px 10px;gap:10px 20px}
  .car-icon {font-size:26px}
  .brand h1 {font-size:26px}
  .connection {font-size:11px}
  .controls {
    padding:25px 12px 30px;
    grid-template-columns:.8fr 1.2fr;
    gap:10px;
  }
  .vertical-arrows {gap:18px}
  .horizontal-arrows {gap:10px}
  .arrow-up,.arrow-down {
    width:clamp(72px,21vw,112px);
    height:clamp(76px,22vw,115px);
    font-size:clamp(47px,13vw,70px);
  }
  .arrow-left,.arrow-right {
    width:clamp(75px,22vw,120px);
    height:clamp(64px,18vw,100px);
    font-size:clamp(44px,12vw,72px);
  }
  .bottom-bar {
    padding:20px 15px;
    grid-template-columns:1fr;
    gap:18px;
  }
  .stop-button {min-height:58px;max-width:none}
  .divider {width:100%;height:1px}
  .speed-heading {margin-bottom:8px}
  .speed-area {padding:0 3px}
  .footer {padding-bottom:14px}
}
@media(max-width:360px) {
  .app {padding:5px}
  .controls {padding:20px 5px 25px;gap:5px}
  .arrow-up,.arrow-down {
    width:70px;height:74px;font-size:46px;
  }
  .arrow-left,.arrow-right {
    width:72px;height:60px;font-size:42px;
  }
  .horizontal-arrows {gap:7px}
  .topbar {gap:8px}
  .brand h1 {font-size:23px}
}
@media(orientation:landscape) and (max-height:500px) {
  .app {padding:8px}
  .topbar {min-height:58px;padding:8px}
  .brand h1 {font-size:23px}
  .car-icon {font-size:24px}
  .controls {padding:12px 25px 18px}
  .vertical-arrows {gap:10px}
  .arrow-up,.arrow-down {
    width:min(18vh,100px);
    height:min(18vh,100px);
    font-size:min(12vh,65px);
  }
  .arrow-left,.arrow-right {
    width:min(25vw,180px);
    height:min(16vh,85px);
    font-size:min(12vh,60px);
  }
  .bottom-bar {padding:12px 25px}
  .stop-button {min-height:52px}
  .divider {height:55px}
  .presets {margin-top:8px}
  .preset {min-height:30px}
  .footer {display:none}
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
      <div class="connection" id="connection">
        <span class="connection-dot"></span>
        <span id="statusText">CONNECTING</span>
      </div>
    </header>

    <main class="controls">
      <div class="vertical-arrows">
        <button class="arrow arrow-up" data-c="F" aria-label="Forward">↑</button>
        <button class="arrow arrow-down" data-c="B" aria-label="Backward">↓</button>
      </div>
      <div class="horizontal-arrows">
        <button class="arrow arrow-left" data-c="L" aria-label="Left">←</button>
        <button class="arrow arrow-right" data-c="R" aria-label="Right">→</button>
      </div>
    </main>

    <section class="bottom-bar">
      <button class="stop-button" id="stopButton">
        <span class="stop-square"></span>STOP
      </button>
      <div class="divider"></div>
      <div class="speed-area">
        <div class="speed-heading">
          <span class="speed-title">SPEED</span>
          <span class="speed-number" id="speedValue">170</span>
        </div>
        <div class="slider-wrap">
          <input id="speedSlider" type="range" min="0" max="255"
            value="170" aria-label="Motor speed">
        </div>
        <div class="speed-labels">
          <span>SLOW</span><span>MEDIUM</span><span>FAST</span>
        </div>
        <div class="presets">
          <button class="preset" data-speed="100">LOW</button>
          <button class="preset selected" data-speed="170">MED</button>
          <button class="preset" data-speed="255">MAX</button>
        </div>
      </div>
    </section>
    <div class="footer">Hold a direction to drive · Release to stop</div>
  </div>
</div>

<script>
'use strict';

const connection = document.getElementById('connection');
const statusText = document.getElementById('statusText');
const slider = document.getElementById('speedSlider');
const speedValue = document.getElementById('speedValue');

let held = null;
let pointerId = null;
let heartbeat = null;
let speedTimer = null;
let speed = 170;
let stopped = true;
let requestChain = Promise.resolve();
let pending = 0;
let commandGeneration = 0;
let stopGeneration = 0;

function setStatus(state, text) {
  connection.classList.remove('online', 'error');
  if (state === 'online') connection.classList.add('online');
  if (state === 'error') connection.classList.add('error');
  statusText.textContent = text;
}

// Serialize control requests so older movement commands cannot
// overtake STOP. Every individual request has a timeout.
function enqueueRequest(path) {
  pending++;

  const task = requestChain.then(async () => {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 1000);

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
  task.then(
    () => { pending--; },
    () => { pending--; }
  );

  return task;
}

function sendCommand(command) {
  const generation = commandGeneration;
  const thisStopGeneration = stopGeneration;

  enqueueRequest('/cmd?c=' + command)
    .then(() => {
      if (generation === commandGeneration &&
          thisStopGeneration === stopGeneration) {
        setStatus('online', 'CONNECTED');
      }
    })
    .catch(() => {
      if (generation === commandGeneration &&
          thisStopGeneration === stopGeneration) {
        setStatus('error', 'CONNECTION ERROR');
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
        setStatus('error', 'SPEED ERROR');
      }
    });
}

function startHeartbeat() {
  stopHeartbeat();

  heartbeat = setInterval(() => {
    if (held !== null && !stopped && pending === 0) {
      sendCommand(held);
    }
  }, 250);
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

  // STOP follows already queued control requests.
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
    } catch (_) {}

    sendCommand(held);
    startHeartbeat();
  });

  button.addEventListener('pointerup', event => {
    if (event.pointerId === pointerId) stopAll();
  });

  button.addEventListener('pointercancel', event => {
    if (event.pointerId === pointerId) stopAll();
  });

  button.addEventListener('lostpointercapture', event => {
    if (event.pointerId === pointerId) stopAll();
  });

  button.addEventListener('contextmenu', event => event.preventDefault());
});

window.addEventListener('pointerup', event => {
  if (pointerId !== null && event.pointerId === pointerId) stopAll();
});

window.addEventListener('pointercancel', event => {
  if (pointerId !== null && event.pointerId === pointerId) stopAll();
});

document.getElementById('stopButton').addEventListener(
  'pointerdown', event => {
    event.preventDefault();
    stopAll();
  }
);

function updateSpeed(value) {
  speed = Math.max(0, Math.min(255, Number(value)));
  speedValue.textContent = speed;

  document.querySelectorAll('[data-speed]').forEach(button => {
    button.classList.toggle(
      'selected', Number(button.dataset.speed) === speed
    );
  });

  if (speedTimer !== null) clearTimeout(speedTimer);

  speedTimer = setTimeout(() => {
    sendSpeed(speed);
    speedTimer = null;
  }, 60);
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

function pingRobot() {
  if (held !== null || pending !== 0) return;

  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 1000);

  fetch('/ping', {
    cache: 'no-store',
    signal: controller.signal
  })
    .then(response => {
      if (!response.ok) throw new Error('HTTP ' + response.status);
      if (held === null) setStatus('online', 'CONNECTED');
    })
    .catch(() => {
      if (held === null) setStatus('error', 'CONNECTION ERROR');
    })
    .finally(() => clearTimeout(timeout));
}

setInterval(pingRobot, 3000);
sendSpeed(speed);
</script>
</body>
</html>
)rawliteral";

// ---------------- PWM initialization ----------------

bool setupPwmPin(int index) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && \
    (ESP_ARDUINO_VERSION_MAJOR >= 3)

  pwmReady[index] = ledcAttach(PWM_PINS[index], PWM_FREQ, PWM_RES);

#else

  double actualFrequency = ledcSetup(index, PWM_FREQ, PWM_RES);

  if (actualFrequency > 0) {
    ledcAttachPin(PWM_PINS[index], index);
    pwmReady[index] = true;
  } else {
    pwmReady[index] = false;
  }

#endif

  if (!pwmReady[index]) {
    Serial.print("PWM initialization failed on GPIO ");
    Serial.println(PWM_PINS[index]);
  }

  return pwmReady[index];
}

void writePwm(int pin, int duty) {
  duty = constrain(duty, 0, 255);

  for (int i = 0; i < 4; i++) {
    if (PWM_PINS[i] != pin) continue;
    if (!pwmReady[i]) return;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && \
    (ESP_ARDUINO_VERSION_MAJOR >= 3)

    ledcWrite(pin, duty);

#else

    ledcWrite(i, duty);

#endif
    return;
  }
}

// ---------------- Motor control ----------------

// power: -255 to +255
// Positive = RPWM; negative = LPWM.
void driveMotor(int rpwm, int lpwm, int power, bool invert) {
  if (invert) power = -power;

  power = constrain(power, -255, 255);

  if (power > 0) {
    writePwm(lpwm, 0);
    writePwm(rpwm, power);
  } else if (power < 0) {
    writePwm(rpwm, 0);
    writePwm(lpwm, -power);
  } else {
    writePwm(rpwm, 0);
    writePwm(lpwm, 0);
  }
}

void stopMotors() {
  // Disable PWM outputs first.
  for (int i = 0; i < 4; i++) {
    writePwm(PWM_PINS[i], 0);
  }

  currentDir = 'S';
}

void applyMovement(char dir, int motorSpeed) {
  if (!motorsReady) return;

  int leftPower = 0;
  int rightPower = 0;

  switch (dir) {
    case 'F':
      leftPower = motorSpeed;
      rightPower = motorSpeed;
      break;

    case 'B':
      leftPower = -motorSpeed;
      rightPower = -motorSpeed;
      break;

    case 'L':
      leftPower = -motorSpeed;
      rightPower = motorSpeed;
      break;

    case 'R':
      leftPower = motorSpeed;
      rightPower = -motorSpeed;
      break;

    default:
      stopMotors();
      return;
  }

  driveMotor(L_RPWM, L_LPWM, leftPower, INVERT_LEFT);
  driveMotor(R_RPWM, R_LPWM, rightPower, INVERT_RIGHT);

  currentDir = dir;
}

// ---------------- Motor initialization ----------------

void initMotors() {
  // Keep both drivers disabled during initialization.
  motorsReady = false;

  for (int i = 0; i < 4; i++) {
    pinMode(EN_PINS[i], OUTPUT);
    digitalWrite(EN_PINS[i], LOW);
  }

  bool allReady = true;

  for (int i = 0; i < 4; i++) {
    if (!setupPwmPin(i)) {
      allReady = false;
    }
  }

  if (!allReady) {
    Serial.println("ERROR: PWM setup failed.");
    Serial.println("Motor drivers remain disabled.");
    return;
  }

  for (int i = 0; i < 4; i++) {
    writePwm(PWM_PINS[i], 0);
  }

  // Enable only after all PWM outputs have initialized.
  for (int i = 0; i < 4; i++) {
    digitalWrite(EN_PINS[i], HIGH);
  }

  motorsReady = true;
  currentDir = 'S';

  Serial.println("Motor PWM initialized successfully.");
}

// ---------------- HTTP helpers ----------------

void sendText(int code, const char* message) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "text/plain", message);
}

bool parseIntStrict(const String& value, int& result) {
  if (value.length() == 0 || value.length() > 3) {
    return false;
  }

  for (size_t i = 0; i < value.length(); i++) {
    if (!isDigit(value[i])) return false;
  }

  result = value.toInt();
  return true;
}

// ---------------- HTTP handlers ----------------

void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html", INDEX_HTML);
}

void handlePing() {
  sendText(200, "OK");
}

void handleCmd() {
  if (!server.hasArg("c")) {
    sendText(400, "Missing c parameter");
    return;
  }

  String value = server.arg("c");

  if (value.length() != 1) {
    sendText(400, "Invalid command");
    return;
  }

  char cmd = value[0];

  if (cmd == 'S') {
    stopMotors();
    lastCommandTime = millis();
    sendText(200, "OK");
    return;
  }

  if (cmd != 'F' && cmd != 'B' &&
      cmd != 'L' && cmd != 'R') {
    sendText(400, "Use F, B, L, R or S");
    return;
  }

  if (!motorsReady) {
    sendText(503, "Motor PWM is not ready");
    return;
  }

  applyMovement(cmd, currentSpeed);
  lastCommandTime = millis();

  sendText(200, "OK");
}

void handleSpeed() {
  if (!server.hasArg("v")) {
    sendText(400, "Missing v parameter");
    return;
  }

  int newSpeed = 0;

  if (!parseIntStrict(server.arg("v"), newSpeed) ||
      newSpeed < 0 || newSpeed > 255) {
    sendText(400, "Speed must be 0-255");
    return;
  }

  currentSpeed = newSpeed;

  // Update the motor PWM immediately while driving.
  if (currentDir != 'S') {
    if (!motorsReady) {
      sendText(503, "Motor PWM is not ready");
      return;
    }

    applyMovement(currentDir, currentSpeed);
    lastCommandTime = millis();
  }

  sendText(200, "OK");
}

void handleNotFound() {
  sendText(404, "Not found");
}

// ---------------- Setup ----------------

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("=== OLL RoboRacer ===");

  initMotors();

  WiFi.mode(WIFI_AP);

  bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD);

  if (apStarted) {
    Serial.println("Wi-Fi access point started.");
    Serial.print("SSID: ");
    Serial.println(AP_SSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("ERROR: Wi-Fi access point failed.");
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/ping", HTTP_GET, handlePing);
  server.on("/cmd", HTTP_GET, handleCmd);
  server.on("/speed", HTTP_GET, handleSpeed);

  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("HTTP server started on port 80.");
  Serial.println("Connect to the robot Wi-Fi and open:");
  Serial.println("http://192.168.4.1");
}

// ---------------- Main loop ----------------

void loop() {
  server.handleClient();

  // Stop if movement commands stop arriving.
  if (motorsReady &&
      currentDir != 'S' &&
      millis() - lastCommandTime > COMMAND_TIMEOUT_MS) {

    stopMotors();

    Serial.println("Safety stop: command timeout.");
  }
}
