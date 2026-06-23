#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ─── OLED ────────────────────────────────────────────────
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ─── TB6612FNG PINS ──────────────────────────────────────
// Motor A = LEFT  side (both left wheels in parallel)
// Motor B = RIGHT side (both right wheels in parallel)
#define STBY 0    // D3
#define AIN1 2    // D4
#define AIN2 14   // D5
#define PWMA 12   // D6
#define BIN1 13   // D7
#define BIN2 15   // D8
#define PWMB 3    // RX

// ─── CONFIG ──────────────────────────────────────────────
const char* AP_SSID     = "RobotCar";
const char* AP_PASS     = "12345678";
const int   BASE_SPEED  = 850;   // 0-1023
const int   TURN_SPEED  = 750;

ESP8266WebServer server(80);
String currentMode = "STOP";

// ─── OLED HELPER ─────────────────────────────────────────
void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);  display.println("4WD WIFI ROBOT");
  display.setCursor(0,10);  display.println(WiFi.softAPIP());
  display.setCursor(0,20);  display.print("Mode: "); display.println(currentMode);
  display.display();
}

// ─── MOTOR PRIMITIVES ────────────────────────────────────
// Both left wheels share Motor A, both right wheels share Motor B.
// Wire: Motor A Out1/Out2 → LEFT  front + rear in parallel
//       Motor B Out1/Out2 → RIGHT front + rear in parallel

void setMotors(int leftDir, int leftPWM, int rightDir, int rightPWM) {
  // leftDir / rightDir: 1=fwd, -1=rev, 0=brake
  if (leftDir >= 0) { digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW); }
  else              { digitalWrite(AIN1, LOW);  digitalWrite(AIN2, HIGH); }

  if (rightDir >= 0) { digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW); }
  else               { digitalWrite(BIN1, LOW);  digitalWrite(BIN2, HIGH); }

  analogWrite(PWMA, leftPWM);
  analogWrite(PWMB, rightPWM);
}

void stopMotors() {
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
  currentMode = "STOP";
  updateOLED();
}

void forward() {
  setMotors(1, BASE_SPEED, 1, BASE_SPEED);
  currentMode = "FORWARD";
  updateOLED();
}

void reverse() {
  setMotors(-1, BASE_SPEED, -1, BASE_SPEED);
  currentMode = "REVERSE";
  updateOLED();
}

// Tank-style turns: all 4 wheels move, opposite sides spin different directions
void turnLeft() {
  // Left wheels reverse, right wheels forward → pivot left
  setMotors(-1, TURN_SPEED, 1, TURN_SPEED);
  currentMode = "LEFT";
  updateOLED();
}

void turnRight() {
  // Left wheels forward, right wheels reverse → pivot right
  setMotors(1, TURN_SPEED, -1, TURN_SPEED);
  currentMode = "RIGHT";
  updateOLED();
}

// Gentle arc turns (one side slower, not reversed)
void arcLeft() {
  setMotors(1, TURN_SPEED / 2, 1, BASE_SPEED);
  currentMode = "ARC-L";
  updateOLED();
}

void arcRight() {
  setMotors(1, BASE_SPEED, 1, TURN_SPEED / 2);
  currentMode = "ARC-R";
  updateOLED();
}

