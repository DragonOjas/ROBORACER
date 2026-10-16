

#include <WiFi.h>
#include <WebServer.h>

// ============================================================================
//                              USER CONFIG
// ============================================================================

const char* AP_SSID     = "IIT KANPUR RACER ROBO";
const char* AP_PASSWORD = "roboracer121321@";

// Failsafe timeouts
const int RC_FAILSAFE_MS   = 300;
const int WIFI_FAILSAFE_MS = 500;

// Motor PWM
const int PWM_FREQ     = 20000;
const int PWM_RES_BITS = 8;
const int MOTOR_MAX    = 255;

// RC pulse width range
const int RC_PULSE_MIN = 1000;
const int RC_PULSE_MAX = 2000;
const int RC_PULSE_MID = 1500;

// RC deadband
const int RC_DEADBAND = 30;

// Minimum stick movement required to take control away from phone
const int RC_TAKEOVER_DEADBAND = 60;


// ============================================================================
//                              PIN DEFINITIONS
// ============================================================================

// ----------------------------- LEFT MOTOR ----------------------------------

#define L_RPWM  4
#define L_LPWM  5
#define L_R_EN  22
#define L_L_EN  23

// ----------------------------- RIGHT MOTOR ---------------------------------

#define R_RPWM 15
#define R_LPWM 19
#define R_R_EN 21
#define R_L_EN 18

// ----------------------------- RC RECEIVER ---------------------------------

#define CH1_PIN 8     // Steering
#define CH2_PIN 9     // Throttle


// ============================================================================
//                                  STATE
// ============================================================================

volatile uint32_t ch1_riseTime  = 0;
volatile uint32_t ch1_pulse     = 1500;
volatile uint32_t ch1_lastUpdate = 0;

volatile uint32_t ch2_riseTime  = 0;
volatile uint32_t ch2_pulse     = 1500;
volatile uint32_t ch2_lastUpdate = 0;

// Phone joystick state
volatile int phoneThrottle = 0;
volatile int phoneSteering = 0;
volatile uint32_t lastPhoneCmdTime = 0;

WebServer server(80);


// ============================================================================
//                              RC INTERRUPTS
// ============================================================================

void IRAM_ATTR isrCH1() {

  if (digitalRead(CH1_PIN) == HIGH) {

    ch1_riseTime = micros();

  } else {

    uint32_t width = micros() - ch1_riseTime;

    if (width > 800 && width < 2200) {
      ch1_pulse = width;
      ch1_lastUpdate = millis();
    }
  }
}


void IRAM_ATTR isrCH2() {

  if (digitalRead(CH2_PIN) == HIGH) {

    ch2_riseTime = micros();

  } else {

    uint32_t width = micros() - ch2_riseTime;

    if (width > 800 && width < 2200) {
      ch2_pulse = width;
      ch2_lastUpdate = millis();
    }
  }
}


// ============================================================================
//                            MOTOR INITIALIZATION
// ============================================================================

void setupMotorPins() {

  pinMode(L_R_EN, OUTPUT);
  pinMode(L_L_EN, OUTPUT);

  pinMode(R_R_EN, OUTPUT);
  pinMode(R_L_EN, OUTPUT);

  // Enable both BTS7960 drivers
  digitalWrite(L_R_EN, HIGH);
  digitalWrite(L_L_EN, HIGH);

  digitalWrite(R_R_EN, HIGH);
  digitalWrite(R_L_EN, HIGH);


  // ESP32 Arduino Core 3.x
  // Pin-based LEDC API

  ledcAttach(L_RPWM, PWM_FREQ, PWM_RES_BITS);
  ledcAttach(L_LPWM, PWM_FREQ, PWM_RES_BITS);

  ledcAttach(R_RPWM, PWM_FREQ, PWM_RES_BITS);
  ledcAttach(R_LPWM, PWM_FREQ, PWM_RES_BITS);


  /*
   * ------------------------------------------------------------------------
   * ESP32 Arduino Core 2.x USERS
   * ------------------------------------------------------------------------
   *
   * Replace the ledcAttach() calls above with:
   *
   * ledcSetup(0, PWM_FREQ, PWM_RES_BITS);
   * ledcAttachPin(L_RPWM, 0);
   *
   * ledcSetup(1, PWM_FREQ, PWM_RES_BITS);
   * ledcAttachPin(L_LPWM, 1);
   *
   * ledcSetup(2, PWM_FREQ, PWM_RES_BITS);
   * ledcAttachPin(R_RPWM, 2);
   *
   * ledcSetup(3, PWM_FREQ, PWM_RES_BITS);
   * ledcAttachPin(R_LPWM, 3);
   *
   * Then replace ledcWrite(PIN, duty) with:
   *
   * ledcWrite(CHANNEL, duty);
   *
   * ------------------------------------------------------------------------
   */
}


