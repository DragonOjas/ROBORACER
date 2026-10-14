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

// ---------------- MOTOR PINS -----------------------
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
    case 'F':
      setMotors(s, s);
      break;

    case 'B':
      setMotors(-s, -s);
      break;

    case 'L':
      setMotors(-s, s);
      break;

    case 'R':
      setMotors(s, -s);
      break;

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

// ---------------- WEBPAGE --------------------------
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="theme-color" content="#0b0e15">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<title>RoboRacer Controller</title>

<style>
:root {
  --bg:#0b0e15;
  --panel:#131a26;
  --border:#222f44;
  --blue:#1678ff;
  --blue-light:#75b4ff;
  --text:#f5f7ff;
  --muted:#9baac1;
  --red:#f21c3b;
}

* {
  box-sizing:border-box;
  -webkit-tap-highlight-color:transparent;
  user-select:none;
  -webkit-user-select:none;
  -webkit-touch-callout:none;
}

html,body {
  margin:0;
  padding:0;
  width:100%;
  height:100%;
  overflow:hidden;
  overscroll-behavior:none;
  background:var(--bg);
  touch-action:none;
}

body {
  height:100vh;
  height:100dvh;
  background:radial-gradient(ellipse at 50% 0%,#172234,transparent 70%),var(--bg);
  color:var(--text);
  font-family:system-ui,-apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif;
}

button,input {font:inherit}
button {cursor:pointer}

.app {
  width:100%;
  height:100%;
  max-width:1200px;
  margin:0 auto;
  padding:
    max(8px, env(safe-area-inset-top))
    max(10px, env(safe-area-inset-right))
    max(8px, env(safe-area-inset-bottom))
    max(10px, env(safe-area-inset-left));
  display:flex;
  align-items:center;
  justify-content:center;
}

.controller {
  width:100%;
  height:100%;
  display:flex;
  flex-direction:column;
  justify-content:space-between;
  border:1px solid var(--border);
  border-radius:22px;
  background:linear-gradient(145deg,#151c28,#0d121b);
  box-shadow:0 16px 50px #0008,inset 0 1px 0 #ffffff0a;
  overflow:hidden;
}

/* --- TOP HEADER --- */
.topbar {
  padding:10px 16px;
  border-bottom:1px solid #1b2536;
  display:flex;
  align-items:center;
  justify-content:space-between;
  gap:10px;
  flex-shrink:0;
}

.brand {
  display:flex;
  align-items:center;
  gap:8px;
}

.car-icon {
  font-size:24px;
  filter:drop-shadow(0 0 8px #1678ff66);
}

.brand h1 {
  margin:0;
  font-size:clamp(18px,4vw,26px);
  font-weight:900;
  letter-spacing:-0.8px;
}

.brand h1 span {color:var(--blue)}

.status-group {
  display:flex;
  align-items:center;
  gap:10px;
}

.connection {
  display:flex;
  align-items:center;
  gap:7px;
  padding:5px 10px;
  border-radius:99px;
  background:#0c111a;
  border:1px solid #1e2a3d;
  color:var(--muted);
  font-size:11px;
  font-weight:800;
  letter-spacing:.6px;
}

.connection-dot {
  width:9px;
  height:9px;
  flex-shrink:0;
  border-radius:50%;
  background:#8792a5;
}

.connection.online .connection-dot {
  background:#00e676;
  box-shadow:0 0 10px #00e676aa;
}

.connection.error .connection-dot {
  background:var(--red);
  box-shadow:0 0 10px #f21c3baa;
}

.fs-btn {
  width:34px;
  height:34px;
  border-radius:10px;
  border:1px solid #28364f;
  background:#182232;
  color:var(--text);
  font-size:16px;
  display:flex;
  align-items:center;
  justify-content:center;
  touch-action:manipulation;
}

.fs-btn:active {transform:scale(.94)}

/* --- MAIN DRIVE CONTROLS --- */
.controls {
  flex:1;
  min-height:0;
  padding:10px 14px;
  display:grid;
  grid-template-columns:1fr 1.35fr;
  align-items:center;
  justify-items:center;
  gap:12px;
  touch-action:none;
}

.vertical-arrows {
  width:100%;
  height:100%;
  display:flex;
  flex-direction:column;
  align-items:center;
  justify-content:center;
  gap:14px;
  touch-action:none;
}

.horizontal-arrows {
  width:100%;
  height:100%;
  display:flex;
  align-items:center;
  justify-content:center;
  gap:12px;
  touch-action:none;
}

.arrow {
  display:flex;
  align-items:center;
  justify-content:center;
  padding:0;
  color:#e9efff;
  background:linear-gradient(155deg,#223047,#121a28);
  border:2px solid #283852;
  border-radius:20px;
  box-shadow:0 6px 0 #070b12,0 10px 22px #0006,inset 0 1px 0 #ffffff18;
  touch-action:none;
  transition:background .05s,transform .05s,box-shadow .05s,border-color .05s;
}

/* Big ergonomic thumb targets */
.arrow-up,.arrow-down {
  width:100%;
  max-width:150px;
  height:clamp(80px,19vh,145px);
  font-size:clamp(44px,9vw,76px);
  line-height:1;
}

.arrow-left,.arrow-right {
  flex:1;
  max-width:170px;
  height:clamp(92px,22vh,165px);
  font-size:clamp(44px,9vw,76px);
  line-height:1;
}

.arrow.active {
  color:#fff;
  background:linear-gradient(145deg,#4da0ff,#0f62fe);
  border-color:#8ec5ff;
  box-shadow:0 2px 0 #070b12,0 0 28px #1678ff88;
  transform:translateY(4px);
}

/* --- BOTTOM SPEED & STOP BAR --- */
.bottom-bar {
  flex-shrink:0;
  border-top:1px solid var(--border);
  background:#0e141f;
  padding:12px 16px;
  display:grid;
  grid-template-columns:1fr;
  gap:12px;
}

.stop-button {
  min-height:54px;
  width:100%;
  display:flex;
  align-items:center;
  justify-content:center;
  gap:10px;
  padding:10px;
  border:0;
  border-radius:16px;
  background:linear-gradient(120deg,#ff2144,#d60026);
  color:white;
  font-size:18px;
  font-weight:900;
  letter-spacing:.8px;
  box-shadow:0 5px 18px #ed153540,inset 0 1px 0 #ffffff40;
  touch-action:none;
}

.stop-button:active {
  transform:scale(.98);
  background:#b80020;
}

.stop-square {
  width:16px;
  height:16px;
  background:white;
  border-radius:3px;
}

.speed-area {
  width:100%;
}

.speed-heading {
  display:flex;
  justify-content:space-between;
  align-items:center;
  margin-bottom:6px;
}

.speed-title {
  color:var(--muted);
  font-size:12px;
  font-weight:900;
  letter-spacing:.8px;
}

.speed-number {
  color:var(--blue-light);
  font-size:22px;
  font-weight:900;
  font-variant-numeric:tabular-nums;
}

.slider-wrap {
  padding:6px 0;
}

#speedSlider {
  display:block;
  width:100%;
  height:14px;
  margin:0;
  border-radius:20px;
  outline:none;
  background:#233047;
  appearance:none;
  -webkit-appearance:none;
  cursor:pointer;
  touch-action:pan-x;
}

#speedSlider::-webkit-slider-thumb {
  appearance:none;
  -webkit-appearance:none;
  width:32px;
  height:32px;
  border-radius:50%;
  background:var(--blue);
  border:3px solid #9bc7ff;
  box-shadow:0 0 14px #1678ff88;
}

#speedSlider::-moz-range-thumb {
  width:28px;
  height:28px;
  border-radius:50%;
  background:var(--blue);
  border:3px solid #9bc7ff;
}

.presets {
  display:grid;
  grid-template-columns:repeat(3,1fr);
  gap:8px;
  margin-top:8px;
}

.preset {
  min-height:38px;
  border:1px solid #303e57;
  border-radius:10px;
  background:#182232;
  color:#d8e3f5;
  font-size:12px;
  font-weight:900;
  touch-action:manipulation;
}

.preset.selected {
  background:#155ac0;
  border-color:#4d9aff;
  color:white;
}

.preset:active {transform:scale(.96)}

/* --- LANDSCAPE GAMEPAD MODE (Best for Mobile Racing) --- */
@media (orientation: landscape) and (max-height: 600px) {
  .topbar {
    padding:6px 14px;
  }
  .brand h1 {font-size:18px}
  .car-icon {font-size:20px}

  /* 3-Column Gamepad: [Throttle Left] [Center Console] [Steering Right] */
  .controller {
    display:grid;
    grid-template-columns:1fr minmax(185px, 0.85fr) 1.25fr;
    grid-template-rows:auto 1fr;
    grid-template-areas:
      "top top top"
      "left center right";
  }

  .topbar {grid-area:top}

  .controls {
    display:contents;
  }

  .vertical-arrows {
    grid-area:left;
    padding:10px 14px;
    gap:12px;
  }

  .horizontal-arrows {
    grid-area:right;
    padding:10px 14px;
    gap:12px;
  }

  .bottom-bar {
    grid-area:center;
    border-top:0;
    border-left:1px solid var(--border);
    border-right:1px solid var(--border);
    height:100%;
    padding:10px 12px;
    display:flex;
    flex-direction:column;
    justify-content:center;
    gap:10px;
  }

  .arrow-up,.arrow-down {
    max-width:none;
    width:100%;
    height:45%;
    font-size:clamp(38px,11vh,64px);
  }

  .arrow-left,.arrow-right {
    max-width:none;
    height:72%;
    font-size:clamp(40px,12vh,68px);
  }

  .stop-button {
    min-height:46px;
    font-size:16px;
  }

  .preset {
    min-height:32px;
    font-size:11px;
  }
}

/* Tablet / Desktop wide layout */
@media (min-width: 768px) and (min-height: 601px) {
  .bottom-bar {
    grid-template-columns:240px 1fr;
    align-items:center;
    gap:28px;
    padding:18px 32px;
  }
  .stop-button {
    height:68px;
    font-size:22px;
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

      <div class="status-group">
        <div class="connection" id="connection">
          <span class="connection-dot"></span>
          <span id="statusText">READY</span>
        </div>
        <button type="button" class="fs-btn" id="fsBtn" aria-label="Fullscreen" title="Fullscreen">⛶</button>
      </div>
    </header>

    <main class="controls" id="padArea">
      <div class="vertical-arrows">
        <button type="button" class="arrow arrow-up" id="btnF"
          data-c="F" aria-label="Forward">↑</button>
        <button type="button" class="arrow arrow-down" id="btnB"
          data-c="B" aria-label="Backward">↓</button>
      </div>

      <div class="horizontal-arrows">
        <button type="button" class="arrow arrow-left" id="btnL"
          data-c="L" aria-label="Left">←</button>
        <button type="button" class="arrow arrow-right" id="btnR"
          data-c="R" aria-label="Right">→</button>
      </div>
    </main>

    <section class="bottom-bar">
      <button type="button" class="stop-button" id="stopButton">
        <span class="stop-square"></span>
        STOP
      </button>

      <div class="speed-area">
        <div class="speed-heading">
          <span class="speed-title">SPEED</span>
          <span class="speed-number" id="speedValue">170</span>
        </div>

        <div class="slider-wrap">
          <input id="speedSlider" type="range"
            min="0" max="255" value="170"
            aria-label="Motor speed">
        </div>

        <div class="presets">
          <button type="button" class="preset" data-speed="100">LOW</button>
          <button type="button" class="preset selected" data-speed="170">MED</button>
          <button type="button" class="preset" data-speed="255">MAX</button>
        </div>
      </div>
    </section>

  </div>
</div>

<script>
'use strict';

const connection = document.getElementById('connection');
const statusText = document.getElementById('statusText');
const slider = document.getElementById('speedSlider');
const speedValue = document.getElementById('speedValue');
const fsBtn = document.getElementById('fsBtn');

const btnMap = {
  F: document.getElementById('btnF'),
  B: document.getElementById('btnB'),
  L: document.getElementById('btnL'),
  R: document.getElementById('btnR')
};

// Track touch state, mouse state, and keyboard state independently
const touchState = { F: false, B: false, L: false, R: false };
const keyState   = { F: false, B: false, L: false, R: false };
let mouseDir = null;

let activeCommand = 'S';
let heartbeat = null;
let speedTimer = null;
let speed = 170;
let requestChain = Promise.resolve();
let commandGeneration = 0;
let stopGeneration = 0;

function setStatus(state, text) {
  connection.classList.remove('online', 'error');

  if (state === 'online') connection.classList.add('online');
  if (state === 'error') connection.classList.add('error');

  statusText.textContent = text;
}

// Keep requests ordered so STOP follows older movement requests.
function enqueueRequest(path) {
  const task = requestChain.then(async () => {
    const response = await fetch(path, {
      method: 'GET',
      cache: 'no-store'
    });

    if (!response.ok) {
      throw new Error('HTTP ' + response.status);
    }

    return response;
  });

  requestChain = task.catch(() => {});
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
  if (heartbeat !== null) return;

  heartbeat = setInterval(() => {
    if (activeCommand !== 'S') {
      sendCommand(activeCommand);
    }
  }, 300);
}

function stopHeartbeat() {
  if (heartbeat !== null) {
    clearInterval(heartbeat);
    heartbeat = null;
  }
}

function evaluateButtons() {
  const isPressed = {
    F: touchState.F || keyState.F || mouseDir === 'F',
    B: touchState.B || keyState.B || mouseDir === 'B',
    L: touchState.L || keyState.L || mouseDir === 'L',
    R: touchState.R || keyState.R || mouseDir === 'R'
  };

  // Highlight all currently held buttons simultaneously
  ['F', 'B', 'L', 'R'].forEach(k => {
    btnMap[k].classList.toggle('active', isPressed[k]);
  });

  // Steering (L/R) overrides while held; releasing L/R immediately resumes F or B
  let nextCmd = 'S';

  if (isPressed.L && !isPressed.R) {
    nextCmd = 'L';
  } else if (isPressed.R && !isPressed.L) {
    nextCmd = 'R';
  } else if (isPressed.F && !isPressed.B) {
    nextCmd = 'F';
  } else if (isPressed.B && !isPressed.F) {
    nextCmd = 'B';
  }

  if (nextCmd === activeCommand) return;

  commandGeneration++;
  activeCommand = nextCmd;

  if (activeCommand === 'S') {
    stopGeneration++;
    stopHeartbeat();
    sendCommand('S');
  } else {
    sendCommand(activeCommand);
    startHeartbeat();
  }
}

function stopAll() {
  commandGeneration++;
  stopGeneration++;

  ['F', 'B', 'L', 'R'].forEach(k => {
    touchState[k] = false;
    keyState[k] = false;
    btnMap[k].classList.remove('active');
  });
  mouseDir = null;
  activeCommand = 'S';

  stopHeartbeat();
  sendCommand('S');
}

// Scan all active touches on the screen so multi-touch and thumb-sliding both work seamlessly
function syncTouches(event) {
  touchState.F = false;
  touchState.B = false;
  touchState.L = false;
  touchState.R = false;

  for (let i = 0; i < event.touches.length; i++) {
    const t = event.touches[i];
    const el = document.elementFromPoint(t.clientX, t.clientY);
    if (el) {
      const btn = el.closest('[data-c]');
      if (btn && btn.dataset.c) {
        touchState[btn.dataset.c] = true;
      }
    }
  }

  evaluateButtons();
}

['F', 'B', 'L', 'R'].forEach(dir => {
  const btn = btnMap[dir];

  btn.addEventListener('touchstart', event => {
    event.preventDefault();
    syncTouches(event);
  }, { passive: false });

  btn.addEventListener('touchmove', event => {
    event.preventDefault();
    syncTouches(event);
  }, { passive: false });

  btn.addEventListener('touchend', event => {
    event.preventDefault();
    syncTouches(event);
  }, { passive: false });

  btn.addEventListener('touchcancel', event => {
    event.preventDefault();
    syncTouches(event);
  }, { passive: false });

  // Desktop mouse support
  btn.addEventListener('mousedown', event => {
    event.preventDefault();
    mouseDir = dir;
    evaluateButtons();
  });

  btn.addEventListener('contextmenu', event => event.preventDefault());
});

window.addEventListener('mouseup', () => {
  if (mouseDir !== null) {
    mouseDir = null;
    evaluateButtons();
  }
});

// Keyboard support (Hold W/Up + A/Left or D/Right together)
const keyToDir = {
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
  const dir = keyToDir[event.code];
  if (dir && !keyState[dir]) {
    event.preventDefault();
    keyState[dir] = true;
    evaluateButtons();
  }
});

window.addEventListener('keyup', event => {
  const dir = keyToDir[event.code];
  if (dir && keyState[dir]) {
    event.preventDefault();
    keyState[dir] = false;
    evaluateButtons();
  }
});

document.getElementById('stopButton').addEventListener('touchstart', event => {
  event.preventDefault();
  stopAll();
}, { passive: false });

document.getElementById('stopButton').addEventListener('click', stopAll);

function updateSpeed(value) {
  speed = Math.max(0, Math.min(255, Number(value)));
  speedValue.textContent = speed;

  document.querySelectorAll('[data-speed]').forEach(button => {
    button.classList.toggle(
      'selected',
      Number(button.dataset.speed) === speed
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

// Fullscreen toggle for mobile browsers
fsBtn.addEventListener('click', () => {
  const docEl = document.documentElement;
  if (!document.fullscreenElement) {
    if (docEl.requestFullscreen) docEl.requestFullscreen().catch(() => {});
  } else {
    if (document.exitFullscreen) document.exitFullscreen().catch(() => {});
  }
});

// Stop if the browser tab loses focus.
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

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.begin();

  lastCommandTime = millis();

  Serial.println("RoboRacer web server ready");
}

// ---------------- MAIN LOOP ------------------------
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
