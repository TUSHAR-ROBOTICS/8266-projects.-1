#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ── WiFi credentials ──────────────────────────────────────────
const char* ssid     = "tushar";
const char* password = "tushar12";

ESP8266WebServer server(80);

// ── OLED ──────────────────────────────────────────────────────
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ── Motor pins ────────────────────────────────────────────────
const int pwmMotorA = 5;  // D1
const int pwmMotorB = 4;  // D2
const int dirMotorA = 0;  // D3
const int dirMotorB = 2;  // D4

int motorSpeed = 700;
String currentCommand = "STOP";
bool emergencyStopped = false;   // 🚨 NEW: lock-out flag

// ═════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);

  pinMode(pwmMotorA, OUTPUT);
  pinMode(pwmMotorB, OUTPUT);
  pinMode(dirMotorA, OUTPUT);
  pinMode(dirMotorB, OUTPUT);

  Wire.begin(13, 12);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  startupSequence();
  connectWiFi();
  setupRoutes();
  server.begin();
}

// ─────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  updateDisplay();
  animateEyesTick();
}

// ══ WiFi ═════════════════════════════════════════════════════
void connectWiFi() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 25);
  display.print("Connecting WiFi...");
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);  display.print("Connected!");
  display.setCursor(0, 28);  display.print("Open browser:");
  display.setCursor(0, 44);  display.print(WiFi.localIP());
  display.display();
  delay(3000);
}

// ══ Web Routes ════════════════════════════════════════════════
void setupRoutes() {
  server.on("/", []() {
    server.send(200, "text/html", getControlPage());
  });

  server.on("/cmd", []() {
    if (server.hasArg("action")) {
      String action = server.arg("action");
      handleCommand(action);
      server.send(200, "text/plain", "OK");
    }
  });

  // 🚨 NEW: dedicated emergency stop route
  server.on("/estop", []() {
    emergencyStopHardware();
    server.send(200, "text/plain", "ESTOP");
  });

  // 🚨 NEW: reset emergency stop route
  server.on("/reset", []() {
    emergencyStopped = false;
    currentCommand = "STOP";
    server.send(200, "text/plain", "RESET");
  });
}

// ══ Commands ══════════════════════════════════════════════════
void handleCommand(String cmd) {
  // 🚨 Block all movement while emergency stop is active
  if (emergencyStopped) return;

  cmd.toUpperCase();
  currentCommand = cmd;

  if      (cmd == "FORWARD")  forward();
  else if (cmd == "BACKWARD") backward();
  else if (cmd == "LEFT")     turnLeft();
  else if (cmd == "RIGHT")    turnRight();
  else                        stopCar();
}

// 🚨 NEW: hardware-level emergency stop
void emergencyStopHardware() {
  emergencyStopped = true;
  currentCommand   = "E-STOP";
  // Cut power to both motors immediately
  analogWrite(pwmMotorA, 0);
  analogWrite(pwmMotorB, 0);
  digitalWrite(dirMotorA, LOW);
  digitalWrite(dirMotorB, LOW);
}

// ══ Motors ════════════════════════════════════════════════════
void forward() {
  digitalWrite(dirMotorA, LOW);  digitalWrite(dirMotorB, LOW);
  analogWrite(pwmMotorA, motorSpeed);  analogWrite(pwmMotorB, motorSpeed);
}
void backward() {
  digitalWrite(dirMotorA, HIGH); digitalWrite(dirMotorB, HIGH);
  analogWrite(pwmMotorA, motorSpeed);  analogWrite(pwmMotorB, motorSpeed);
}
void turnLeft() {
  digitalWrite(dirMotorA, HIGH); digitalWrite(dirMotorB, LOW);
  analogWrite(pwmMotorA, motorSpeed);  analogWrite(pwmMotorB, motorSpeed);
}
void turnRight() {
  digitalWrite(dirMotorA, LOW);  digitalWrite(dirMotorB, HIGH);
  analogWrite(pwmMotorA, motorSpeed);  analogWrite(pwmMotorB, motorSpeed);
}
void stopCar() {
  analogWrite(pwmMotorA, 0);
  analogWrite(pwmMotorB, 0);
}

// ══ OLED Display ══════════════════════════════════════════════
void updateDisplay() {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(30, 55);
  display.print("CMD: ");
  display.print(currentCommand);
  display.display();
}

unsigned long lastEyeUpdate = 0;
int eyePhase = 0;
int eyeHeights[] = {30, 10, 3, 10};
int eyeDelays[]  = {700, 120, 120, 120};

