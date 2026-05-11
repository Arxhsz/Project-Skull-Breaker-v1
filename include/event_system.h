#pragma once

#include <Arduino.h>

enum AppEventType : uint8_t {
    APP_EVENT_NONE = 0,
    APP_EVENT_SCAN_COMPLETE,
    APP_EVENT_DEVICE_FOUND,
    APP_EVENT_MODE_CHANGED
};

struct AppEvent {
    AppEventType type;
    uint32_t value;
    unsigned long timestampMs;
};

void postAppEvent(AppEventType type, uint32_t value = 0);
bool pollAppEvent(AppEvent& event);
