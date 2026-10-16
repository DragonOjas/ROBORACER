#include <WiFi.h>
#include <WebServer.h>

// ---------------- LEFT BTS7960 ----------------
#define L_RPWM  4
#define L_LPWM  5
#define L_R_EN  22
#define L_L_EN  23

// ---------------- RIGHT BTS7960 ---------------
#define R_RPWM  15
#define R_LPWM  19
#define R_R_EN  21
#define R_L_EN  18

// ---------------- WIFI ACCESS POINT ------------
const char* ssid = "OLL_RoboRacer";
const char* password = "robostudios@121321";

WebServer server(80);

// ---------------- MOTOR STATE -----------------
int motorSpeed = 170;
char currentCommand = 'S';

unsigned long lastCommandTime = 0;
const unsigned long TIMEOUT_MS = 600;

const uint32_t PWM_FREQUENCY = 1000;
const uint8_t PWM_RESOLUTION = 8;

bool motorsReady = false;

// ---------------- STOP MOTORS -----------------
void stopMotors() {
  ledcWrite(L_RPWM, 0);
  ledcWrite(L_LPWM, 0);
  ledcWrite(R_RPWM, 0);
  ledcWrite(R_LPWM, 0);
}

// ---------------- LEFT MOTOR ------------------
void setLeftMotor(int speed) {
  speed = constrain(speed, -255, 255);

  // Turn off both directions before switching.
  ledcWrite(L_RPWM, 0);
  ledcWrite(L_LPWM, 0);

  if (speed > 0) {
    ledcWrite(L_RPWM, speed);
  } else if (speed < 0) {
    ledcWrite(L_LPWM, -speed);
  }
}

// ---------------- RIGHT MOTOR -----------------
void setRightMotor(int speed) {
  speed = constrain(speed, -255, 255);

  ledcWrite(R_RPWM, 0);
  ledcWrite(R_LPWM, 0);

  if (speed > 0) {
    ledcWrite(R_RPWM, speed);
  } else if (speed < 0) {
    ledcWrite(R_LPWM, -speed);
  }
}

// ---------------- BOTH MOTORS -----------------
void setMotors(int left, int right) {
  if (!motorsReady) {
    stopMotors();
    return;
  }

  setLeftMotor(left);
  setRightMotor(right);
}

// ---------------- APPLY COMMAND ---------------
void applyCommand() {
  if (!motorsReady) {
    stopMotors();
    return;
  }

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

// ---------------- COMMAND HANDLER -------------
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

// ---------------- SPEED HANDLER ---------------
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

  // Apply the new speed without refreshing the
  // movement safety timeout.
  applyCommand();

  server.send(200, "text/plain", "Speed updated");
}

// ---------------- ORIGINAL WEBPAGE ------------
const char webpage[] PROGMEM = R"rawliteral(
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
  user-select:none;
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
        <span id="statusText">READY</span>
      </div>
    </header>

    <main class="controls">
      <div class="vertical-arrows">
        <button class="arrow arrow-up"
          data-c="F" aria-label="Forward">↑</button>
        <button class="arrow arrow-down"
          data-c="B" aria-label="Backward">↓</button>
      </div>

      <div class="horizontal-arrows">
        <button class="arrow arrow-left"
          data-c="L" aria-label="Left">←</button>
        <button class="arrow arrow-right"
          data-c="R" aria-label="Right">→</button>
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
          <span class="speed-title">SPEED</span>
          <span class="speed-number" id="speedValue">170</span>
        </div>

        <div class="slider-wrap">
          <input id="speedSlider" type="range"
            min="0" max="255" value="170"
            aria-label="Motor speed">
        </div>

        <div class="speed-labels">
          <span>SLOW</span>
          <span>MEDIUM</span>
          <span>FAST</span>
        </div>

        <div class="presets">
          <button class="preset" data-speed="100">LOW</button>
          <button class="preset selected" data-speed="170">MED</button>
          <button class="preset" data-speed="255">MAX</button>
        </div>
      </div>
    </section>

    <div class="footer">
      Hold a direction to drive · Release to stop
    </div>

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
  stopHeartbeat();

  heartbeat = setInterval(() => {
    if (held !== null && !stopped) {
      sendCommand(held);
    }
  }, 300);
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
    } catch (error) {
      // Window-level release handlers remain available.
    }

    sendCommand(held);
    startHeartbeat();
  });

  button.addEventListener('pointerup', event => {
    if (event.pointerId !== pointerId) return;
    stopAll();
  });

  button.addEventListener('pointercancel', event => {
    if (event.pointerId !== pointerId) return;
    stopAll();
  });

  button.addEventListener('lostpointercapture', event => {
    if (event.pointerId !== pointerId) return;
    stopAll();
  });
});

window.addEventListener('pointerup', event => {
  if (pointerId !== null && event.pointerId === pointerId) {
    stopAll();
  }
});

window.addEventListener('pointercancel', event => {
  if (pointerId !== null && event.pointerId === pointerId) {
    stopAll();
  }
});

document.getElementById('stopButton').addEventListener(
  'click', stopAll
);

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

// ---------------- SETUP -----------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  // Keep motor drivers disabled while initializing.
  pinMode(L_R_EN, OUTPUT);
  pinMode(L_L_EN, OUTPUT);
  pinMode(R_R_EN, OUTPUT);
  pinMode(R_L_EN, OUTPUT);

  digitalWrite(L_R_EN, LOW);
  digitalWrite(L_L_EN, LOW);
  digitalWrite(R_R_EN, LOW);
  digitalWrite(R_L_EN, LOW);

  // Initialize PWM outputs using Arduino-ESP32 3.x.
  bool pwmOK = true;

  pwmOK &= ledcAttach(L_RPWM, PWM_FREQUENCY, PWM_RESOLUTION);
  pwmOK &= ledcAttach(L_LPWM, PWM_FREQUENCY, PWM_RESOLUTION);
  pwmOK &= ledcAttach(R_RPWM, PWM_FREQUENCY, PWM_RESOLUTION);
  pwmOK &= ledcAttach(R_LPWM, PWM_FREQUENCY, PWM_RESOLUTION);

  if (!pwmOK) {
    Serial.println("ERROR: PWM initialization failed.");
    stopMotors();

    while (true) {
      delay(1000);
    }
  }

  stopMotors();

  // Enable both BTS7960 drivers after PWM setup.
  digitalWrite(L_R_EN, HIGH);
  digitalWrite(L_L_EN, HIGH);
  digitalWrite(R_R_EN, HIGH);
  digitalWrite(R_L_EN, HIGH);

  motorsReady = true;
  currentCommand = 'S';

  // Start ESP32 Wi-Fi hotspot.
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  if (!WiFi.softAP(ssid, password)) {
    Serial.println("ERROR: Wi-Fi startup failed.");

    motorsReady = false;
    stopMotors();

    digitalWrite(L_R_EN, LOW);
    digitalWrite(L_L_EN, LOW);
    digitalWrite(R_R_EN, LOW);
    digitalWrite(R_L_EN, LOW);
    return;
  }

  Serial.println();
  Serial.println("RoboRacer Wi-Fi started");
  Serial.print("Network: ");
  Serial.println(ssid);
  Serial.print("Controller URL: http://");
  Serial.println(WiFi.softAPIP());

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

// ---------------- MAIN LOOP -------------------
void loop() {
  server.handleClient();

  // Safety stop if movement heartbeats disappear.
  if (currentCommand != 'S' &&
      millis() - lastCommandTime >= TIMEOUT_MS) {
    currentCommand = 'S';
    stopMotors();
    Serial.println("Safety timeout: motors stopped");
  }

  delay(1);
}