// ─── WEB PAGE ────────────────────────────────────────────
void handleRoot() {
  String html = R"rawlit(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>4WD Robot Controller</title>
<style>
  :root {
    --bg:      #0d0f14;
    --surface: #161b26;
    --border:  #2a3040;
    --accent:  #00e5ff;
    --accent2: #ff4d6d;
    --text:    #e4eaf4;
    --muted:   #5a6478;
    --fwd:     #00e5ff;
    --rev:     #ff4d6d;
    --turn:    #f4c430;
    --stop:    #ff6b35;
    --radius:  14px;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Segoe UI', system-ui, sans-serif;
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    padding: 20px;
    user-select: none;
  }
  header {
    text-align: center;
    margin-bottom: 28px;
  }
  header h1 {
    font-size: 1.5rem;
    letter-spacing: 0.25em;
    text-transform: uppercase;
    color: var(--accent);
    text-shadow: 0 0 18px rgba(0,229,255,0.4);
  }
  header p {
    color: var(--muted);
    font-size: 0.78rem;
    margin-top: 4px;
    letter-spacing: 0.1em;
  }

  /* Status bar */
  #status-bar {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 8px 20px;
    font-size: 0.75rem;
    letter-spacing: 0.12em;
    color: var(--muted);
    margin-bottom: 28px;
    display: flex;
    align-items: center;
    gap: 10px;
  }
  #status-dot {
    width: 8px; height: 8px;
    border-radius: 50%;
    background: var(--accent);
    box-shadow: 0 0 8px var(--accent);
    animation: pulse 1.6s ease-in-out infinite;
  }
  @keyframes pulse {
    0%,100% { opacity: 1; }
    50%      { opacity: 0.35; }
  }
  #mode-label { color: var(--text); font-weight: 600; }

  /* Car SVG */
  .car-wrap {
    margin-bottom: 24px;
  }
  .car-wrap svg { display: block; }

  /* D-PAD grid */
  .dpad {
    display: grid;
    grid-template-columns: repeat(3, 80px);
    grid-template-rows: repeat(3, 80px);
    gap: 8px;
  }
  @media (max-width: 360px) {
    .dpad { grid-template-columns: repeat(3, 70px); grid-template-rows: repeat(3, 70px); }
  }

  .btn {
    border: none;
    border-radius: var(--radius);
    font-size: 1.5rem;
    font-weight: 700;
    cursor: pointer;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 4px;
    background: var(--surface);
    border: 1.5px solid var(--border);
    color: var(--text);
    transition: transform 0.08s, box-shadow 0.08s, background 0.12s;
    -webkit-tap-highlight-color: transparent;
  }
  .btn span.lbl { font-size: 0.55rem; letter-spacing: 0.1em; color: var(--muted); }
  .btn:active, .btn.active {
    transform: scale(0.93);
  }
  .btn-fwd  { border-color: var(--fwd);  color: var(--fwd);  box-shadow: 0 0 14px rgba(0,229,255,0.15); }
  .btn-rev  { border-color: var(--rev);  color: var(--rev);  box-shadow: 0 0 14px rgba(255,77,109,0.15); }
  .btn-turn { border-color: var(--turn); color: var(--turn); box-shadow: 0 0 14px rgba(244,196,48,0.12); }
  .btn-stop { border-color: var(--stop); color: var(--stop); box-shadow: 0 0 14px rgba(255,107,53,0.15); font-size: 0.85rem; }

  .btn-fwd:active, .btn-fwd.active   { background: rgba(0,229,255,0.12);  }
  .btn-rev:active, .btn-rev.active   { background: rgba(255,77,109,0.12); }
  .btn-turn:active, .btn-turn.active { background: rgba(244,196,48,0.10); }
  .btn-stop:active, .btn-stop.active { background: rgba(255,107,53,0.12); }

  /* Speed slider */
  .speed-wrap {
    margin-top: 28px;
    width: 260px;
    text-align: center;
  }
  .speed-wrap label {
    font-size: 0.72rem;
    letter-spacing: 0.12em;
    color: var(--muted);
    display: block;
    margin-bottom: 8px;
  }
  input[type=range] {
    -webkit-appearance: none;
    width: 100%;
    height: 5px;
    border-radius: 3px;
    background: var(--border);
    outline: none;
  }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 20px; height: 20px;
    border-radius: 50%;
    background: var(--accent);
    box-shadow: 0 0 10px rgba(0,229,255,0.5);
    cursor: pointer;
  }
  #speed-val {
    margin-top: 6px;
    font-size: 0.78rem;
    color: var(--accent);
    font-weight: 600;
  }

  /* Keyboard hint */
  .hint {
    margin-top: 22px;
    font-size: 0.68rem;
    color: var(--muted);
    letter-spacing: 0.08em;
    text-align: center;
  }
  .key {
    display: inline-block;
    border: 1px solid var(--border);
    border-radius: 4px;
    padding: 1px 5px;
    font-family: monospace;
    color: var(--text);
  }
</style>
</head>
<body>

<header>
  <h1>4WD Robot</h1>
  <p>WiFi Controller &mdash; ESP8266</p>
</header>

<div id="status-bar">
  <div id="status-dot"></div>
  MODE: <span id="mode-label">STOP</span>
</div>

