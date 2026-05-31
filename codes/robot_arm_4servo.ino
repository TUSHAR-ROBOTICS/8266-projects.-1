/*
 * ============================================================
 *  ESP8266 - 4 Servo Robot Arm Controller
 *  Control via Serial Monitor (9600 baud)
 * ============================================================
 *  WIRING:
 *    Servo 1 (Base)     → D1 (GPIO5)
 *    Servo 2 (Shoulder) → D2 (GPIO4)
 *    Servo 3 (Elbow)    → D3 (GPIO0)
 *    Servo 4 (Gripper)  → D4 (GPIO2)
 *
 *    ⚠️  POWER:
 *    - Servo VCC (Red)   → External 5V supply (min 2A)
 *    - Servo GND (Black) → External GND  ──┐
 *    - ESP8266 GND       → External GND  ──┘ (MUST be joined!)
 *    - DO NOT power servos from ESP8266 3.3V pin
 *
 * ============================================================
 *  SERIAL COMMANDS (open Serial Monitor at 9600 baud):
 *
 *    Single servo:
 *      1:90       → Move Servo 1 (Base) to 90°
 *      2:45       → Move Servo 2 (Shoulder) to 45°
 *      3:120      → Move Servo 3 (Elbow) to 120°
 *      4:0        → Move Servo 4 (Gripper) to 0° (open)
 *
 *    Preset positions:
 *      HOME       → All servos to home position (90°)
 *      GRAB       → Preset grab sequence
 *      RELEASE    → Preset release sequence
 *      SWEEP      → Test sweep all servos 0→180→0
 *      STATUS     → Print current angles of all servos
 *
 * ============================================================
 *  LIBRARY REQUIRED:
 *    Install "ESP8266Servo" from Arduino Library Manager
 * ============================================================
 */

#include <ESP8266Servo.h>

// ----- Pin Definitions -----
#define SERVO1_PIN  D1   // Base
#define SERVO2_PIN  D2   // Shoulder
#define SERVO3_PIN  D3   // Elbow
#define SERVO4_PIN  D4   // Gripper

// ----- Servo Objects -----
ESP8266Servo servo1;  // Base
ESP8266Servo servo2;  // Shoulder
ESP8266Servo servo3;  // Elbow
ESP8266Servo servo4;  // Gripper

// ----- Current angle tracking -----
int angle1 = 90;
int angle2 = 90;
int angle3 = 90;
int angle4 = 90;

// ----- Smooth move delay (ms between each degree step) -----
#define MOVE_DELAY  15

// ============================================================
//  SMOOTH MOVE FUNCTION
//  Moves a servo gradually from current angle to target angle
//  This prevents jerking and reduces current spikes
// ============================================================
void smoothMove(ESP8266Servo &servo, int &currentAngle, int targetAngle) {
  targetAngle = constrain(targetAngle, 0, 180);  // Safety clamp

  if (currentAngle < targetAngle) {
    for (int pos = currentAngle; pos <= targetAngle; pos++) {
      servo.write(pos);
      delay(MOVE_DELAY);
    }
  } else {
    for (int pos = currentAngle; pos >= targetAngle; pos--) {
      servo.write(pos);
      delay(MOVE_DELAY);
    }
  }
  currentAngle = targetAngle;
}

// ============================================================
//  PRESET: HOME  — All servos to 90°
// ============================================================
void gotoHome() {
  Serial.println(">> Moving to HOME position...");
  smoothMove(servo1, angle1, 90);
  smoothMove(servo2, angle2, 90);
  smoothMove(servo3, angle3, 90);
  smoothMove(servo4, angle4, 90);
  Serial.println(">> HOME done.");
}

// ============================================================
//  PRESET: GRAB  — Example grab sequence
// ============================================================
void doGrab() {
  Serial.println(">> GRAB sequence starting...");
  smoothMove(servo2, angle2, 60);   // Shoulder down
  smoothMove(servo3, angle3, 120);  // Elbow extend
  smoothMove(servo4, angle4, 160);  // Gripper close
  smoothMove(servo2, angle2, 90);   // Shoulder up
  Serial.println(">> GRAB done.");
}

