#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <IRremote.h>

// =====================
// PIN DEFINITIONS
// =====================
#define IR_RECEIVE_PIN   4
#define IR_TRANSMIT_PIN  23

#define OLED_SDA_PIN     21
#define OLED_SCL_PIN     22

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

void showText(const String &text) {
  display.clearBuffer();
  display.setFont(u8g2_font_7x14_tf);

  int textWidth = display.getUTF8Width(text.c_str());
  int x = (128 - textWidth) / 2;
  if (x < 0) x = 0;

  display.drawUTF8(x, 22, text.c_str());
  display.sendBuffer();
}

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  display.begin();
  display.enableUTF8Print();
  showText("No codes");

  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  IrSender.begin(IR_TRANSMIT_PIN);

  Serial.println("IR System Ready");
}

void loop() {
  if (IrReceiver.decode()) {
    uint32_t code = IrReceiver.decodedIRData.decodedRawData;

    Serial.print("IR Code: ");
    Serial.println(code, HEX);

    String text = "IR: " + String(code, HEX);
    showText(text);

    IrReceiver.resume();
  }
}