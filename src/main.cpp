#include <Arduino.h>

#include "gm67.hpp"

UART Serial2(8, 9, 0, 0);

GM67 gm67(Serial2);

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200);

  gm67.wake();
  gm67.set_trigger_mode(GM67TriggerMode::HOST);
  gm67.set_packetize_data(true);
  gm67.set_data_format(GM67DataFormat::CODE);
  gm67.set_scanner_enabled(true);
}

void loop() {
  GM67Barcode* resp = gm67.scan();
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