// ============================================================================
//                              MOTOR CONTROL
// ============================================================================

// Speed:
//   -255 = Full Reverse
//      0 = Stop
//   +255 = Full Forward

void driveMotor(int rpwmPin, int lpwmPin, int speed) {

  speed = constrain(speed, -MOTOR_MAX, MOTOR_MAX);

  if (speed >= 0) {

    ledcWrite(rpwmPin, speed);
    ledcWrite(lpwmPin, 0);

  } else {

    ledcWrite(rpwmPin, 0);
    ledcWrite(lpwmPin, -speed);
  }
}


// ----------------------------------------------------------------------------
// Stop both motors
// ----------------------------------------------------------------------------

void stopMotors() {

  ledcWrite(L_RPWM, 0);
  ledcWrite(L_LPWM, 0);

  ledcWrite(R_RPWM, 0);
  ledcWrite(R_LPWM, 0);
}


// ----------------------------------------------------------------------------
// Differential drive mixing
//
// throttle : -255 .. +255
// steering : -255 .. +255
// ----------------------------------------------------------------------------

void driveDifferential(int throttle, int steering) {

  int left  = throttle + steering;
  int right = throttle - steering;

  left  = constrain(left,  -MOTOR_MAX, MOTOR_MAX);
  right = constrain(right, -MOTOR_MAX, MOTOR_MAX);

  driveMotor(L_RPWM, L_LPWM, left);
  driveMotor(R_RPWM, R_LPWM, right);
}


// ============================================================================
//                              PHONE CONTROL UI
// ============================================================================

const char* controlPage = R"HTML(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
      content="width=device-width, initial-scale=1,
               viewport-fit=cover">

<title>SumoBot Control</title>

<style>

:root{

  --red:#ff3b3b;
  --red-dark:#b8232a;

  --bg1:#0d0f14;
  --bg2:#181c24;

  --panel:#1c212b;
  --line:#2a303c;

  --text:#f2f3f5;
  --muted:#8b93a3;

  --green:#3ddc84;
}

*{
  box-sizing:border-box;
  -webkit-tap-highlight-color:transparent;
}

html,body{

  margin:0;
  height:100%;
  overflow:hidden;

  background:
    radial-gradient(
      circle at 50% 0%,
      var(--bg2),
      var(--bg1) 70%
    );

  color:var(--text);

  font-family:
    -apple-system,
    BlinkMacSystemFont,
    'Segoe UI',
    Roboto,
    sans-serif;

  touch-action:none;
  user-select:none;

  padding-top:
    env(safe-area-inset-top,0px);

  padding-bottom:
    env(safe-area-inset-bottom,0px);
}

.wrap{

  height:100%;

  display:flex;
  flex-direction:column;

  align-items:center;
  justify-content:space-between;

  padding:18px 16px 22px;
}

header{

  width:100%;

  display:flex;
  align-items:center;
  justify-content:space-between;
}

.brand{

  display:flex;
  align-items:center;
  gap:10px;
}

.brand .dot{

  width:10px;
  height:10px;

  border-radius:50%;

  background:var(--red);

  box-shadow:
    0 0 10px var(--red);
}

.brand h1{

  font-size:17px;
  letter-spacing:1.5px;

  margin:0;

  font-weight:700;
}

