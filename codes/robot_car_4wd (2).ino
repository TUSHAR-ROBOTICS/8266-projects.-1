/*
  4WD WiFi Robot Car — ESP8266 + TB6612FNG
  =========================================
  OLED : D1(GPIO5)=SCL  D2(GPIO4)=SDA
  TB6612: STBY=D3(0) AIN1=D4(2) AIN2=D5(14) PWMA=D6(12)
          BIN1=D7(13) BIN2=D8(15) PWMB=RX(3)

  NOTE: GPIO3 (RX) has weak PWM support on ESP8266.
        We drive PWMA and PWMB with analogWrite but also
        call analogWriteFreq(1000) and analogWriteRange(1023)
        once in setup to synchronise both channels.

  Motor wiring:
    Channel A → Left-front  + Left-rear  (parallel)
    Channel B → Right-front + Right-rear (parallel)
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── OLED ─────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ── PINS (DO NOT CHANGE) ──────────────────────────────────
#define STBY  0   // D3
#define AIN1  2   // D4  — Left  direction 1
#define AIN2  14  // D5  — Left  direction 2
#define PWMA  12  // D6  — Left  speed
#define BIN1  13  // D7  — Right direction 1
#define BIN2  15  // D8  — Right direction 2
#define PWMB  3   // RX  — Right speed

// ── WIFI ─────────────────────────────────────────────────
const char* SSID = "RobotCar";
const char* PASS = "12345678";

// ── SPEED (0-1023) ────────────────────────────────────────
// Trim values let you compensate if one side is faster than other.
// Increase LEFT_TRIM if left side is slower, decrease if faster.
// Same for RIGHT_TRIM.
#define MAX_SPEED    900
#define LEFT_TRIM    0     // -100 to +100, adjust if car veers
#define RIGHT_TRIM   0     // -100 to +100

ESP8266WebServer server(80);
String currentMode = "STOP";

// ── MOTOR CORE ───────────────────────────────────────────
// dir: 1=forward  -1=reverse  0=stop
// spd: 0-1023
void driveMotors(int leftDir, int leftSpd, int rightDir, int rightSpd) {
  // Clamp speeds
  leftSpd  = constrain(leftSpd  + LEFT_TRIM,  0, 1023);
  rightSpd = constrain(rightSpd + RIGHT_TRIM, 0, 1023);

  // Left side (Channel A)
  if (leftDir > 0)       { digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);  }
  else if (leftDir < 0)  { digitalWrite(AIN1, LOW);  digitalWrite(AIN2, HIGH); }
  else                   { digitalWrite(AIN1, LOW);  digitalWrite(AIN2, LOW);  }

  // Right side (Channel B)
  if (rightDir > 0)      { digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);  }
  else if (rightDir < 0) { digitalWrite(BIN1, LOW);  digitalWrite(BIN2, HIGH); }
  else                   { digitalWrite(BIN1, LOW);  digitalWrite(BIN2, LOW);  }

  // Write PWM — both at same time to minimise lag
  analogWrite(PWMA, (leftDir  == 0) ? 0 : leftSpd);
  analogWrite(PWMB, (rightDir == 0) ? 0 : rightSpd);
}

// ── MOVEMENT COMMANDS ────────────────────────────────────
void motorStop() {
  driveMotors(0, 0, 0, 0);
  currentMode = "STOP";
}

void motorForward(int spd) {
  driveMotors(1, spd, 1, spd);
  currentMode = "FORWARD";
}

void motorReverse(int spd) {
  driveMotors(-1, spd, -1, spd);
  currentMode = "REVERSE";
}

void motorLeft(int spd) {
  // Tank turn: left wheels reverse, right wheels forward
  driveMotors(-1, spd, 1, spd);
  currentMode = "LEFT";
}

void motorRight(int spd) {
  // Tank turn: left wheels forward, right wheels reverse
  driveMotors(1, spd, -1, spd);
  currentMode = "RIGHT";
}

// ── OLED ─────────────────────────────────────────────────
void showOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,  0); display.println("4WD WIFI ROBOT");
  display.setCursor(0, 10); display.println(WiFi.softAPIP());
  display.setCursor(0, 20); display.print(">> "); display.println(currentMode);
  display.display();
}

// ── SPEED FROM URL PARAM ─────────────────────────────────
// ?spd=0-100  →  0-MAX_SPEED
int urlSpeed() {
  if (server.hasArg("spd")) {
    int pct = constrain(server.arg("spd").toInt(), 0, 100);
    return map(pct, 0, 100, 0, MAX_SPEED);
  }
  return MAX_SPEED;
}

// ── HTML ─────────────────────────────────────────────────
const char HTML_A[] PROGMEM =
  "<!DOCTYPE html><html lang='en'><head>"
  "<meta charset='UTF-8'>"
  "<meta name='viewport' content='width=device-width,initial-scale=1'>"
  "<title>4WD Robot</title>"
  "<style>"
  ":root{--bg:#0d0f14;--sur:#161b26;--brd:#2a3040;--acc:#00e5ff;"
  "--red:#ff4d6d;--yel:#f4c430;--org:#ff6b35;--txt:#e4eaf4;--mut:#5a6478}"
  "*{box-sizing:border-box;margin:0;padding:0}"
  "body{background:var(--bg);color:var(--txt);font-family:'Segoe UI',sans-serif;"
  "min-height:100vh;display:flex;flex-direction:column;align-items:center;"
  "justify-content:center;padding:20px;user-select:none;-webkit-user-select:none}"
  "h1{font-size:1.4rem;letter-spacing:.25em;text-transform:uppercase;"
  "color:var(--acc);text-shadow:0 0 16px rgba(0,229,255,.4);text-align:center}"
  "p.sub{color:var(--mut);font-size:.75rem;margin-top:4px;letter-spacing:.1em;text-align:center}"
  "#sbar{background:var(--sur);border:1px solid var(--brd);border-radius:14px;"
  "padding:8px 20px;font-size:.75rem;letter-spacing:.12em;color:var(--mut);"
  "margin:16px 0;display:flex;align-items:center;gap:10px}"
  "#dot{width:8px;height:8px;border-radius:50%;background:var(--acc);"
  "box-shadow:0 0 8px var(--acc);animation:pulse 1.6s ease-in-out infinite}"
  "@keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}"
  "#ml{color:var(--txt);font-weight:700;min-width:70px}"
  "svg{display:block;margin-bottom:18px}"
  ".dpad{display:grid;grid-template-columns:repeat(3,88px);"
  "grid-template-rows:repeat(3,88px);gap:8px}"
  ".btn{border:none;border-radius:14px;font-size:1.6rem;font-weight:700;"
  "cursor:pointer;display:flex;flex-direction:column;align-items:center;"
  "justify-content:center;gap:3px;background:var(--sur);"
  "border:1.5px solid var(--brd);color:var(--txt);"
  "transition:transform .07s,background .1s;touch-action:manipulation}"
  ".lbl{font-size:.54rem;letter-spacing:.1em;color:var(--mut)}"
  ".btn:active,.on{transform:scale(.91)}"
  ".F{border-color:var(--acc);color:var(--acc)}"
  ".R{border-color:var(--red);color:var(--red)}"
  ".T{border-color:var(--yel);color:var(--yel)}"
  ".S{border-color:var(--org);color:var(--org);font-size:.9rem}"
  ".F:active,.F.on{background:rgba(0,229,255,.13)}"
  ".R:active,.R.on{background:rgba(255,77,109,.13)}"
  ".T:active,.T.on{background:rgba(244,196,48,.11)}"
  ".S:active,.S.on{background:rgba(255,107,53,.13)}"
  ".sw{margin-top:22px;width:264px;text-align:center}"
  ".sw label{font-size:.7rem;letter-spacing:.12em;color:var(--mut);"
  "display:block;margin-bottom:8px}"
  "input[type=range]{-webkit-appearance:none;width:100%;height:5px;"
  "border-radius:3px;background:var(--brd);outline:none}"
  "input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;"
  "width:22px;height:22px;border-radius:50%;background:var(--acc);"
  "box-shadow:0 0 10px rgba(0,229,255,.5);cursor:pointer}"
  "#sv{margin-top:6px;font-size:.8rem;color:var(--acc);font-weight:600}"
  ".hint{margin-top:16px;font-size:.68rem;color:var(--mut);text-align:center}"
  ".k{display:inline-block;border:1px solid var(--brd);border-radius:4px;"
  "padding:1px 5px;font-family:monospace;color:var(--txt)}"
  "</style></head><body>";

const char HTML_B[] PROGMEM =
  "<h1>4WD Robot</h1>"
  "<p class='sub'>WiFi Controller &mdash; ESP8266</p>"
  "<div id='sbar'><div id='dot'></div>MODE:&nbsp;<span id='ml'>STOP</span></div>"
  "<svg width='110' height='140' viewBox='0 0 110 140' xmlns='http://www.w3.org/2000/svg'>"
  "<rect x='22' y='30' width='66' height='80' rx='10' fill='#161b26' stroke='#2a3040' stroke-width='1.5'/>"
  "<rect x='32' y='38' width='46' height='24' rx='5' fill='#0d0f14' stroke='#2a3040'/>"
  "<polygon id='ar' points='55,46 63,59 55,55 47,59' fill='#5a6478'/>"
  "<rect id='wFL' x='5'  y='30' width='17' height='30' rx='5' fill='#2a3040' stroke='#5a6478' stroke-width='1.5'/>"
  "<rect id='wFR' x='88' y='30' width='17' height='30' rx='5' fill='#2a3040' stroke='#5a6478' stroke-width='1.5'/>"
  "<rect id='wRL' x='5'  y='80' width='17' height='30' rx='5' fill='#2a3040' stroke='#5a6478' stroke-width='1.5'/>"
  "<rect id='wRR' x='88' y='80' width='17' height='30' rx='5' fill='#2a3040' stroke='#5a6478' stroke-width='1.5'/>"
  "</svg>"
  "<div class='dpad'>"
  "<div></div>"
  "<button class='btn F' id='bF'"
    " ontouchstart=\"g('forward')\" ontouchend=\"g('stop')\""
    " onmousedown=\"g('forward')\" onmouseup=\"g('stop')\""
    " onmouseleave=\"g('stop')\">&#9650;<span class='lbl'>FWD</span></button>"
  "<div></div>"
  "<button class='btn T' id='bL'"
    " ontouchstart=\"g('left')\" ontouchend=\"g('stop')\""
    " onmousedown=\"g('left')\" onmouseup=\"g('stop')\""
    " onmouseleave=\"g('stop')\">&#9664;<span class='lbl'>LEFT</span></button>"
  "<button class='btn S' id='bS' onclick=\"g('stop')\">&#9632;<span class='lbl'>STOP</span></button>"
  "<button class='btn T' id='bR'"
    " ontouchstart=\"g('right')\" ontouchend=\"g('stop')\""
    " onmousedown=\"g('right')\" onmouseup=\"g('stop')\""
    " onmouseleave=\"g('stop')\">&#9654;<span class='lbl'>RIGHT</span></button>"
  "<div></div>"
  "<button class='btn R' id='bV'"
    " ontouchstart=\"g('reverse')\" ontouchend=\"g('stop')\""
    " onmousedown=\"g('reverse')\" onmouseup=\"g('stop')\""
    " onmouseleave=\"g('stop')\">&#9660;<span class='lbl'>REV</span></button>"
  "<div></div>"
  "</div>"
  "<div class='sw'><label>SPEED</label>"
  "<input type='range' id='sp' min='20' max='100' value='85'"
    " oninput=\"document.getElementById('sv').textContent=this.value+'%'\">"
  "<div id='sv'>85%</div></div>"
  "<p class='hint'>"
  "<span class='k'>W</span><span class='k'>A</span>"
  "<span class='k'>S</span><span class='k'>D</span>"
  " &nbsp;or arrow keys &nbsp;|&nbsp; hold to move</p>";

const char HTML_C[] PROGMEM =
  "<script>"
  // Wheel colours
  "var CF='#00e5ff',CR='#ff4d6d',CY='#f4c430',CO='#2a3040';"
  // Current state
  "var cur='stop',tmr=null,last='';"
  // Set all 4 wheel colours + arrow colour
  "function wh(fl,fr,rl,rr,a){"
    "var ids=['wFL','wFR','wRL','wRR'];"
    "var cols=[fl,fr,rl,rr];"
    "for(var i=0;i<4;i++)document.getElementById(ids[i]).style.fill=cols[i];"
    "document.getElementById('ar').setAttribute('fill',a);"
  "}"
  // Update UI visuals for given action
  "function vis(a){"
    "var btns=['bF','bL','bR','bV','bS'];"
    "for(var i=0;i<btns.length;i++){"
      "var e=document.getElementById(btns[i]);"
      "if(e)e.classList.remove('on');"
    "}"
    "document.getElementById('ml').textContent=a.toUpperCase();"
    "if(a==='forward'){"
      "wh(CF,CF,CF,CF,CF);"    // all 4 cyan
      "document.getElementById('bF').classList.add('on');"
    "}else if(a==='reverse'){"
      "wh(CR,CR,CR,CR,CR);"    // all 4 red
      "document.getElementById('bV').classList.add('on');"
    "}else if(a==='left'){"
      "wh(CR,CF,CR,CF,CY);"    // left=red(back) right=cyan(fwd)
      "document.getElementById('bL').classList.add('on');"
    "}else if(a==='right'){"
      "wh(CF,CR,CF,CR,CY);"    // left=cyan(fwd) right=red(back)
      "document.getElementById('bR').classList.add('on');"
    "}else{"
      "wh(CO,CO,CO,CO,'#5a6478');" // all off
      "document.getElementById('bS').classList.add('on');"
    "}"
  "}"
  // Send command — throttled to 1 per 80ms
  "function g(a){"
    "if(a===cur)return;"
    "cur=a;vis(a);"
    "if(tmr)return;"
    "tmr=setTimeout(function(){"
      "tmr=null;"
      "var u='/'+cur+'?spd='+document.getElementById('sp').value;"
      "if(u!==last){fetch(u).catch(function(){});last=u;}"
    "},80);"
  "}"
  // Keyboard
  "var KM={"
    "'ArrowUp':'forward','w':'forward','W':'forward',"
    "'ArrowDown':'reverse','s':'reverse','S':'reverse',"
    "'ArrowLeft':'left','a':'left','A':'left',"
    "'ArrowRight':'right','d':'right','D':'right',"
    "' ':'stop'"
  "};"
  "var held={};"
  "document.addEventListener('keydown',function(e){"
    "if(held[e.key])return;held[e.key]=1;"
    "var a=KM[e.key];if(a){e.preventDefault();g(a);}"
  "});"
  "document.addEventListener('keyup',function(e){"
    "delete held[e.key];"
    "if(KM[e.key]&&KM[e.key]!=='stop')g('stop');"
  "});"
  "vis('stop');"
  "</script></body></html>";

void handleRoot() {
  String page = FPSTR(HTML_A);
  page += FPSTR(HTML_B);
  page += FPSTR(HTML_C);
  server.send(200, "text/html", page);
}

// ── SETUP ────────────────────────────────────────────────
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

  // IMPORTANT: sync PWM frequency and range for BOTH channels
  // so they respond identically (fixes lag between left & right)
  analogWriteFreq(1000);   // 1 kHz — stable on GPIO3
  analogWriteRange(1023);  // full 10-bit range

  // Start with standby HIGH (motors enabled)
  digitalWrite(STBY, HIGH);
  motorStop();

  // OLED
  Wire.begin(4, 5);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { while (1); }
  showOLED();

  // WiFi AP
  WiFi.softAP(SSID, PASS);
  showOLED();

  // Routes
  server.on("/", handleRoot);

  server.on("/forward", []() {
    motorForward(urlSpeed()); showOLED();
    server.send(200, "text/plain", "ok");
  });
  server.on("/reverse", []() {
    motorReverse(urlSpeed()); showOLED();
    server.send(200, "text/plain", "ok");
  });
  server.on("/left", []() {
    motorLeft(urlSpeed()); showOLED();
    server.send(200, "text/plain", "ok");
  });
  server.on("/right", []() {
    motorRight(urlSpeed()); showOLED();
    server.send(200, "text/plain", "ok");
  });
  server.on("/stop", []() {
    motorStop(); showOLED();
    server.send(200, "text/plain", "ok");
  });

  server.begin();
}

// ── LOOP ─────────────────────────────────────────────────
void loop() {
  server.handleClient();
}
