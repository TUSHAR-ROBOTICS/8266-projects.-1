#include <Servo.h>

// Create servo objects
Servo servo1;
Servo servo2;
Servo servo3;

// Define pins
const int servo1Pin = 5; // D1 on ESP8266
const int servo2Pin = 4; // D2 on ESP8266
const int servo3Pin = 0; // D3 on ESP8266

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  delay(10);
  
  // Attach servos to pins
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);
  servo3.attach(servo3Pin);

  // Set initial safe positions (In sync at 90 degrees)
  servo1.write(90);
  servo2.write(90);
  servo3.write(180 - 90); // Matches Servo 2's physical 90

  Serial.println("--- ESP8266 3-Servo Control (In Sync) Ready ---");
  Serial.println("Enter commands like: s1 0, s2 90, s3 50");
}

void loop() {
  // Check if data is available in the Serial Buffer
  if (Serial.available() > 0) {
    // Read the incoming string until a newline character
    String command = Serial.readStringUntil('\n');
    command.trim(); // Remove any accidental spaces or hidden characters

    // Parse the command
    if (command.startsWith("s1 ")) {
      int angle = command.substring(3).toInt(); // Extract the number
      angle = constrain(angle, 0, 180);         // Keep within limits
      servo1.write(angle);
      Serial.print("Servo 1 moved to physical angle: ");
      Serial.println(angle);
    } 
    else if (command.startsWith("s2 ")) {
      int angle = command.substring(3).toInt();
      angle = constrain(angle, 0, 180);
      servo2.write(angle);
      Serial.print("Servo 2 moved to physical angle: ");
      Serial.println(angle);
    } 
    else if (command.startsWith("s3 ")) {
      int angle = command.substring(3).toInt();
      angle = constrain(angle, 0, 180);
      
      // Vise-versa fix: Invert the angle so s3 matches s2 physically
      int invertedAngle = 180 - angle; 
      servo3.write(invertedAngle);
      
      Serial.print("Servo 3 commanded to: ");
      Serial.print(angle);
      Serial.print(" (Sent ");
      Serial.print(invertedAngle);
      Serial.println(" to motor to keep in sync)");
    } 
    else {
      Serial.println("Invalid Command! Use: s1 [0-180], s2 [0-180], or s3 [0-180]");
    }
  }
}