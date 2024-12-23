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

  const GM67Response* buf = gm67.poll();
  if (buf != nullptr) {
    Serial.print("Read: Op: ");
    Serial.print(buf->opcode, HEX);
    Serial.print(" Data: ");
    for (int i = 0; i < buf->length; i++) {
      Serial.print(buf->data[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
}
