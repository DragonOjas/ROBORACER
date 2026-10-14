#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ======================================================
// ROBO RACER - ESP8266 NODEMCU / ESP-12E + L298N
// ======================================================
// ENA -> D7 / GPIO13
// IN1 -> D1 / GPIO5
// IN2 -> D2 / GPIO4
// ENB -> D8 / GPIO15 (remove ENB jumper)
// IN3 -> D5 / GPIO14
// IN4 -> D6 / GPIO12
//
// IMPORTANT:
// - Connect ESP8266 GND to L298N GND.
// - Power motors from a suitable motor supply.
// - Do not power motors directly from the ESP8266.
// - Lift the wheels before testing.
//
// Wi-Fi: OLL_RoboRacer
// Password: robostudios@121321##@@

#define ENA 13
#define IN1 5
#define IN2 4
#define ENB 15
#define IN3 14
#define IN4 12

const char* ssid = "OLL_RoboRacer";
const char* password = "robostudios@121321##@@";

ESP8266WebServer server(80);

int motorSpeed = 170;
int currentLeft = 0;
int currentRight = 0;

unsigned long lastCommandTime = 0;
const unsigned long FAILSAFE_MS = 1200;

// ======================================================
// MOTOR CONTROL
// ======================================================

void stopMotors() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  currentLeft = 0;
  currentRight = 0;
}

void setMotors(int left, int right) {
  left = constrain(left, -255, 255);
  right = constrain(right, -255, 255);

  // Stop PWM while switching motor direction.
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  // Left motor direction.
  digitalWrite(IN1, left > 0 ? HIGH : LOW);
  digitalWrite(IN2, left < 0 ? HIGH : LOW);

  // Right motor direction.
  digitalWrite(IN3, right > 0 ? HIGH : LOW);
  digitalWrite(IN4, right < 0 ? HIGH : LOW);

  analogWrite(ENA, abs(left));
  analogWrite(ENB, abs(right));

  currentLeft = left;
  currentRight = right;
  lastCommandTime = millis();
}

// ======================================================
// WEBPAGE
// ======================================================

const char webpage[] PROGMEM = R"ROBORACER(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport"
 content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<meta name="theme-color" content="#0b0e15">
<title>RoboRacer</title>

