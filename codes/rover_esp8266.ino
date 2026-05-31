/*
  ============================================================
  WiFi Rover Controller - NodeMCU ESP8266
  ============================================================
  Motor Pins : D1, D2, D3, D4
  Servo Pins : D5 (pan/horizontal), D6 (tilt/vertical)
  Control    : WebSocket for zero-delay commands
  ============================================================

  WIRING:
  Motor Driver (L298N or L293D):
    IN1 → D1 (GPIO5)
    IN2 → D2 (GPIO4)
    IN3 → D3 (GPIO0)
    IN4 → D4 (GPIO2)

  Servo 1 (Pan  - Left/Right) → D5 (GPIO14)
  Servo 2 (Tilt - Up/Down)    → D6 (GPIO12)

  LIBRARIES NEEDED (Install via Arduino Library Manager):
    - ESP8266WiFi       (built-in with ESP8266 board package)
    - ESPAsyncWebServer (by ESP Async WebServer)
    - ESPAsyncTCP       (required by ESPAsyncWebServer)
    - Servo             (built-in)

  BOARD SETTINGS:
    Board: NodeMCU 1.0 (ESP-12E Module)
    CPU Frequency: 160 MHz  ← IMPORTANT for WebSocket performance
    Upload Speed: 921600
*/

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Servo.h>

// ─── WiFi Credentials ────────────────────────────────────────
const char* SSID     = "RoverAP";      // AP name your laptop connects to
const char* PASSWORD = "rover1234";    // Min 8 chars

// ─── Motor Pins (NodeMCU GPIO mapping) ───────────────────────
#define IN1  5   // D1 - Left Motor Forward
#define IN2  4   // D2 - Left Motor Backward
#define IN3  0   // D3 - Right Motor Forward
#define IN4  2   // D4 - Right Motor Backward

// ─── Servo Pins ───────────────────────────────────────────────
#define SERVO_PAN_PIN   14  // D5 - Horizontal (Left/Right)
#define SERVO_TILT_PIN  12  // D6 - Vertical   (Up/Down)

// ─── Servo Angle Limits ───────────────────────────────────────
#define PAN_MIN    30
#define PAN_MAX   150
#define PAN_MID    90
#define TILT_MIN   40
#define TILT_MAX  130
#define TILT_MID   90
#define SERVO_STEP  3   // degrees per command

// ─── Objects ──────────────────────────────────────────────────
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

Servo servoPan;
Servo servoTilt;

int panAngle  = PAN_MID;
int tiltAngle = TILT_MID;