.status{

  display:flex;
  align-items:center;

  gap:6px;

  font-size:12px;
  color:var(--muted);

  background:var(--panel);

  border:1px solid var(--line);

  padding:6px 10px;

  border-radius:20px;
}

.status .led{

  width:8px;
  height:8px;

  border-radius:50%;

  background:var(--green);

  transition:background .2s;
}

.status.offline .led{
  background:#555;
}

.readout{

  display:flex;

  gap:10px;

  width:100%;

  justify-content:center;

  margin:10px 0 4px;
}

.readout .box{

  flex:1;

  max-width:150px;

  background:var(--panel);

  border:1px solid var(--line);

  border-radius:12px;

  padding:8px 10px;

  text-align:center;
}

.readout .label{

  font-size:10px;

  color:var(--muted);

  letter-spacing:1px;

  text-transform:uppercase;
}

.readout .val{

  font-size:20px;

  font-weight:700;

  font-variant-numeric:
    tabular-nums;
}

#pad{

  width:min(78vw, 320px);
  height:min(78vw, 320px);

  border-radius:50%;

  background:
    radial-gradient(
      circle at 50% 50%,
      #232936 0%,
      #171b22 70%
    );

  border:2px solid var(--line);

  position:relative;

  box-shadow:
    inset 0 0 30px rgba(0,0,0,.5),
    0 0 40px rgba(255,59,59,0.06);

  touch-action:none;
}

#pad::before{

  content:'';

  position:absolute;

  inset:14%;

  border:
    1px dashed #333a47;

  border-radius:50%;
}

#stick{

  width:34%;
  height:34%;

  border-radius:50%;

  position:absolute;

  top:33%;
  left:33%;

  background:
    radial-gradient(
      circle at 35% 30%,
      #ff6a5f,
      var(--red) 55%,
      var(--red-dark) 100%
    );

  box-shadow:
    0 6px 18px rgba(255,59,59,.45),
    inset 0 -4px 8px rgba(0,0,0,.25);

  transition:box-shadow .15s;
}

#pad.active #stick{

  box-shadow:
    0 6px 24px rgba(255,59,59,.75),
    inset 0 -4px 8px rgba(0,0,0,.25);
}

#stop{

  margin-top:18px;

  width:min(78vw,320px);

  padding:16px;

  font-size:16px;

  font-weight:700;

  letter-spacing:1px;

  background:
    linear-gradient(
      180deg,
      #ff4d4d,
      var(--red-dark)
    );

  color:#fff;

  border:none;

  border-radius:14px;

  box-shadow:
    0 6px 16px rgba(255,59,59,.35);
}

#stop:active{
  transform:scale(.97);
}

footer{

  font-size:11px;

  color:var(--muted);

  text-align:center;

  max-width:280px;
}

</style>

</head>


<body>

<div class="wrap">

<header>

  <div class="brand">

    <div class="dot"></div>

    <h1>SUMOBOT</h1>

  </div>


  <div class="status offline" id="status">

    <div class="led"></div>

    <span id="statusText">
      connecting…
    </span>

  </div>

</header>


<div class="readout">

  <div class="box">

    <div class="label">
      Throttle
    </div>

    <div class="val" id="tVal">
      0
    </div>

  </div>


  <div class="box">

    <div class="label">
      Steering
    </div>

    <div class="val" id="sVal">
      0
    </div>

  </div>

</div>


<div id="pad">

  <div id="stick"></div>

</div>


<button id="stop">
  ■ STOP
</button>


<footer>

  Drive with the joystick.
  Picking up the CT6B transmitter overrides this
  automatically — release its sticks to regain
  phone control.

</footer>

</div>


<script>

const pad =
  document.getElementById('pad');

const stick =
  document.getElementById('stick');

const tVal =
  document.getElementById('tVal');

const sVal =
  document.getElementById('sVal');

const statusEl =
  document.getElementById('status');

const statusText =
  document.getElementById('statusText');


let dragging = false;

let cx, cy, radius;

let lastOkTime = Date.now();


// --------------------------------------------------------------------------
// Calculate joystick geometry
// --------------------------------------------------------------------------

