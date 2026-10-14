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
  applyCommand();

  server.send(200, "text/plain", "Speed updated");
}

// ---------------- WEBPAGE (HTML + CSS + JS) --------
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
  --bg:#0b0e15;
  --panel:#111722;
  --border:#202b3d;
  --accent:#1678ff;
  --accent-light:#75b4ff;
  --text:#f5f7ff;
  --muted:#9baac1;
  --red:#f21c3b;
}

[data-theme="red"] {
  --accent:#ff2a2a;
  --accent-light:#ff8585;
}

[data-theme="green"] {
  --accent:#00e676;
  --accent-light:#76ff03;
}

[data-theme="dark"] {
  --bg:#050505;
  --panel:#121212;
  --border:#262626;
  --accent:#3b82f6;
  --accent-light:#93c5fd;
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
  min-height:100%;
  background:var(--bg);
  overscroll-behavior:none;
}

body {
  min-height:100vh;
  background:radial-gradient(ellipse at 50% 0%,#172131,transparent 65%),var(--bg);
  color:var(--text);
  font-family:Arial,Helvetica,sans-serif;
  display:flex;
  align-items:center;
  justify-content:center;
}

button,input,select {font:inherit}
button {cursor:pointer}

.app {
  width:100%;
  max-width:960px;
  margin:auto;
  padding:8px;
}

.controller {
  width:100%;
  overflow:hidden;
  border:1px solid var(--border);
  border-radius:20px;
  background:linear-gradient(145deg,var(--panel),var(--bg));
  box-shadow:0 16px 50px #0006,inset 0 1px 0 #ffffff08;
}

.topbar {
  padding:12px 16px;
  border-bottom:1px solid var(--border);
  display:flex;
  align-items:center;
  justify-content:space-between;
  gap:12px;
  flex-wrap:wrap;
}

.brand-group {
  display:flex;
  align-items:center;
  gap:10px;
}

.settings-btn {
  width:38px;
  height:38px;
  border-radius:10px;
  background:var(--panel);
  border:1px solid var(--border);
  color:var(--text);
  font-size:18px;
  display:flex;
  align-items:center;
  justify-content:center;
  touch-action:manipulation;
}

.settings-btn:active {transform:scale(0.95)}

.brand {
  display:flex;
  align-items:center;
  gap:8px;
}

.car-icon {
  font-size:24px;
  filter:drop-shadow(0 0 8px var(--accent));
}

.brand h1 {
  margin:0;
  font-size:22px;
  font-weight:900;
  letter-spacing:-1px;
}

.brand h1 span {color:var(--accent)}

.connection {
  display:flex;
  align-items:center;
  gap:8px;
  color:var(--muted);
  font-size:12px;
  font-weight:800;
  letter-spacing:.6px;
}

.connection-dot {
  width:10px;
  height:10px;
  flex-shrink:0;
  border-radius:50%;
  background:#8792a5;
}

.connection.online .connection-dot {
  background:var(--accent);
  box-shadow:0 0 12px var(--accent);
}

.connection.error .connection-dot {
  background:var(--red);
  box-shadow:0 0 12px var(--red);
}

/* Controls */
.controls {
  padding:18px 12px;
  display:grid;
  grid-template-columns:1fr 1.35fr;
  align-items:center;
  justify-items:center;
  gap:12px;
  touch-action:none;
}

.vertical-arrows {
  width:100%;
  display:flex;
  flex-direction:column;
  align-items:center;
  gap:14px;
  touch-action:none;
}

.horizontal-arrows {
  width:100%;
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
  background:linear-gradient(145deg,#202c40,#121a28);
  border:2px solid #070b12;
  border-radius:16px;
  box-shadow:0 5px 0 #070b12,inset 0 1px 0 #ffffff12;
  touch-action:none;
  transition:background .06s,color .06s,transform .06s,box-shadow .06s;
}

.arrow-up,.arrow-down {
  width:92px;
  height:88px;
  font-size:48px;
  line-height:1;
}

.arrow-left,.arrow-right {
  width:96px;
  height:88px;
  font-size:48px;
  line-height:1;
}

.arrow.active {
  color:white;
  background:linear-gradient(145deg,var(--accent-light),var(--accent));
  border-color:var(--accent);
  box-shadow:0 0 0 3px var(--accent),0 0 25px var(--accent);
  transform:translateY(3px);
}

.bottom-bar {
  border-top:1px solid var(--border);
  padding:14px 16px;
  display:grid;
  grid-template-columns:1fr;
  align-items:center;
  gap:14px;
}

.stop-button {
  min-height:52px;
  width:100%;
  display:flex;
  align-items:center;
  justify-content:center;
  gap:10px;
  padding:10px;
  border:0;
  border-radius:40px;
  background:linear-gradient(120deg,#ff2144,#e9002b);
  color:white;
  font-size:18px;
  font-weight:900;
  box-shadow:0 6px 16px #ed153526,inset 0 1px 0 #ffffff35;
  touch-action:manipulation;
}

.stop-button:active {
  transform:scale(.97);
  background:#c90025;
}

.stop-square {
  width:16px;
  height:16px;
  background:white;
  border-radius:3px;
}

.speed-area {
  min-width:0;
  width:100%;
}

.speed-heading {
  display:flex;
  justify-content:space-between;
  align-items:center;
  gap:10px;
  margin-bottom:6px;
}

.speed-title {
  color:var(--muted);
  font-size:13px;
  font-weight:900;
  letter-spacing:.6px;
}

.speed-number {
  color:var(--accent-light);
  font-size:22px;
  font-weight:900;
  font-variant-numeric:tabular-nums;
}

.slider-wrap {padding:4px 0 6px}

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
  width:28px;
  height:28px;
  border-radius:50%;
  background:var(--accent);
  border:2px solid var(--accent-light);
  box-shadow:0 0 14px var(--accent);
}

.presets {
  display:grid;
  grid-template-columns:repeat(3,1fr);
  gap:8px;
  margin-top:10px;
}

.preset {
  min-height:36px;
  border:1px solid var(--border);
  border-radius:9px;
  background:var(--panel);
  color:var(--text);
  font-size:12px;
  font-weight:900;
  touch-action:manipulation;
}

.preset.selected {
  background:var(--accent);
  border-color:var(--accent-light);
  color:white;
}

.preset:active {transform:scale(.97)}

.footer {
  padding:0 12px 10px;
  text-align:center;
  color:#65758d;
  font-size:11px;
}

/* Settings Modal */
.modal-overlay {
  position:fixed;
  top:0;left:0;width:100%;height:100%;
  background:rgba(0,0,0,0.7);
  display:none;
  align-items:center;
  justify-content:center;
  z-index:999;
}
.modal-overlay.open {display:flex}
.modal {
  width:90%;
  max-width:380px;
  background:var(--panel);
  border:1px solid var(--border);
  border-radius:18px;
  padding:20px;
  box-shadow:0 10px 30px #000a;
}
.modal h2 {margin-top:0;font-size:20px;color:var(--text)}
.modal label {display:block;margin:14px 0 6px;font-size:13px;color:var(--muted);font-weight:bold}
.modal select, .modal input[type="range"] {
  width:100%;
  padding:10px;
  border-radius:8px;
  background:var(--bg);
  border:1px solid var(--border);
  color:var(--text);
}
.close-modal {
  width:100%;
  margin-top:18px;
  padding:12px;
  border:0;
  border-radius:10px;
  background:var(--accent);
  color:white;
  font-weight:bold;
}

@media(min-width:650px) {
  .app {padding:20px}
  .controls {padding:32px 40px;gap:30px}
  .arrow-up,.arrow-down {width:125px;height:120px;font-size:68px}
  .arrow-left,.arrow-right {width:150px;height:115px;font-size:68px}
  .bottom-bar {
    grid-template-columns:220px 1fr;
    gap:28px;
    padding:20px 32px;
  }
}
</style>
</head>

<body>
<div class="app">
  <div class="controller">

    <header class="topbar">
      <div class="brand-group">
        <button type="button" class="settings-btn" id="openSettings" aria-label="Settings">⚙️</button>
        <div class="brand">
          <div class="car-icon">🏎️</div>
          <h1>Robo<span>Racer</span></h1>
        </div>
      </div>

      <div class="connection" id="connection">
        <span class="connection-dot"></span>
        <span id="statusText">READY</span>
      </div>
    </header>

    <main class="controls">
      <div class="vertical-arrows">
        <button type="button" class="arrow arrow-up" id="btnF" data-c="F">↑</button>
        <button type="button" class="arrow arrow-down" id="btnB" data-c="B">↓</button>
      </div>

      <div class="horizontal-arrows">
        <button type="button" class="arrow arrow-left" id="btnL" data-c="L">←</button>
        <button type="button" class="arrow arrow-right" id="btnR" data-c="R">→</button>
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
          <input id="speedSlider" type="range" min="0" max="255" value="170">
        </div>

        <div class="presets">
          <button type="button" class="preset" data-speed="100">LOW</button>
          <button type="button" class="preset selected" data-speed="170">MED</button>
          <button type="button" class="preset" data-speed="255">MAX</button>
        </div>
      </div>
    </section>

    <div class="footer">
      Hold Forward/Backward + Tap Left/Right to steer
    </div>

  </div>
</div>

<!-- Settings Modal -->
<div class="modal-overlay" id="settingsModal">
  <div class="modal">
    <h2>Controller Settings</h2>
    
    <label for="themeSelect">Theme Color</label>
    <select id="themeSelect">
      <option value="blue">Cyber Blue</option>
      <option value="red">Racing Red</option>
      <option value="green">Neon Green</option>
      <option value="dark">Midnight Dark</option>
    </select>

    <label for="sensitivitySlider">Sensitivity / Responsiveness</label>
    <input type="range" id="sensitivitySlider" min="1" max="5" value="3">

    <button type="button" class="close-modal" id="closeSettings">Save & Close</button>
  </div>
</div>

<script>
'use strict';

const connection = document.getElementById('connection');
const statusText = document.getElementById('statusText');
const slider = document.getElementById('speedSlider');
const speedValue = document.getElementById('speedValue');
const modal = document.getElementById('settingsModal');
const themeSelect = document.getElementById('themeSelect');
const sensitivitySlider = document.getElementById('sensitivitySlider');

document.getElementById('openSettings').addEventListener('click', () => modal.classList.add('open'));
document.getElementById('closeSettings').addEventListener('click', () => modal.classList.remove('open'));

themeSelect.addEventListener('change', (e) => {
  const theme = e.target.value;
  if (theme === 'blue') {
    document.documentElement.removeAttribute('data-theme');
  } else {
    document.documentElement.setAttribute('data-theme', theme);
  }
  localStorage.setItem('robocytic_theme', theme);
});

// Load saved theme
const savedTheme = localStorage.getItem('robocytic_theme');
if (savedTheme) {
  themeSelect.value = savedTheme;
  if (savedTheme !== 'blue') document.documentElement.setAttribute('data-theme', savedTheme);
}

const btnMap = {
  F: document.getElementById('btnF'),
  B: document.getElementById('btnB'),
  L: document.getElementById('btnL'),
  R: document.getElementById('btnR')
};

const buttonState = { F: false, B: false, L: false, R: false };
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

function enqueueRequest(path) {
  const task = requestChain.then(async () => {
    const response = await fetch(path, { method: 'GET', cache: 'no-store' });
    if (!response.ok) throw new Error('HTTP ' + response.status);
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
      if (generation === commandGeneration && thisStopGeneration === stopGeneration) {
        setStatus('online', 'CONNECTED');
      }
    })
    .catch(() => {
      if (generation === commandGeneration && thisStopGeneration === stopGeneration) {
        setStatus('error', 'CONNECTION ERROR');
      }
    });
}

function sendSpeed(value) {
  const generation = commandGeneration;
  enqueueRequest('/speed?v=' + value)
    .then(() => { if (generation === commandGeneration) setStatus('online', 'CONNECTED'); })
    .catch(() => { if (generation === commandGeneration) setStatus('error', 'SPEED ERROR'); });
}

function startHeartbeat() {
  if (heartbeat !== null) return;
  const sensitivity = parseInt(sensitivitySlider.value) || 3;
  // Interval adjusts based on sensitivity slider (faster heartbeat for higher sensitivity)
  const interval = Math.max(150, 350 - (sensitivity * 40));

  heartbeat = setInterval(() => {
    if (activeCommand !== 'S') sendCommand(activeCommand);
  }, interval);
}

function stopHeartbeat() {
  if (heartbeat !== null) {
    clearInterval(heartbeat);
    heartbeat = null;
  }
}

function evaluateButtons() {
  ['F', 'B', 'L', 'R'].forEach(key => {
    btnMap[key].classList.toggle('active', buttonState[key]);
  });

  let nextCmd = 'S';
  if (buttonState.L && !buttonState.R) nextCmd = 'L';
  else if (buttonState.R && !buttonState.L) nextCmd = 'R';
  else if (buttonState.F && !buttonState.B) nextCmd = 'F';
  else if (buttonState.B && !buttonState.F) nextCmd = 'B';

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
  buttonState.F = buttonState.B = buttonState.L = buttonState.R = false;
  activeCommand = 'S';
  stopHeartbeat();
  ['F', 'B', 'L', 'R'].forEach(key => btnMap[key].classList.remove('active'));
  sendCommand('S');
}

['F', 'B', 'L', 'R'].forEach(dir => {
  const btn = btnMap[dir];
  const handleTouch = event => {
    event.preventDefault();
    buttonState[dir] = event.targetTouches.length > 0;
    evaluateButtons();
  };

  btn.addEventListener('touchstart', handleTouch, { passive: false });
  btn.addEventListener('touchend', handleTouch, { passive: false });
  btn.addEventListener('touchcancel', handleTouch, { passive: false });

  btn.addEventListener('mousedown', event => {
    event.preventDefault();
    buttonState[dir] = true;
    evaluateButtons();
  });
  btn.addEventListener('mouseup', () => { if (buttonState[dir]) { buttonState[dir] = false; evaluateButtons(); } });
  btn.addEventListener('mouseleave', () => { if (buttonState[dir]) { buttonState[dir] = false; evaluateButtons(); } });
  btn.addEventListener('contextmenu', event => event.preventDefault());
});

window.addEventListener('touchend', event => {
  if (event.touches && event.touches.length === 0 && activeCommand !== 'S') stopAll();
});

const keyToDir = { ArrowUp: 'F', KeyW: 'F', ArrowDown: 'B', KeyS: 'B', ArrowLeft: 'L', KeyA: 'L', ArrowRight: 'R', KeyD: 'R' };
window.addEventListener('keydown', event => {
  if (event.code === 'Space') { event.preventDefault(); stopAll(); return; }
  const dir = keyToDir[event.code];
  if (dir && !buttonState[dir]) { event.preventDefault(); buttonState[dir] = true; evaluateButtons(); }
});
window.addEventListener('keyup', event => {
  const dir = keyToDir[event.code];
  if (dir && buttonState[dir]) { event.preventDefault(); buttonState[dir] = false; evaluateButtons(); }
});

document.getElementById('stopButton').addEventListener('touchstart', event => { event.preventDefault(); stopAll(); }, { passive: false });
document.getElementById('stopButton').addEventListener('click', stopAll);

function updateSpeed(value) {
  speed = Math.max(0, Math.min(255, Number(value)));
  speedValue.textContent = speed;
  document.querySelectorAll('[data-speed]').forEach(button => {
    button.classList.toggle('selected', Number(button.dataset.speed) === speed);
  });
  if (speedTimer !== null) clearTimeout(speedTimer);
  speedTimer = setTimeout(() => { sendSpeed(speed); speedTimer = null; }, 60);
}

slider.addEventListener('input', event => updateSpeed(event.target.value));
document.querySelectorAll('[data-speed]').forEach(button => {
  button.addEventListener('click', () => { slider.value = button.dataset.speed; updateSpeed(button.dataset.speed); });
});

window.addEventListener('blur', stopAll);
document.addEventListener('visibilitychange', () => { if (document.hidden) stopAll(); });
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
  WiFi.setOutputPower(15.0); // Prevents Wi-Fi drops on battery power

  bool apStarted = WiFi.softAP(ssid, password, 1, false, 1);

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

  if (currentCommand != 'S' && millis() - lastCommandTime >= TIMEOUT_MS) {
    currentCommand = 'S';
    stopMotors();
  }

  yield();
}