// ─── Motor Control ────────────────────────────────────────────
void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void moveForward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void moveBackward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void turnLeft() {
  // Pivot: left motor backward, right motor forward
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turnRight() {
  // Pivot: left motor forward, right motor backward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

// ─── Command Parser ───────────────────────────────────────────
// Commands from laptop:
//   FORWARD / BACKWARD / LEFT / RIGHT / STOP
//   PAN:90  (set pan to angle)
//   TILT:75 (set tilt to angle)
//   PAN_LEFT / PAN_RIGHT / TILT_UP / TILT_DOWN
//   SERVO_RESET

void handleCommand(const String& cmd) {
  if (cmd == "FORWARD")       { moveForward();  return; }
  if (cmd == "BACKWARD")      { moveBackward(); return; }
  if (cmd == "LEFT")          { turnLeft();     return; }
  if (cmd == "RIGHT")         { turnRight();    return; }
  if (cmd == "STOP")          { stopMotors();   return; }

  // Servo incremental
  if (cmd == "PAN_LEFT") {
    panAngle = max(PAN_MIN, panAngle - SERVO_STEP);
    servoPan.write(panAngle);
    return;
  }
  if (cmd == "PAN_RIGHT") {
    panAngle = min(PAN_MAX, panAngle + SERVO_STEP);
    servoPan.write(panAngle);
    return;
  }
  if (cmd == "TILT_UP") {
    tiltAngle = max(TILT_MIN, tiltAngle - SERVO_STEP);
    servoTilt.write(tiltAngle);
    return;
  }
  if (cmd == "TILT_DOWN") {
    tiltAngle = min(TILT_MAX, tiltAngle + SERVO_STEP);
    servoTilt.write(tiltAngle);
    return;
  }
  if (cmd == "SERVO_RESET") {
    panAngle  = PAN_MID;
    tiltAngle = TILT_MID;
    servoPan.write(panAngle);
    servoTilt.write(tiltAngle);
    return;
  }

  // Servo absolute: "PAN:90" or "TILT:75"
  if (cmd.startsWith("PAN:")) {
    int angle = cmd.substring(4).toInt();
    panAngle  = constrain(angle, PAN_MIN, PAN_MAX);
    servoPan.write(panAngle);
    return;
  }
  if (cmd.startsWith("TILT:")) {
    int angle = cmd.substring(5).toInt();
    tiltAngle = constrain(angle, TILT_MIN, TILT_MAX);
    servoTilt.write(tiltAngle);
    return;
  }
}

// ─── WebSocket Event Handler ──────────────────────────────────
void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {

  if (type == WS_EVT_CONNECT) {
    Serial.printf("WS client #%u connected from %s\n",
                  client->id(), client->remoteIP().toString().c_str());
    client->text("CONNECTED");

  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WS client #%u disconnected\n", client->id());
    stopMotors(); // Safety: stop rover when connection drops

  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len
        && info->opcode == WS_TEXT) {
      String cmd = "";
      for (size_t i = 0; i < len; i++) cmd += (char)data[i];
      cmd.trim();
      Serial.println("CMD: " + cmd);
      handleCommand(cmd);
    }

  } else if (type == WS_EVT_ERROR) {
    stopMotors();
  }
}

// ─── Inline HTML Controller Page ─────────────────────────────
// Served at http://192.168.4.1 — open this on your laptop
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8">
<title>Rover Control</title>
<style>
  body{background:#0a0a0a;color:#00ff88;font-family:monospace;
       display:flex;flex-direction:column;align-items:center;
       justify-content:center;height:100vh;margin:0;user-select:none;}
  h2{letter-spacing:4px;font-size:1.2rem;margin-bottom:20px;
     color:#00ffcc;text-shadow:0 0 10px #00ffcc;}
  #status{font-size:.8rem;margin-bottom:20px;color:#888;}
  #status.ok{color:#00ff88;}
  .hint{font-size:.75rem;color:#444;margin-top:30px;line-height:2;}
  kbd{background:#1a1a1a;border:1px solid #333;border-radius:4px;
      padding:2px 6px;color:#00ff88;}
</style></head><body>
<h2>⬡ ROVER CONTROL ⬡</h2>
<div id="status">Connecting...</div>
<div class="hint">
  <kbd>W</kbd> Forward &nbsp;
  <kbd>S</kbd> Backward &nbsp;
  <kbd>A</kbd> Left &nbsp;
  <kbd>D</kbd> Right<br>
  <kbd>↑↓←→</kbd> Tilt/Pan Servo &nbsp;
  <kbd>R</kbd> Reset Servos<br>
  Hold key = continuous. Release = Stop.
</div>
<script>
const ws = new WebSocket('ws://' + location.hostname + '/ws');
const status = document.getElementById('status');

ws.onopen = () => { status.textContent = '● CONNECTED'; status.className='ok'; };
ws.onclose = () => { status.textContent = '○ DISCONNECTED'; status.className=''; };

const motorKeys = { w:'FORWARD', s:'BACKWARD', a:'LEFT', d:'RIGHT' };
const servoKeys = { ArrowLeft:'PAN_LEFT', ArrowRight:'PAN_RIGHT',
                    ArrowUp:'TILT_UP',    ArrowDown:'TILT_DOWN' };

const held = new Set();

function send(cmd) {
  if (ws.readyState === 1) ws.send(cmd);
}

document.addEventListener('keydown', e => {
  const k = e.key.toLowerCase();
  if (held.has(k)) return; // already held
  held.add(k);

  if (motorKeys[k])        { e.preventDefault(); send(motorKeys[k]); }
  else if (servoKeys[e.key]){ e.preventDefault(); send(servoKeys[e.key]); }
  else if (k === 'r')       { send('SERVO_RESET'); }
});

document.addEventListener('keyup', e => {
  const k = e.key.toLowerCase();
  held.delete(k);
  if (motorKeys[k]) send('STOP');
  // Servos stop automatically (positional)
});
</script></body></html>
)rawliteral";

// ─── Setup ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Motor pins
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  stopMotors();

  // Servos
  servoPan.attach(SERVO_PAN_PIN);
  servoTilt.attach(SERVO_TILT_PIN);
  servoPan.write(PAN_MID);
  servoTilt.write(TILT_MID);

  // Access Point (laptop connects to this WiFi)
  WiFi.mode(WIFI_AP);
  WiFi.softAP(SSID, PASSWORD);
  Serial.println("\nAP IP: " + WiFi.softAPIP().toString());
  // → Open  http://192.168.4.1  on laptop browser

  // WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Serve controller page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", INDEX_HTML);
  });

  // REST fallback (optional, for testing with curl)
  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (req->hasParam("v")) {
      handleCommand(req->getParam("v")->value());
      req->send(200, "text/plain", "OK");
    } else {
      req->send(400, "text/plain", "Missing ?v=COMMAND");
    }
  });

  server.begin();
  Serial.println("Server started. Open http://192.168.4.1 on laptop.");
}

// ─── Loop ─────────────────────────────────────────────────────
void loop() {
  ws.cleanupClients(); // Keep WebSocket memory clean
  // Nothing blocking here — async handles everything
}
