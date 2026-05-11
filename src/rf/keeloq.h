#pragma once
#include <Arduino.h>

// ===== KeeLoq Protocol Decoder =====
// Ported from Flipper Zero open source firmware
// https://github.com/flipperdevices/flipperzero-firmware (MIT License)
//
// KeeLoq NLF constant
#define KEELOQ_NLF 0x3A5C742EUL

// Learning types
#define KEELOQ_LEARNING_SIMPLE              1
#define KEELOQ_LEARNING_NORMAL              2
#define KEELOQ_LEARNING_SECURE              3
#define KEELOQ_LEARNING_MAGIC_XOR_TYPE_1    4
#define KEELOQ_LEARNING_MAGIC_SERIAL_TYPE_1 5
#define KEELOQ_LEARNING_MAGIC_SERIAL_TYPE_2 6
#define KEELOQ_LEARNING_MAGIC_SERIAL_TYPE_3 7

// Pulse timing constants (microseconds)
#define KEELOQ_TE_SHORT   400
#define KEELOQ_TE_LONG    800
#define KEELOQ_TE_DELTA   140
#define KEELOQ_MIN_BITS   64

struct KeeloqResult {
    bool     valid;
    uint64_t data;          // raw 64-bit key
    uint32_t fix;           // upper 32 bits (serial + button)
    uint32_t hop;           // lower 32 bits (encrypted counter)
    uint32_t serial;        // 28-bit serial number
    uint8_t  btn;           // 4-bit button code
    uint16_t cnt;           // 16-bit counter (after decrypt)
    char     manufacturer[32];
};

// Core KeeLoq crypto (from Flipper keeloq_common.c)
uint32_t keeloq_encrypt(uint32_t data, uint64_t key);
uint32_t keeloq_decrypt(uint32_t data, uint64_t key);
uint64_t keeloq_normal_learning(uint32_t serial, uint64_t key);
uint64_t keeloq_magic_xor_type1(uint32_t serial, uint64_t xorKey);
uint64_t keeloq_magic_serial_type1(uint32_t serial, uint64_t man);
uint64_t keeloq_magic_serial_type2(uint32_t data, uint64_t man);
uint64_t keeloq_magic_serial_type3(uint32_t data, uint64_t man);

// Pulse decoder state machine
void keeloq_decoder_reset();
bool keeloq_decoder_feed(bool level, uint32_t durationUs);  // returns true when 64 bits decoded
KeeloqResult keeloq_decode_result();  // call after feed returns true

// Reverse bit order of a 64-bit key (Flipper uses this)
uint64_t keeloq_reverse_key(uint64_t key, uint8_t bitCount);
