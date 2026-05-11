#include <Arduino.h>

#include "event_system.h"

namespace {
constexpr int APP_EVENT_QUEUE_SIZE = 16;
AppEvent eventQueue[APP_EVENT_QUEUE_SIZE];
volatile uint8_t eventHead = 0;
volatile uint8_t eventTail = 0;
}

void postAppEvent(AppEventType type, uint32_t value) {
    uint8_t nextHead = (uint8_t)((eventHead + 1) % APP_EVENT_QUEUE_SIZE);
    if (nextHead == eventTail) {
        eventTail = (uint8_t)((eventTail + 1) % APP_EVENT_QUEUE_SIZE);
    }

    eventQueue[eventHead] = {type, value, millis()};
    eventHead = nextHead;
}

bool pollAppEvent(AppEvent& event) {
    if (eventTail == eventHead) {
        return false;
    }

    event = eventQueue[eventTail];
    eventTail = (uint8_t)((eventTail + 1) % APP_EVENT_QUEUE_SIZE);
    return true;
}
