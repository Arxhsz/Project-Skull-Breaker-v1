#include <Arduino.h>

#include "system.h"
#include "system_runtime.h"

void setup() {
    system_init();
    app_setup();
}

void loop() {
    app_loop();
}