<!-- Tiny car diagram showing wheel activity -->
<div class="car-wrap">
<svg width="110" height="140" viewBox="0 0 110 140" xmlns="http://www.w3.org/2000/svg">
  <!-- Body -->
  <rect x="22" y="30" width="66" height="80" rx="10" fill="#161b26" stroke="#2a3040" stroke-width="1.5"/>
  <!-- Windshield -->
  <rect x="32" y="38" width="46" height="24" rx="5" fill="#0d0f14" stroke="#2a3040" stroke-width="1"/>
  <!-- Arrow indicator -->
  <polygon id="car-arrow" points="55,46 62,58 55,55 48,58" fill="#5a6478"/>
  <!-- Wheels: FL, FR, RL, RR -->
  <rect id="wFL" x="6"  y="32" width="16" height="28" rx="4" fill="#2a3040" stroke="#5a6478" stroke-width="1.5"/>
  <rect id="wFR" x="88" y="32" width="16" height="28" rx="4" fill="#2a3040" stroke="#5a6478" stroke-width="1.5"/>
  <rect id="wRL" x="6"  y="80" width="16" height="28" rx="4" fill="#2a3040" stroke="#5a6478" stroke-width="1.5"/>
  <rect id="wRR" x="88" y="80" width="16" height="28" rx="4" fill="#2a3040" stroke="#5a6478" stroke-width="1.5"/>
</svg>
</div>

<!-- D-Pad -->
<div class="dpad">
  <!--row1-->
  <div></div>
  <button class="btn btn-fwd" id="btn-fwd"   ontouchstart="cmd('forward')" ontouchend="cmd('stop')" onmousedown="cmd('forward')" onmouseup="cmd('stop')">&#9650;<span class="lbl">FWD</span></button>
  <div></div>
  <!--row2-->
  <button class="btn btn-turn" id="btn-left"  ontouchstart="cmd('left')"    ontouchend="cmd('stop')" onmousedown="cmd('left')"    onmouseup="cmd('stop')">&#9664;<span class="lbl">LEFT</span></button>
  <button class="btn btn-stop" id="btn-stop"  onclick="cmd('stop')">&#9632;<span class="lbl">STOP</span></button>
  <button class="btn btn-turn" id="btn-right" ontouchstart="cmd('right')"   ontouchend="cmd('stop')" onmousedown="cmd('right')"   onmouseup="cmd('stop')">&#9654;<span class="lbl">RIGHT</span></button>
  <!--row3-->
  <div></div>
  <button class="btn btn-rev"  id="btn-rev"   ontouchstart="cmd('reverse')" ontouchend="cmd('stop')" onmousedown="cmd('reverse')" onmouseup="cmd('stop')">&#9660;<span class="lbl">REV</span></button>
  <div></div>
</div>

<div class="speed-wrap">
  <label>SPEED</label>
  <input type="range" id="speed" min="30" max="100" value="85">
  <div id="speed-val">85%</div>
</div>

<p class="hint">
  <span class="key">W</span> <span class="key">A</span> <span class="key">S</span> <span class="key">D</span> &nbsp;or arrow keys &nbsp;|&nbsp; hold to move
</p>

<script>
const WHEEL_FWD  = '#00e5ff';
const WHEEL_REV  = '#ff4d6d';
const WHEEL_OFF  = '#2a3040';
const WHEEL_TURN = '#f4c430';

const wFL = document.getElementById('wFL');
const wFR = document.getElementById('wFR');
const wRL = document.getElementById('wRL');
const wRR = document.getElementById('wRR');
const arrow = document.getElementById('car-arrow');
const modeLabel = document.getElementById('mode-label');

function setWheels(fl, fr, rl, rr) {
  wFL.style.fill = fl; wFR.style.fill = fr;
  wRL.style.fill = rl; wRR.style.fill = rr;
}

let currentCmd = 'stop';
let speedVal = 85;
let lastSent = '';
let sendTimer = null;

document.getElementById('speed').addEventListener('input', function() {
  speedVal = this.value;
  document.getElementById('speed-val').textContent = speedVal + '%';
});

