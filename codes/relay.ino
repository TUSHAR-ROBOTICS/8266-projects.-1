#define RELAY_PIN 13   // Change if needed

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
}

void loop() {
  // Turn relay ON
  digitalWrite(RELAY_PIN, LOW);   // For active LOW relay
  delay(2000);                    // 2 seconds

  // Turn relay OFF
  digitalWrite(RELAY_PIN, HIGH);  // For active LOW relay
  delay(2000);                    // 2 seconds
}