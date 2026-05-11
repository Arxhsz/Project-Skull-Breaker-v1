#pragma once

#include <Arduino.h>

#include "system_types.h"

struct ToolDescriptor {
    const char* name;
    RadioMode mode;
    void (*run)();
    void (*handleInput)();
};

const ToolDescriptor* findToolByMode(RadioMode mode);
