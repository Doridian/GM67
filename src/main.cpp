#include <Arduino.h>

#include "gm67.hpp"

UART Serial2(8, 9, 0, 0);

GM67 gm67(Serial2);

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200);
}

void loop() {
  if (Serial.available() && Serial.read() == 'w') {
    gm67.wake();
    gm67.set_trigger_mode(GM67TriggerMode::HOST);
    Serial.println("Trigger mode set to HOST");
  }

  GM67Barcode* resp = gm67.scan(1000);
  if (resp != nullptr) {
    Serial.print("Read: Type: ");
    Serial.print(resp->barcode_type, HEX);
    Serial.print(" Data: ");
    for (int i = 0; i < resp->length; i++) {
      Serial.print(resp->data[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    free(resp);
  }
}
