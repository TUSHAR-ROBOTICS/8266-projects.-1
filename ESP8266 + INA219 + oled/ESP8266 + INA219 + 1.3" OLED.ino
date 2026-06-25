#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_INA219.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

Adafruit_INA219 ina219;

void setup() {
  Serial.begin(115200);

  Wire.begin(4, 5);

  u8g2.begin();

  if (!ina219.begin()) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(10, 30, "INA219 NOT FOUND");
    u8g2.sendBuffer();

    while (1);
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(35, 30, "READY");
  u8g2.sendBuffer();

  delay(2000);
}

void loop() {
  float voltage = ina219.getBusVoltage_V();
  float current = ina219.getCurrent_mA();
  float power = ina219.getPower_mW();

  Serial.print("Voltage: ");
  Serial.print(voltage);
  Serial.println(" V");

  Serial.print("Current: ");
  Serial.print(current);
  Serial.println(" mA");

  Serial.print("Power: ");
  Serial.print(power);
  Serial.println(" mW");

  Serial.println("----------------");

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tf);

  u8g2.drawStr(0, 12, "INA219 Monitor");

  char buf[20];

  sprintf(buf, "V: %.2f V", voltage);
  u8g2.drawStr(0, 28, buf);

  sprintf(buf, "I: %.1f mA", current);
  u8g2.drawStr(0, 42, buf);

  sprintf(buf, "P: %.1f mW", power);
  u8g2.drawStr(0, 56, buf);

  u8g2.sendBuffer();

  delay(1000);
}