function updateGeometry(){

  const r =
    pad.getBoundingClientRect();

  cx =
    r.left + r.width / 2;

  cy =
    r.top + r.height / 2;

  radius =
    r.width / 2;
}

updateGeometry();

window.addEventListener(
  'resize',
  updateGeometry
);


// --------------------------------------------------------------------------
// Connection status
// --------------------------------------------------------------------------

function setStatus(ok){

  if(ok){

    statusEl.classList.remove(
      'offline'
    );

    statusText.textContent =
      'connected';

  }else{

    statusEl.classList.add(
      'offline'
    );

    statusText.textContent =
      'offline';
  }
}


// --------------------------------------------------------------------------
// Send command to ESP32
// --------------------------------------------------------------------------

async function sendCmd(t,s){

  tVal.textContent = t;

  sVal.textContent = s;

  try{

    const res =
      await fetch(
        `/cmd?t=${t}&s=${s}`,
        {
          cache:'no-store'
        }
      );

    if(res.ok){

      lastOkTime =
        Date.now();

      setStatus(true);
    }

  }catch(e){

    // Watchdog handles connection status
  }
}


// --------------------------------------------------------------------------
// Joystick processing
// --------------------------------------------------------------------------

function handle(x,y){

  let dx =
    x - cx;

  let dy =
    y - cy;


  const dist =
    Math.min(
      Math.sqrt(
        dx*dx + dy*dy
      ),
      radius
    );


  const angle =
    Math.atan2(dy,dx);


  const px =
    Math.cos(angle) * dist;

  const py =
    Math.sin(angle) * dist;


  stick.style.left =
    (50 + (px/radius)*33) + '%';

  stick.style.top =
    (50 + (py/radius)*33) + '%';


  const throttle =
    Math.round(
      (-py/radius) * 255
    );


  const steering =
    Math.round(
      (px/radius) * 255
    );


  sendCmd(
    throttle,
    steering
  );
}


// --------------------------------------------------------------------------
// Reset joystick
// --------------------------------------------------------------------------

function reset(){

  stick.style.left = '33%';

  stick.style.top = '33%';

  pad.classList.remove(
    'active'
  );

  sendCmd(0,0);
}


// --------------------------------------------------------------------------
// Touch / Pointer controls
// --------------------------------------------------------------------------

pad.addEventListener(
  'pointerdown',
  e => {

    dragging = true;

    pad.classList.add(
      'active'
    );

    updateGeometry();

    handle(
      e.clientX,
      e.clientY
    );

    if(navigator.vibrate)
      navigator.vibrate(10);
  }
);


window.addEventListener(
  'pointermove',
  e => {

    if(dragging)
      handle(
        e.clientX,
        e.clientY
      );
  }
);


window.addEventListener(
  'pointerup',
  () => {

    dragging = false;

    reset();
  }
);


window.addEventListener(
  'pointercancel',
  () => {

    dragging = false;

    reset();
  }
);


// --------------------------------------------------------------------------
// Emergency STOP button
// --------------------------------------------------------------------------

document
  .getElementById('stop')
  .addEventListener(
    'click',
    () => {

      if(navigator.vibrate)
        navigator.vibrate(20);

      reset();
    }
  );


// --------------------------------------------------------------------------
// Phone keepalive
// --------------------------------------------------------------------------

setInterval(
  () => {

    if(!dragging)
      sendCmd(0,0);

  },
  200
);


// --------------------------------------------------------------------------
// Connection watchdog
// --------------------------------------------------------------------------

setInterval(
  () => {

    if(
      Date.now() - lastOkTime > 800
    ){

      setStatus(false);
    }

  },
  300
);

</script>

</body>

</html>

)HTML";


// ============================================================================
//                              WEB SERVER
// ============================================================================

void handleRoot() {

  server.send(
    200,
    "text/html",
    controlPage
  );
}