// ============================================================
//  PRESET: RELEASE  — Example release sequence
// ============================================================
void doRelease() {
  Serial.println(">> RELEASE sequence starting...");
  smoothMove(servo2, angle2, 60);   // Shoulder down
  smoothMove(servo3, angle3, 120);  // Elbow extend
  smoothMove(servo4, angle4, 10);   // Gripper open
  smoothMove(servo2, angle2, 90);   // Shoulder up
  Serial.println(">> RELEASE done.");
}

// ============================================================
//  PRESET: SWEEP  — Test all servos 0→180→90
// ============================================================
void doSweep() {
  Serial.println(">> SWEEP test starting...");
  Serial.println("   Sweeping Servo 1 (Base)...");
  smoothMove(servo1, angle1, 0);
  smoothMove(servo1, angle1, 180);
  smoothMove(servo1, angle1, 90);

  Serial.println("   Sweeping Servo 2 (Shoulder)...");
  smoothMove(servo2, angle2, 0);
  smoothMove(servo2, angle2, 180);
  smoothMove(servo2, angle2, 90);

  Serial.println("   Sweeping Servo 3 (Elbow)...");
  smoothMove(servo3, angle3, 0);
  smoothMove(servo3, angle3, 180);
  smoothMove(servo3, angle3, 90);

  Serial.println("   Sweeping Servo 4 (Gripper)...");
  smoothMove(servo4, angle4, 0);
  smoothMove(servo4, angle4, 180);
  smoothMove(servo4, angle4, 90);

  Serial.println(">> SWEEP done.");
}

// ============================================================
//  PRINT STATUS
// ============================================================
void printStatus() {
  Serial.println("-----------------------------");
  Serial.print  ("  Servo 1 (Base)     : "); Serial.print(angle1); Serial.println("°");
  Serial.print  ("  Servo 2 (Shoulder) : "); Serial.print(angle2); Serial.println("°");
  Serial.print  ("  Servo 3 (Elbow)    : "); Serial.print(angle3); Serial.println("°");
  Serial.print  ("  Servo 4 (Gripper)  : "); Serial.print(angle4); Serial.println("°");
  Serial.println("-----------------------------");
}

// ============================================================
//  PARSE SERIAL COMMAND
// ============================================================
void parseCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "HOME") {
    gotoHome();
  } else if (cmd == "GRAB") {
    doGrab();
  } else if (cmd == "RELEASE") {
    doRelease();
  } else if (cmd == "SWEEP") {
    doSweep();
  } else if (cmd == "STATUS") {
    printStatus();
  }
  // Single servo command: format  "1:90"
  else if (cmd.length() >= 3 && cmd.charAt(1) == ':') {
    int servoNum = cmd.charAt(0) - '0';       // Get servo number (1–4)
    int angle    = cmd.substring(2).toInt();  // Get angle after ':'

    if (servoNum < 1 || servoNum > 4) {
      Serial.println("!! Invalid servo number. Use 1, 2, 3 or 4.");
      return;
    }
    if (angle < 0 || angle > 180) {
      Serial.println("!! Angle must be 0–180.");
      return;
    }

    Serial.print(">> Moving Servo ");
    Serial.print(servoNum);
    Serial.print(" to ");
    Serial.print(angle);
    Serial.println("°...");

    switch (servoNum) {
      case 1: smoothMove(servo1, angle1, angle); break;
      case 2: smoothMove(servo2, angle2, angle); break;
      case 3: smoothMove(servo3, angle3, angle); break;
      case 4: smoothMove(servo4, angle4, angle); break;
    }
    Serial.println(">> Done.");
  }
  else {
    Serial.println("!! Unknown command. Try: 1:90  HOME  GRAB  RELEASE  SWEEP  STATUS");
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(9600);
  delay(500);

  // Attach servos to pins
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo3.attach(SERVO3_PIN);
  servo4.attach(SERVO4_PIN);

  // Move all to home on startup (one at a time to avoid current spike)
  Serial.println("==============================");
  Serial.println(" ESP8266 Robot Arm Controller");
  Serial.println("==============================");
  Serial.println("Initialising servos...");

  servo1.write(90); delay(300);
  servo2.write(90); delay(300);
  servo3.write(90); delay(300);
  servo4.write(90); delay(300);

  angle1 = angle2 = angle3 = angle4 = 90;

  Serial.println("Ready! Commands:");
  Serial.println("  1:90  2:45  3:120  4:10");
  Serial.println("  HOME  GRAB  RELEASE  SWEEP  STATUS");
  Serial.println("==============================");
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    parseCommand(input);
  }
}