void animateEyesTick() {
  // 🚨 Show X eyes when emergency stopped
  if (emergencyStopped) { drawXEyes(); return; }

  if (millis() - lastEyeUpdate > eyeDelays[eyePhase]) {
    lastEyeUpdate = millis();
    drawEyes(eyeHeights[eyePhase]);
    eyePhase = (eyePhase + 1) % 4;
  }
}

void drawEyes(int height) {
  display.clearDisplay();
  int eyeWidth = 40;
  display.fillRoundRect(10, 15 + (30 - height) / 2, eyeWidth, height, 10, WHITE);
  display.fillRoundRect(78, 15 + (30 - height) / 2, eyeWidth, height, 10, WHITE);
  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print(WiFi.localIP());
}

// 🚨 NEW: X eyes on emergency stop
void drawXEyes() {
  display.clearDisplay();
  // Left X eye
  display.drawLine(10, 15, 49, 44, WHITE);
  display.drawLine(49, 15, 10, 44, WHITE);
  // Right X eye
  display.drawLine(78, 15, 117, 44, WHITE);
  display.drawLine(117, 15, 78, 44, WHITE);
  // Warning text
  display.setTextSize(1);
  display.setCursor(18, 52);
  display.print("!! E-STOP !!");
  display.display();
}

// ══ Startup ═══════════════════════════════════════════════════
void startupSequence() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(20, 20);
  display.print("WELCOME");
  display.display();
  delay(2000);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 25); display.print("Service Robot");
  display.setCursor(15, 40); display.print("Starting...");
  display.display();
  delay(1500);

  loadingAnimation();
}

void loadingAnimation() {
  for (int i = 0; i <= 100; i += 10) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(40, 10);
    display.print("Loading");
    display.drawRect(14, 30, 100, 10, WHITE);
    display.fillRect(14, 30, i, 10, WHITE);
    display.display();
    delay(200);
  }
}