void handleCmd() {

  if(server.hasArg("t")){

    phoneThrottle =
      constrain(
        server.arg("t").toInt(),
        -MOTOR_MAX,
        MOTOR_MAX
      );
  }


  if(server.hasArg("s")){

    phoneSteering =
      constrain(
        server.arg("s").toInt(),
        -MOTOR_MAX,
        MOTOR_MAX
      );
  }


  lastPhoneCmdTime =
    millis();


  server.send(
    200,
    "text/plain",
    "OK"
  );
}


// ============================================================================
//                            WEB SERVER SETUP
// ============================================================================

void setupWebServer() {

  WiFi.mode(WIFI_AP);

  WiFi.softAP(
    AP_SSID,
    AP_PASSWORD
  );


  Serial.print(
    "AP IP address: "
  );

  Serial.println(
    WiFi.softAPIP()
  );


  server.on(
    "/",
    handleRoot
  );

  server.on(
    "/cmd",
    handleCmd
  );


  server.begin();
}


// ============================================================================
//                                  SETUP
// ============================================================================

void setup() {

  Serial.begin(115200);

  delay(200);


  // Motor initialization
  setupMotorPins();

  stopMotors();


  // RC input pins
  pinMode(
    CH1_PIN,
    INPUT
  );

  pinMode(
    CH2_PIN,
    INPUT
  );


  // RC interrupts
  attachInterrupt(
    digitalPinToInterrupt(CH1_PIN),
    isrCH1,
    CHANGE
  );

  attachInterrupt(
    digitalPinToInterrupt(CH2_PIN),
    isrCH2,
    CHANGE
  );


  // Wi-Fi control
  setupWebServer();


  Serial.println(
    "================================"
  );

  Serial.println(
    "      ROBO RACER READY"
  );

  Serial.println(
    "       By Pratyoosh Shukla"
  );

  Serial.println(
    "================================"
  );
}


// ============================================================================
//                                   LOOP
// ============================================================================

void loop() {

  // Handle incoming web requests
  server.handleClient();


  uint32_t now =
    millis();


  // --------------------------------------------------------------------------
  // RC STATUS
  // --------------------------------------------------------------------------

  bool rcValid =
    (now - ch1_lastUpdate) <
      RC_FAILSAFE_MS
    &&
    (now - ch2_lastUpdate) <
      RC_FAILSAFE_MS;


  bool rcActive =
    rcValid
    &&
    (
      abs(
        (int)ch1_pulse -
        RC_PULSE_MID
      )
      >= RC_TAKEOVER_DEADBAND

      ||

      abs(
        (int)ch2_pulse -
        RC_PULSE_MID
      )
      >= RC_TAKEOVER_DEADBAND
    );


  // --------------------------------------------------------------------------
  // PHONE STATUS
  // --------------------------------------------------------------------------

  bool phoneValid =
    (now - lastPhoneCmdTime) <
    WIFI_FAILSAFE_MS;


  // --------------------------------------------------------------------------
  // AUTOMATIC CONTROL ARBITRATION
  // --------------------------------------------------------------------------

  if(rcActive){

    // RC control has priority

    int throttle =
      map(
        constrain(
          (int)ch2_pulse,
          RC_PULSE_MIN,
          RC_PULSE_MAX
        ),

        RC_PULSE_MIN,
        RC_PULSE_MAX,

        -MOTOR_MAX,
        MOTOR_MAX
      );


    int steering =
      map(
        constrain(
          (int)ch1_pulse,
          RC_PULSE_MIN,
          RC_PULSE_MAX
        ),

        RC_PULSE_MIN,
        RC_PULSE_MAX,

        -MOTOR_MAX,
        MOTOR_MAX
      );


    // RC throttle deadband
    if(
      abs(
        (int)ch2_pulse -
        RC_PULSE_MID
      ) < RC_DEADBAND
    ){

      throttle = 0;
    }


    // RC steering deadband
    if(
      abs(
        (int)ch1_pulse -
        RC_PULSE_MID
      ) < RC_DEADBAND
    ){

      steering = 0;
    }


    driveDifferential(
      throttle,
      steering
    );

  }

  else if(phoneValid){

    // Phone control

    driveDifferential(
      phoneThrottle,
      phoneSteering
    );  

  }

  else{

    // No valid control source

    stopMotors();
  }
}