<style>
:root {
  --bg:#0b0e15;
  --panel:#111722;
  --border:#263248;
  --blue:#1678ff;
  --blue2:#75b4ff;
  --text:#f5f7ff;
  --muted:#9baac1;
  --red:#f21c3b;
  --green:#3ddc97;
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
  display:flex;
  align-items:center;
  justify-content:center;
  color:var(--text);
  font-family:Arial,Helvetica,sans-serif;
  background:radial-gradient(ellipse at top,#172131,transparent 65%),var(--bg);
}

button,input {
  font:inherit;
}

button {
  cursor:pointer;
}

.app {
  width:100%;
  max-width:960px;
  padding:10px;
  margin:auto;
}

.controller {
  overflow:hidden;
  border:1px solid var(--border);
  border-radius:20px;
  background:linear-gradient(145deg,#151c28,#0e131d);
  box-shadow:0 16px 50px #0006;
}

.topbar {
  padding:13px 15px;
  display:flex;
  align-items:center;
  justify-content:space-between;
  gap:12px;
  flex-wrap:wrap;
  border-bottom:1px solid var(--border);
}

.brand {
  display:flex;
  align-items:center;
  gap:8px;
}

.brand h1 {
  margin:0;
  font-size:22px;
  font-weight:900;
  letter-spacing:-1px;
}

.brand h1 span {
  color:var(--blue);
}

.top-actions {
  display:flex;
  align-items:center;
  gap:10px;
}

.connection {
  display:flex;
  align-items:center;
  gap:7px;
  font-size:10px;
  font-weight:900;
  color:var(--muted);
}

.dot {
  width:9px;
  height:9px;
  border-radius:50%;
  background:var(--red);
}

.connection.online .dot {
  background:var(--green);
}

.icon-button {
  padding:10px;
  border:1px solid #344158;
  border-radius:10px;
  background:#1a2434;
  color:white;
  font-weight:800;
}

.stats {
  display:grid;
  grid-template-columns:repeat(3,minmax(0,1fr));
  gap:8px;
  padding:10px 12px 0;
}

.stat {
  min-width:0;
  padding:9px;
  border:1px solid #202b3d;
  border-radius:11px;
  background:#101722;
}

.stat small {
  display:block;
  color:#8494ad;
  font-size:9px;
  font-weight:900;
  letter-spacing:.7px;
}

.stat strong {
  display:block;
  margin-top:5px;
  font-size:14px;
  overflow:hidden;
  text-overflow:ellipsis;
  white-space:nowrap;
}

.blue {
  color:var(--blue2);
}

.mode-row {
  padding:13px 12px 0;
  display:flex;
  justify-content:space-between;
  gap:8px;
  color:var(--muted);
  font-size:10px;
  font-weight:900;
  letter-spacing:.5px;
}

.controls {
  padding:25px 12px;
  display:grid;
  grid-template-columns:1fr 1.35fr;
  gap:12px;
  align-items:center;
  justify-items:center;
  touch-action:none;
}

.vertical {
  display:flex;
  flex-direction:column;
  gap:12px;
}

.horizontal {
  display:flex;
  gap:10px;
}

.arrow {
  display:flex;
  align-items:center;
  justify-content:center;
  color:#e9efff;
  background:linear-gradient(145deg,#202c40,#121a28);
  border:2px solid #070b12;
  border-radius:16px;
  box-shadow:0 5px 0 #070b12;
  touch-action:none;
  transition:transform .05s,background .05s;
}

.up,.down {
  width:92px;
  height:82px;
  font-size:45px;
}

.left,.right {
  width:96px;
  height:82px;
  font-size:45px;
}

.arrow.active {
  background:linear-gradient(145deg,#56a1ff,#1269f2);
  border-color:var(--blue);
  transform:translateY(3px);
}

.joysticks {
  display:none;
  justify-content:space-evenly;
  align-items:center;
  gap:12px;
  padding:22px 10px;
  touch-action:none;
}

.joysticks.visible {
  display:flex;
}

.stick-column {
  display:flex;
  flex-direction:column;
  align-items:center;
  gap:9px;
}

.stick-label {
  color:var(--muted);
  font-size:10px;
  font-weight:900;
}

.pad {
  position:relative;
  width:clamp(130px,34vw,210px);
  height:clamp(130px,34vw,210px);
  border:2px solid #344158;
  border-radius:50%;
  background:radial-gradient(circle at 35% 25%,#202c40,#101722 70%);
  touch-action:none;
}

.pad:before,.pad:after {
  content:"";
  position:absolute;
  background:#34415855;
}

.pad:before {
  left:50%;
  top:0;
  width:1px;
  height:100%;
}

.pad:after {
  top:50%;
  left:0;
  height:1px;
  width:100%;
}

.knob {
  position:absolute;
  left:50%;
  top:50%;
  width:34%;
  height:34%;
  transform:translate(-50%,-50%);
  border:2px solid var(--blue2);
  border-radius:50%;
  background:linear-gradient(145deg,#348cff,#1052b5);
  pointer-events:none;
}

.bottom {
  padding:17px;
  display:grid;
  grid-template-columns:1fr;
  align-items:center;
  gap:15px;
  border-top:1px solid var(--border);
}

.stop {
  min-height:52px;
  border:0;
  border-radius:40px;
  background:linear-gradient(120deg,#ff2144,#e9002b);
  color:white;
  font-size:18px;
  font-weight:900;
}

.speed-head {
  display:flex;
  align-items:center;
  justify-content:space-between;
  gap:8px;
  margin-bottom:8px;
}

.speed-head span:first-child {
  color:var(--muted);
  font-size:11px;
  font-weight:900;
}

.speed-head strong {
  color:var(--blue2);
  font-size:22px;
}

#speedSlider {
  width:100%;
  accent-color:var(--blue);
  touch-action:pan-y;
}

.speed-labels {
  display:flex;
  justify-content:space-between;
  color:#8594ab;
  font-size:10px;
}

.presets {
  display:grid;
  grid-template-columns:repeat(3,1fr);
  gap:8px;
  margin-top:10px;
}

.preset {
  min-height:36px;
  border:1px solid #344158;
  border-radius:9px;
  background:#1a2434;
  color:white;
  font-size:12px;
  font-weight:900;
}

.preset.selected {
  background:#155ac0;
  border-color:#3288ff;
}

.footer {
  padding:0 12px 12px;
  color:#75859c;
  text-align:center;
  font-size:11px;
}

.hidden {
  display:none!important;
}

/* SETTINGS MODAL */

.backdrop {
  position:fixed;
  inset:0;
  z-index:10;
  display:none;
  align-items:center;
  justify-content:center;
  padding:15px;
  background:#03060bdf;
}

.backdrop.open {
  display:flex;
}

.settings {
  width:100%;
  max-width:470px;
  max-height:90vh;
  max-height:90dvh;
  overflow:auto;
  padding:18px;
  border:1px solid #2b3950;
  border-radius:18px;
  background:linear-gradient(145deg,#151d2a,#0c111a);
}

.settings-head {
  display:flex;
  align-items:center;
  justify-content:space-between;
}

.settings h2 {
  margin:0;
  font-size:21px;
}

.close {
  width:35px;
  height:35px;
  border:1px solid #344158;
  border-radius:9px;
  background:#202a3a;
  color:white;
  font-size:22px;
}

.subtitle {
  color:var(--muted);
  font-size:12px;
}

.setting {
  padding:13px;
  margin-top:12px;
  border:1px solid var(--border);
  border-radius:12px;
  background:#101722;
}

.setting-title {
  margin-bottom:8px;
  font-size:12px;
  font-weight:900;
}

.option {
  display:flex;
  align-items:center;
  gap:9px;
  padding:10px;
  margin-top:7px;
  border:1px solid #344158;
  border-radius:9px;
  background:#182231;
  color:#dce6f7;
  font-size:12px;
}

.option input {
  accent-color:var(--blue);
}

.sens-row {
  display:grid;
  grid-template-columns:1fr auto;
  gap:8px;
  align-items:center;
  margin-top:12px;
  font-size:12px;
}

.sens-row output {
  color:var(--blue2);
  font-weight:900;
}

.sens-row input {
  grid-column:1/-1;
  width:100%;
  accent-color:var(--blue);
}

.setting-actions {
  display:flex;
  gap:8px;
  margin-top:14px;
}

.setting-actions button {
  flex:1;
  min-height:40px;
  border:1px solid #344158;
  border-radius:9px;
  background:#1a2434;
  color:white;
  font-weight:900;
  font-size:11px;
}

.setting-actions .primary {
  background:#155ac0;
  border-color:#3288ff;
}

.note {
  color:#75859c;
  font-size:10px;
  line-height:1.5;
}

@media(min-width:650px) {
  .app {padding:20px}
  .bottom {
    grid-template-columns:190px 1fr;
    padding:20px 28px;
    gap:25px;
  }
  .controls {padding:25px 40px}
  .up,.down {width:112px;height:100px;font-size:60px}
  .left,.right {width:130px;height:100px;font-size:60px}
}

@media(max-width:360px) {
  .up,.down {width:72px;height:68px;font-size:38px}
  .left,.right {width:74px;height:68px;font-size:38px}
  .horizontal {gap:6px}
  .stats {gap:5px}
  .stat {padding:7px}
  .stat strong {font-size:12px}
}
</style>
</head>

<body>
<div class="app">
<div class="controller">

<header class="topbar">
  <div class="brand">
    <span>???</span>
    <h1>Robo<span>Racer</span></h1>
  </div>
  <div class="top-actions">
    <div class="connection" id="connection">
      <span class="dot"></span>
      <span id="statusText">CONNECTING</span>
    </div>
    <button class="icon-button" id="settingsButton">? SETTINGS</button>
  </div>
</header>

<section class="stats">
  <div class="stat">
    <small>DRIVE STATE</small>
    <strong id="driveState">STOPPED</strong>
  </div>
  <div class="stat">
    <small>OUTPUT</small>
    <strong class="blue" id="outputValue">0 / 255</strong>
  </div>
  <div class="stat">
    <small>CONTROL MODE</small>
    <strong id="modeLabel">ARROWS</strong>
  </div>
</section>

<div class="mode-row">
  <span>QUICK SPEED</span>
  <span id="sensitivitySummary">L 100% · R 100%</span>
</div>

<main class="controls" id="classicControls">
  <div class="vertical">
    <button class="arrow up" data-dir="F">?</button>
    <button class="arrow down" data-dir="B">?</button>
  </div>
  <div class="horizontal">
    <button class="arrow left" data-dir="L">?</button>
    <button class="arrow right" data-dir="R">?</button>
  </div>
</main>

<section class="joysticks" id="singleLayout">
  <div class="stick-column">
    <div class="stick-label">THROTTLE + STEERING</div>
    <div class="pad" data-stick="single">
      <div class="knob" id="singleKnob"></div>
    </div>
  </div>
</section>

<section class="joysticks" id="dualLayout">
  <div class="stick-column">
    <div class="stick-label">THROTTLE</div>
    <div class="pad" data-stick="throttle">
      <div class="knob" id="throttleKnob"></div>
    </div>
  </div>
  <div class="stick-column">
    <div class="stick-label">STEERING</div>
    <div class="pad" data-stick="steering">
      <div class="knob" id="steeringKnob"></div>
    </div>
  </div>
</section>

<section class="bottom">
  <button class="stop" id="stopButton">¦ STOP</button>
  <div>
    <div class="speed-head">
      <span>MOTOR OUTPUT LIMIT</span>
      <strong id="speedValue">170</strong>
    </div>
    <input id="speedSlider" type="range" min="0" max="255" value="170">
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

<div class="footer">
  Hold to drive · Slide between arrows · Space = STOP
</div>

</div>
</div>

<div class="backdrop" id="settingsBackdrop">
<section class="settings" role="dialog" aria-modal="true">
  <div class="settings-head">
    <h2>Controller settings</h2>
    <button class="close" id="closeSettings">×</button>
  </div>
  <p class="subtitle">Customize your RoboRacer controls.</p>

  <div class="setting">
    <div class="setting-title">CONTROL LAYOUT</div>
    <label class="option">
      <input type="radio" name="layout" value="classic">
      Classic arrows
    </label>
    <label class="option">
      <input type="radio" name="layout" value="joystick">
      Virtual joystick
    </label>
    <label class="option">
      <input type="radio" name="layout" value="dual">
      Dual-stick controller
    </label>
  </div>

  <div class="setting">
    <div class="setting-title">STEERING SENSITIVITY</div>
    <div class="sens-row">
      <label for="leftSensitivity">Left turn</label>
      <output id="leftValue">100%</output>
      <input id="leftSensitivity" type="range" min="25" max="150" step="5" value="100">
    </div>
    <div class="sens-row">
      <label for="rightSensitivity">Right turn</label>
      <output id="rightValue">100%</output>
      <input id="rightSensitivity" type="range" min="25" max="150" step="5" value="100">
    </div>
  </div>

  <div class="setting">
    <div class="setting-title">SLIDE BETWEEN BUTTONS</div>
    <p class="note">
      Always enabled. You can slide between arrow buttons without lifting
      your finger.
    </p>
  </div>

  <div class="setting-actions">
    <button id="resetSettings">RESET</button>
    <button class="primary" id="doneSettings">DONE</button>
  </div>
</section>
</div>

<script>
"use strict";

const $ = id => document.getElementById(id);

const defaults = {
  layout: "classic",
  left: 100,
  right: 100
};

let settings = {...defaults};
let speed = 170;
let commandGeneration = 0;
let requestChain = Promise.resolve();
let lastSent = {l:0, r:0};
let active = false;
let pointerButtons = new Map();
let keyboard = {F:false,B:false,L:false,R:false};
let sticks = {
  single:{x:0,y:0},
  throttle:{x:0,y:0},
  steering:{x:0,y:0}
};

let lastRequestTime = 0;
let pendingDrive = null;
let driveTimer = null;
let speedTimer = null;

const directionButtons = [...document.querySelectorAll("[data-dir]")];

function setConnection(ok, text) {
  $("connection").classList.toggle("online", ok);
  $("statusText").textContent = text;
}

function enqueue(path, generation, allowOld = false) {
  const task = requestChain.then(async () => {
    if (!allowOld && generation !== commandGeneration) return;

    const response = await fetch(path, {
      method:"GET",
      cache:"no-store"
    });

    if (!response.ok) throw new Error("HTTP " + response.status);
    return response;
  });

  requestChain = task.catch(() => {});
  return task;
}

// Coalesce rapid joystick movements so old steering requests don't pile up.
function sendDrive(left, right, force = false) {
  left = Math.max(-255, Math.min(255, Math.round(left)));
  right = Math.max(-255, Math.min(255, Math.round(right)));

  lastSent = {l:left,r:right};
  active = left !== 0 || right !== 0;

  $("driveState").textContent = active ? "MOVING" : "STOPPED";
  $("outputValue").textContent =
    Math.max(Math.abs(left),Math.abs(right)) + " / 255";

  pendingDrive = {left,right,generation:commandGeneration,force};

  const now = Date.now();
  const wait = force ? 0 : Math.max(0,60 - (now - lastRequestTime));

  if (driveTimer) clearTimeout(driveTimer);

  driveTimer = setTimeout(() => {
    const job = pendingDrive;
    pendingDrive = null;
    driveTimer = null;

    if (!job) return;
    lastRequestTime = Date.now();

    enqueue("/drive?l=" + job.left + "&r=" + job.right,
            job.generation, job.force)
      .then(response => {
        if (response && job.generation === commandGeneration) {
          setConnection(true,"CONNECTED");
        }
      })
      .catch(() => {
        if (job.generation === commandGeneration) {
          setConnection(false,"CONNECTION ERROR");
          stopHeartbeat();
        }
      });
  },wait);
}

let heartbeatTimer = null;

function startHeartbeat() {
  if (heartbeatTimer) return;

  heartbeatTimer = setInterval(() => {
    if (active) sendDrive(lastSent.l,lastSent.r);
  },300);
}

function stopHeartbeat() {
  if (heartbeatTimer) clearInterval(heartbeatTimer);
  heartbeatTimer = null;
}

function axes() {
  let throttle = 0;
  let steering = 0;

  if (settings.layout === "classic") {
    const pressed = {F:false,B:false,L:false,R:false};

    pointerButtons.forEach(dir => {
      if (dir) pressed[dir] = true;
    });

    Object.keys(keyboard).forEach(dir => {
      if (keyboard[dir]) pressed[dir] = true;
    });

    throttle = pressed.F === pressed.B ? 0 : pressed.F ? 1 : -1;
    steering = pressed.R === pressed.L ? 0 : pressed.R ? 1 : -1;
  } else {
    const s = settings.layout === "joystick"
      ? sticks.single
      : null;

    if (s) {
      throttle = -s.y;
      steering = s.x;
    } else {
      throttle = -sticks.throttle.y;
      steering = sticks.steering.x;
    }

    const keyThrottle = keyboard.F === keyboard.B
      ? 0 : keyboard.F ? 1 : -1;

    const keySteering = keyboard.R === keyboard.L
      ? 0 : keyboard.R ? 1 : -1;

    throttle = Math.max(-1,Math.min(1,throttle + keyThrottle));
    steering = Math.max(-1,Math.min(1,steering + keySteering));
  }

  return {throttle,steering};
}

function calculateMotors() {
  let {throttle,steering} = axes();

  if (steering < 0) steering *= settings.left / 100;
  else steering *= settings.right / 100;

  let left = throttle + steering;
  let right = throttle - steering;

  const peak = Math.max(1,Math.abs(left),Math.abs(right));
  left = left / peak * speed;
  right = right / peak * speed;

  return {left:Math.round(left),right:Math.round(right)};
}

function renderButtons() {
  directionButtons.forEach(button => {
    const dir = button.dataset.dir;
    let down = keyboard[dir];

    pointerButtons.forEach(value => {
      if (value === dir) down = true;
    });

    button.classList.toggle("active",down);
  });
}

function evaluate() {
  renderButtons();

  const motors = calculateMotors();
  sendDrive(motors.left,motors.right);

  if (motors.left || motors.right) startHeartbeat();
  else stopHeartbeat();
}

function stopAll() {
  commandGeneration++;

  if (driveTimer) clearTimeout(driveTimer);
  driveTimer = null;
  pendingDrive = null;

  pointerButtons.clear();
  Object.keys(keyboard).forEach(key => keyboard[key] = false);

  Object.keys(sticks).forEach(key => {
    sticks[key].x = 0;
    sticks[key].y = 0;
  });

  document.querySelectorAll(".knob").forEach(knob => {
    knob.style.transform = "translate(-50%,-50%)";
  });

  active = false;
  lastSent = {l:0,r:0};
  stopHeartbeat();
  renderButtons();

  $("driveState").textContent = "STOPPED";
  $("outputValue").textContent = "0 / 255";

  // Force a final stop command even if older commands are queued.
  sendDrive(0,0,true);
}

function pointerButtonAt(x,y) {
  const element = document.elementFromPoint(x,y);
  const button = element && element.closest
    ? element.closest("[data-dir]")
    : null;

  return button ? button.dataset.dir : null;
}

function setPointerDirection(id,dir) {
  pointerButtons.set(id,dir);
  evaluate();
}

directionButtons.forEach(button => {
  button.addEventListener("pointerdown",event => {
    event.preventDefault();

    try { button.setPointerCapture(event.pointerId); } catch (_) {}

    setPointerDirection(event.pointerId,button.dataset.dir);
  });

  button.addEventListener("contextmenu",event => event.preventDefault());
});

// Continue tracking the finger across buttons and gaps.
document.addEventListener("pointermove",event => {
  if (!pointerButtons.has(event.pointerId)) return;

  const next = pointerButtonAt(event.clientX,event.clientY);
  if (pointerButtons.get(event.pointerId) !== next) {
    setPointerDirection(event.pointerId,next);
  }
});

function releasePointer(event) {
  if (!pointerButtons.has(event.pointerId)) return;
  pointerButtons.delete(event.pointerId);
  evaluate();
}

document.addEventListener("pointerup",releasePointer);
document.addEventListener("pointercancel",releasePointer);

// Joystick movement
function renderStick(name) {
  const knobIds = {
    single:"singleKnob",
    throttle:"throttleKnob",
    steering:"steeringKnob"
  };

  const knob = $(knobIds[name]);
  if (!knob) return;

  const pad = knob.parentElement;
  const state = sticks[name];

  knob.style.transform =
    "translate(calc(-50% + " + state.x * pad.clientWidth * .32 +
    "px),calc(-50% + " + state.y * pad.clientHeight * .32 + "px))";
}

function updateStick(name,x,y) {
  const pad = document.querySelector('[data-stick="' + name + '"]');
  const rect = pad.getBoundingClientRect();

  let nx = (x - rect.left - rect.width/2) / (rect.width*.34);
  let ny = (y - rect.top - rect.height/2) / (rect.height*.34);

  nx = Math.max(-1,Math.min(1,nx));
  ny = Math.max(-1,Math.min(1,ny));

  if (name === "throttle") nx = 0;
  if (name === "steering") ny = 0;

  sticks[name] = {x:nx,y:ny};
  renderStick(name);
  evaluate();
}

const stickPointers = new Map();

document.querySelectorAll("[data-stick]").forEach(pad => {
  const name = pad.dataset.stick;

  pad.addEventListener("pointerdown",event => {
    event.preventDefault();

    try { pad.setPointerCapture(event.pointerId); } catch (_) {}

    stickPointers.set(event.pointerId,name);
    updateStick(name,event.clientX,event.clientY);
  });
});

document.addEventListener("pointermove",event => {
  const name = stickPointers.get(event.pointerId);
  if (name) updateStick(name,event.clientX,event.clientY);
});

function releaseStick(event) {
  const name = stickPointers.get(event.pointerId);
  if (!name) return;

  stickPointers.delete(event.pointerId);
  sticks[name] = {x:0,y:0};
  renderStick(name);
  evaluate();
}

document.addEventListener("pointerup",releaseStick);
document.addEventListener("pointercancel",releaseStick);

// Keyboard
const keyMap = {
  ArrowUp:"F", KeyW:"F",
  ArrowDown:"B", KeyS:"B",
  ArrowLeft:"L", KeyA:"L",
  ArrowRight:"R", KeyD:"R"
};

window.addEventListener("keydown",event => {
  if (event.code === "Space") {
    event.preventDefault();
    stopAll();
    return;
  }

  if ($("settingsBackdrop").classList.contains("open")) return;

  const dir = keyMap[event.code];
  if (dir && !keyboard[dir]) {
    event.preventDefault();
    keyboard[dir] = true;
    evaluate();
  }
});

window.addEventListener("keyup",event => {
  const dir = keyMap[event.code];
  if (dir && keyboard[dir]) {
    event.preventDefault();
    keyboard[dir] = false;

    if (!$("settingsBackdrop").classList.contains("open")) evaluate();
  }
});

// Speed
function setSpeed(value) {
  speed = Math.max(0,Math.min(255,Number(value)||0));
  $("speedValue").textContent = speed;
  $("speedSlider").value = speed;

  document.querySelectorAll("[data-speed]").forEach(button => {
    button.classList.toggle("selected",Number(button.dataset.speed) === speed);
  });

  if (speedTimer) clearTimeout(speedTimer);

  speedTimer = setTimeout(() => {
    const gen = commandGeneration;

    enqueue("/speed?v=" + speed,gen)
      .then(response => {
        if (response && gen === commandGeneration) {
          setConnection(true,"CONNECTED");
        }
      })
      .catch(() => {
        if (gen === commandGeneration) setConnection(false,"SPEED ERROR");
      });

    evaluate();
  },80);
}

$("speedSlider").addEventListener("input",event => setSpeed(event.target.value));

document.querySelectorAll("[data-speed]").forEach(button => {
  button.addEventListener("click",() => setSpeed(button.dataset.speed));
});

// Settings persistence
function saveSettings() {
  try {
    localStorage.setItem("roboracer-settings",JSON.stringify(settings));
  } catch (_) {}
}

function loadSettings() {
  try {
    const data = JSON.parse(localStorage.getItem("roboracer-settings") || "null");

    if (data && ["classic","joystick","dual"].includes(data.layout)) {
      settings.layout = data.layout;
      settings.left = clampSensitivity(data.left);
      settings.right = clampSensitivity(data.right);
    }
  } catch (_) {}
}

function clampSensitivity(value) {
  const n = Number(value);
  return Number.isFinite(n)
    ? Math.max(25,Math.min(150,Math.round(n/5)*5))
    : 100;
}

function renderSettings() {
  document.querySelectorAll('input[name="layout"]').forEach(input => {
    input.checked = input.value === settings.layout;
  });

  $("leftSensitivity").value = settings.left;
  $("rightSensitivity").value = settings.right;
  $("leftValue").textContent = settings.left + "%";
  $("rightValue").textContent = settings.right + "%";

  $("sensitivitySummary").textContent =
    "L " + settings.left + "% · R " + settings.right + "%";

  $("classicControls").classList.toggle("hidden",settings.layout !== "classic");
  $("singleLayout").classList.toggle("visible",settings.layout === "joystick");
  $("dualLayout").classList.toggle("visible",settings.layout === "dual");

  $("modeLabel").textContent = {
    classic:"ARROWS",
    joystick:"JOYSTICK",
    dual:"DUAL STICK"
  }[settings.layout];
}

function openSettings() {
  stopAll();
  renderSettings();
  $("settingsBackdrop").classList.add("open");
}

function closeSettings() {
  $("settingsBackdrop").classList.remove("open");
}

$("settingsButton").addEventListener("click",openSettings);
$("closeSettings").addEventListener("click",closeSettings);
$("doneSettings").addEventListener("click",closeSettings);

$("settingsBackdrop").addEventListener("click",event => {
  if (event.target === $("settingsBackdrop")) closeSettings();
});

document.addEventListener("keydown",event => {
  if (event.key === "Escape") closeSettings();
});

document.querySelectorAll('input[name="layout"]').forEach(input => {
  input.addEventListener("change",() => {
    stopAll();
    settings.layout = input.value;
    saveSettings();
    renderSettings();
  });
});

$("leftSensitivity").addEventListener("input",event => {
  settings.left = clampSensitivity(event.target.value);
  renderSettings();
  saveSettings();
});

$("rightSensitivity").addEventListener("input",event => {
  settings.right = clampSensitivity(event.target.value);
  renderSettings();
  saveSettings();
});

$("resetSettings").addEventListener("click",() => {
  stopAll();
  settings = {...defaults};
  saveSettings();
  renderSettings();
});

$("stopButton").addEventListener("click",stopAll);
$("stopButton").addEventListener("pointerdown",event => event.preventDefault());

window.addEventListener("blur",stopAll);

document.addEventListener("visibilitychange",() => {
  if (document.hidden) stopAll();
});

window.addEventListener("pagehide",stopAll);

loadSettings();
renderSettings();
setConnection(false,"CONNECTING");

// Initial connection check.
fetch("/speed?v=" + speed,{cache:"no-store"})
  .then(response => {
    if (!response.ok) throw new Error("Connection failed");
    setConnection(true,"CONNECTED");
  })
  .catch(() => setConnection(false,"CONNECTION ERROR"));
</script>
</body>
</html>
)ROBORACER";

// ======================================================
// HTTP ROUTES
// ======================================================

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

  int requested = value.toInt();

  if (requested < 0 || requested > 255) {
    server.send(400, "text/plain", "Speed must be 0-255");
    return;
  }

  motorSpeed = requested;

  // If a speed command is set to zero, stop the robot.
  if (motorSpeed == 0) {
    stopMotors();
  }

  lastCommandTime = millis();
  server.send(200, "text/plain", "OK");
}

void handleDrive() {
  if (!server.hasArg("l") || !server.hasArg("r")) {
    server.send(400, "text/plain", "Missing left/right values");
    return;
  }

  String leftText = server.arg("l");
  String rightText = server.arg("r");

  if (leftText.length() == 0 || rightText.length() == 0 ||
      leftText.length() > 4 || rightText.length() > 4) {
    server.send(400, "text/plain", "Invalid motor values");
    return;
  }

  // Validate signed integer text before conversion.
  auto validInteger = [](const String& s) -> bool {
    unsigned int i = (s.length() && s.charAt(0) == '-') ? 1 : 0;
    if (i == s.length()) return false;

    for (; i < s.length(); i++) {
      if (s.charAt(i) < '0' || s.charAt(i) > '9') return false;
    }
    return true;
  };

  if (!validInteger(leftText) || !validInteger(rightText)) {
    server.send(400, "text/plain", "Values must be integers");
    return;
  }

  int left = leftText.toInt();
  int right = rightText.toInt();

  if (left < -255 || left > 255 ||
      right < -255 || right > 255) {
    server.send(400, "text/plain", "Values must be -255..255");
    return;
  }

  // The browser calculates steering and speed.
  // Firmware applies the signed motor values directly.
  setMotors(left, right);
  server.send(200, "text/plain", "OK");
}

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

  stopMotors();

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  bool started = WiFi.softAP(ssid, password);

  if (started) {
    Serial.println();
    Serial.println("RoboRacer Wi-Fi started");
    Serial.print("Network: ");
    Serial.println(ssid);
    Serial.print("Controller: http://");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Wi-Fi startup failed");
  }

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", webpage);
  });

  server.on("/speed", HTTP_GET, handleSpeed);
  server.on("/drive", HTTP_GET, handleDrive);

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  lastCommandTime = millis();

  Serial.println("RoboRacer web server ready");
}

void loop() {
  server.handleClient();

  // Stop the motors if browser commands disappear.
  if ((currentLeft != 0 || currentRight != 0) &&
      millis() - lastCommandTime >= FAILSAFE_MS) {
    stopMotors();
  }

  yield();
}