function updateVisuals(action) {
  // Reset all button highlights
  ['btn-fwd','btn-rev','btn-left','btn-right','btn-stop'].forEach(id => {
    const el = document.getElementById(id);
    if(el) el.classList.remove('active');
  });
  modeLabel.textContent = action.toUpperCase();

  switch(action) {
    case 'forward':
      setWheels(WHEEL_FWD, WHEEL_FWD, WHEEL_FWD, WHEEL_FWD);
      arrow.setAttribute('fill', WHEEL_FWD);
      document.getElementById('btn-fwd').classList.add('active');
      break;
    case 'reverse':
      setWheels(WHEEL_REV, WHEEL_REV, WHEEL_REV, WHEEL_REV);
      arrow.setAttribute('fill', WHEEL_REV);
      document.getElementById('btn-rev').classList.add('active');
      break;
    case 'left':
      setWheels(WHEEL_REV, WHEEL_TURN, WHEEL_REV, WHEEL_TURN);
      arrow.setAttribute('fill', WHEEL_TURN);
      document.getElementById('btn-left').classList.add('active');
      break;
    case 'right':
      setWheels(WHEEL_TURN, WHEEL_REV, WHEEL_TURN, WHEEL_REV);
      arrow.setAttribute('fill', WHEEL_TURN);
      document.getElementById('btn-right').classList.add('active');
      break;
    default:
      setWheels(WHEEL_OFF, WHEEL_OFF, WHEEL_OFF, WHEEL_OFF);
      arrow.setAttribute('fill', '#5a6478');
      document.getElementById('btn-stop').classList.add('active');
  }
}

function cmd(action) {
  if(action === currentCmd) return;
  currentCmd = action;
  updateVisuals(action);
  // Throttle requests to every 80ms
  if(sendTimer) return;
  sendTimer = setTimeout(() => {
    sendTimer = null;
    const url = '/' + currentCmd + '?spd=' + speedVal;
    if(url !== lastSent) {
      fetch(url).catch(() => {});
      lastSent = url;
    }
  }, 80);
}

// Keyboard support
const keyMap = {
  'ArrowUp':'forward','w':'forward','W':'forward',
  'ArrowDown':'reverse','s':'reverse','S':'reverse',
  'ArrowLeft':'left','a':'left','A':'left',
  'ArrowRight':'right','d':'right','D':'right',
  ' ':'stop'
};
const held = new Set();
document.addEventListener('keydown', e => {
  if(held.has(e.key)) return;
  held.add(e.key);
  const action = keyMap[e.key];
  if(action) { e.preventDefault(); cmd(action); }
});
document.addEventListener('keyup', e => {
  held.delete(e.key);
  if(keyMap[e.key] && keyMap[e.key] !== 'stop') cmd('stop');
});

// Init
updateVisuals('stop');
</script>
</body>
</html>
)rawlit";
  server.send(200, "text/html", html);
}

// ─── SPEED HELPER ────────────────────────────────────────
int getSpeed() {
  // Optional ?spd=0-100 query param
  if (server.hasArg("spd")) {
    int pct = server.arg("spd").toInt();
    pct = constrain(pct, 0, 100);
    return map(pct, 0, 100, 0, 1023);
  }
  return BASE_SPEED;
}

// ─── SETUP ───────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Motor pins
  pinMode(STBY, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);
  digitalWrite(STBY, HIGH);

  // OLED
  Wire.begin(4, 5); // SDA=D2, SCL=D1
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { while (1); }

  // WiFi AP
  WiFi.softAP(AP_SSID, AP_PASS);
  updateOLED();

  // Routes
  server.on("/", handleRoot);

  server.on("/forward", []() {
    int spd = getSpeed();
    setMotors(1, spd, 1, spd);
    currentMode = "FORWARD"; updateOLED();
    server.send(200, "text/plain", "ok");
  });
  server.on("/reverse", []() {
    int spd = getSpeed();
    setMotors(-1, spd, -1, spd);
    currentMode = "REVERSE"; updateOLED();
    server.send(200, "text/plain", "ok");
  });
  server.on("/left", []() {
    int spd = getSpeed();
    // Tank pivot: left wheels reverse, right wheels forward
    setMotors(-1, spd, 1, spd);
    currentMode = "LEFT"; updateOLED();
    server.send(200, "text/plain", "ok");
  });
  server.on("/right", []() {
    int spd = getSpeed();
    // Tank pivot: left wheels forward, right wheels reverse
    setMotors(1, spd, -1, spd);
    currentMode = "RIGHT"; updateOLED();
    server.send(200, "text/plain", "ok");
  });
  server.on("/stop", []() {
    stopMotors();
    server.send(200, "text/plain", "ok");
  });

  server.begin();
}

// ─── LOOP ────────────────────────────────────────────────
void loop() {
  server.handleClient();
}