// ══ HTML Controller Page ══════════════════════════════════════
String getControlPage() {
  return R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>🚗 Car Controller</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      background: #0f0f1a;
      color: #fff;
      font-family: 'Segoe UI', sans-serif;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      min-height: 100vh;
      gap: 20px;
    }
    h1 { font-size: 1.8rem; letter-spacing: 3px; color: #00e5ff; }

    #status {
      font-size: 1rem; color: #aaa;
      background: #1a1a2e;
      padding: 8px 24px;
      border-radius: 20px;
    }
    #status span { color: #00e5ff; font-weight: bold; }

    /* 🚨 Emergency banner — hidden by default */
    #estop-banner {
      display: none;
      background: #ff000022;
      border: 2px solid #ff4444;
      border-radius: 12px;
      padding: 10px 30px;
      color: #ff4444;
      font-size: 1.1rem;
      font-weight: bold;
      letter-spacing: 2px;
      animation: pulse 0.8s infinite alternate;
    }
    @keyframes pulse { from { opacity: 1; } to { opacity: 0.4; } }

    .grid {
      display: grid;
      grid-template-columns: repeat(3, 80px);
      grid-template-rows: repeat(3, 80px);
      gap: 10px;
    }
    .btn {
      background: #1a1a2e;
      border: 2px solid #00e5ff33;
      border-radius: 14px;
      color: #00e5ff;
      font-size: 1.6rem;
      cursor: pointer;
      transition: background 0.1s, transform 0.1s;
      display: flex; align-items: center; justify-content: center;
      user-select: none;
    }
    .btn:active, .btn.active {
      background: #00e5ff22;
      border-color: #00e5ff;
      transform: scale(0.95);
    }
    .btn.disabled {
      opacity: 0.3;
      cursor: not-allowed;
      pointer-events: none;
    }

    /* 🚨 E-STOP button */
    #btn-ESTOP {
      grid-column: 1 / 4;
      width: 100%;
      height: 70px;
      background: #330000;
      border: 2px solid #ff4444;
      border-radius: 14px;
      color: #ff4444;
      font-size: 1.1rem;
      font-weight: bold;
      letter-spacing: 2px;
      cursor: pointer;
      transition: background 0.1s;
      display: flex; align-items: center; justify-content: center;
      gap: 10px;
    }
    #btn-ESTOP:hover  { background: #550000; }
    #btn-ESTOP:active { background: #ff000033; transform: scale(0.98); }

    /* Reset button */
    #btn-RESET {
      background: #003300;
      border: 2px solid #44ff44;
      border-radius: 14px;
      color: #44ff44;
      font-size: 0.85rem;
      font-weight: bold;
      letter-spacing: 1px;
      padding: 10px 28px;
      cursor: pointer;
      display: none;    /* shown only after E-STOP */
    }
    #btn-RESET:hover { background: #004400; }

    .hint { font-size: 0.8rem; color: #555; letter-spacing: 1px; }
  </style>
</head>
<body>
  <h1>🚗 CAR CONTROL</h1>
  <div id="status">Command: <span id="cmd">STOP</span></div>
  <div id="estop-banner">🚨 EMERGENCY STOP ACTIVE 🚨</div>

  <div class="grid">
    <div></div>
    <button class="btn" id="btn-FORWARD"  ontouchstart="send('FORWARD')"  ontouchend="send('STOP')">▲</button>
    <div></div>
    <button class="btn" id="btn-LEFT"     ontouchstart="send('LEFT')"     ontouchend="send('STOP')">◄</button>
    <button class="btn" id="btn-STOP"                                     onclick="send('STOP')">■</button>
    <button class="btn" id="btn-RIGHT"    ontouchstart="send('RIGHT')"    ontouchend="send('STOP')">►</button>
    <div></div>
    <button class="btn" id="btn-BACKWARD" ontouchstart="send('BACKWARD')" ontouchend="send('STOP')">▼</button>
    <div></div>
  </div>

  <!-- 🚨 Emergency stop button (full width) -->
  <button id="btn-ESTOP" onclick="emergencyStop()">🚨 EMERGENCY STOP &nbsp;[I]</button>
  <button id="btn-RESET" onclick="resetStop()">✅ RESET &amp; RESUME</button>

  <div class="hint">W A S D · Arrow keys · Hold to move &nbsp;|&nbsp; I = Emergency Stop</div>

<script>
  const keyMap = {
    'w': 'FORWARD',  'ArrowUp':    'FORWARD',
    's': 'BACKWARD', 'ArrowDown':  'BACKWARD',
    'a': 'LEFT',     'ArrowLeft':  'LEFT',
    'd': 'RIGHT',    'ArrowRight': 'RIGHT'
  };

  let active = null;
  let stopped = false;

  // ── Normal send ─────────────────────────────────────────────
  function send(action) {
    if (stopped) return;                         // 🚨 blocked
    document.getElementById('cmd').textContent = action;
    document.querySelectorAll('.btn').forEach(b => b.classList.remove('active'));
    const b = document.getElementById('btn-' + action);
    if (b) b.classList.add('active');
    fetch('/cmd?action=' + action).catch(() => {});
  }

  // 🚨 Emergency stop ─────────────────────────────────────────
  function emergencyStop() {
    stopped = true;
    active  = null;

    // Update UI
    document.getElementById('cmd').textContent = '🚨 E-STOP';
    document.getElementById('estop-banner').style.display = 'block';
    document.getElementById('btn-RESET').style.display    = 'block';

    // Disable all movement buttons
    ['FORWARD','BACKWARD','LEFT','RIGHT','STOP'].forEach(id => {
      const b = document.getElementById('btn-' + id);
      if (b) b.classList.add('disabled');
    });

    // Hit the dedicated /estop route for instant hardware stop
    fetch('/estop').catch(() => {});
  }

  // ✅ Reset ───────────────────────────────────────────────────
  function resetStop() {
    stopped = false;

    document.getElementById('cmd').textContent = 'STOP';
    document.getElementById('estop-banner').style.display = 'none';
    document.getElementById('btn-RESET').style.display    = 'none';

    ['FORWARD','BACKWARD','LEFT','RIGHT','STOP'].forEach(id => {
      const b = document.getElementById('btn-' + id);
      if (b) b.classList.remove('disabled');
    });

    fetch('/reset').catch(() => {});
  }

  // ── Keyboard ────────────────────────────────────────────────
  document.addEventListener('keydown', e => {
    // 🚨 I key → emergency stop
    if (e.key === 'i' || e.key === 'I') { emergencyStop(); return; }

    const cmd = keyMap[e.key];
    if (cmd && active !== cmd) { active = cmd; send(cmd); }
  });

  document.addEventListener('keyup', e => {
    if (keyMap[e.key]) { active = null; send('STOP'); }
  });
</script>
</body>
</html>
)rawhtml";
}