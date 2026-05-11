#pragma once

#include <Arduino.h>
#include <stdarg.h>

#ifndef DEBUG
#define DEBUG 1
#endif

#ifndef DEBUG_VERBOSE_SCANS
#define DEBUG_VERBOSE_SCANS 0
#endif

#ifndef DEBUG_VERBOSE_UI
#define DEBUG_VERBOSE_UI 0
#endif

#ifndef DEBUG_VERBOSE_ICONS
#define DEBUG_VERBOSE_ICONS 0
#endif

#ifndef DEBUG_VERBOSE_RF
#define DEBUG_VERBOSE_RF 1
#endif

#if DEBUG
inline String orionNormalizeLogMessage(String message) {
    message.replace("✓", "OK ");
    message.replace("✗", "FAIL ");
    message.replace("âœ“", "OK ");
    message.replace("âœ—", "FAIL ");
    message.replace("Âµ", "u");
    message.replace("ðŸ‘ˆ", "");
    message.replace("ðŸ”¥", "");
    return message;
}

inline void orionLogPrefix(const char* scope) {
    Serial.print('[');
    Serial.print((scope != nullptr && scope[0] != '\0') ? scope : "SYS");
    Serial.print("] ");
}

inline void orionLogLine(const char* scope, const char* message) {
    String normalized = orionNormalizeLogMessage(String(message != nullptr ? message : ""));
    orionLogPrefix(scope);
    Serial.println(normalized);
}

inline void orionLogLine(const char* scope, const String& message) {
    String normalized = orionNormalizeLogMessage(message);
    orionLogPrefix(scope);
    Serial.println(normalized);
}

template <typename T>
inline void orionLogKV(const char* scope, const char* key, const T& value) {
    orionLogPrefix(scope);
    Serial.print(key);
    Serial.print(": ");
    Serial.println(value);
}

inline void orionLogSection(const char* scope, const char* title) {
    orionLogPrefix(scope);
    Serial.print("==== ");
    Serial.print(title != nullptr ? title : "");
    Serial.println(" ====");
}

inline void orionLogBlank() {
    Serial.println();
}

inline void orionLogPrintf(const char* scope, const char* format, ...) {
    char buffer[192];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    orionLogLine(scope, buffer);
}

inline void orionLogAsciiLogo(const char* projectName, const char* projectVersion) {
    Serial.println();
    Serial.println(F("  ____             _           _      ___       _"));
    Serial.println(F(" |  _ \\ _ __ ___  (_) ___  ___| |_   / _ \\ _ __(_) ___  _ __"));
    Serial.println(F(" | |_) | '__/ _ \\ | |/ _ \\/ __| __| | | | | '__| |/ _ \\| '_ \\"));
    Serial.println(F(" |  __/| | | (_) || |  __/ (__| |_  | |_| | |  | | (_) | | | |"));
    Serial.println(F(" |_|   |_|  \\___// |\\___|\\___|\\__|  \\___/|_|  |_|\\___/|_| |_|"));
    Serial.println(F("                 |__/"));
    orionLogLine("BOOT", String(projectName) + " " + String(projectVersion));
    Serial.println();
}
#else
template <typename... Args>
inline void orionLogLine(Args...) {}

template <typename... Args>
inline void orionLogKV(Args...) {}

template <typename... Args>
inline void orionLogSection(Args...) {}

template <typename... Args>
inline void orionLogPrintf(Args...) {}

inline void orionLogBlank() {}
inline void orionLogAsciiLogo(const char*, const char*) {}
#endif

#ifndef LOG_SCOPE
#define LOG_SCOPE "SYS"
#endif

#if DEBUG
#define LOG(x) orionLogLine(LOG_SCOPE, String(x))
#define LOG_KV(key, value) orionLogKV(LOG_SCOPE, key, value)
#define LOG_SECTION(title) orionLogSection(LOG_SCOPE, title)
#define LOGF(...) orionLogPrintf(LOG_SCOPE, __VA_ARGS__)
#define LOG_IF(flag, x) do { if (flag) orionLogLine(LOG_SCOPE, String(x)); } while (0)
#else
#define LOG(x) do {} while (0)
#define LOG_KV(key, value) do {} while (0)
#define LOG_SECTION(title) do {} while (0)
#define LOGF(...) do {} while (0)
#define LOG_IF(flag, x) do {} while (0)
#endif
