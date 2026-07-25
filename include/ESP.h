#pragma once

// The Arduino-ESP32 core ships this header as Esp.h. Windows builds hide the
// case mismatch, while Linux/CI builds do not. Keep existing includes working
// on both filesystems without touching every module.
#include <Esp.h>
