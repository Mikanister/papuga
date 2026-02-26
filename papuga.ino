#include <Arduino.h>
#include "src/board.h"
#include "src/app.h"

void setup() {
  boardInit();
  appInit();
}

void loop() {
  appTick(millis());
}