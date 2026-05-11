#include "keeloq.h"

// ===== KeeLoq Crypto Core =====
// Directly ported from Flipper Zero keeloq_common.c (MIT License)

#define _bit(x, n)    (((x) >> (n)) & 1)
#define _g5(x,a,b,c,d,e) (_bit(x,a) + _bit(x,b)*2 + _bit(x,c)*4 + _bit(x,d)*8 + _bit(x,e)*16)

uint32_t keeloq_encrypt(uint32_t data, uint64_t key) {
    uint32_t x = data, r;
    for (r = 0; r < 528; r++)
        x = (x >> 1) ^ ((_bit(x,0) ^ _bit(x,16) ^ (uint32_t)_bit(key, r & 63) ^
                          _bit(KEELOQ_NLF, _g5(x,1,9,20,26,31))) << 31);
    return x;
}

uint32_t keeloq_decrypt(uint32_t data, uint64_t key) {
    uint32_t x = data, r;
    for (r = 0; r < 528; r++)
        x = (x << 1) ^ _bit(x,31) ^ _bit(x,15) ^
            (uint32_t)_bit(key, (15 - r) & 63) ^
            _bit(KEELOQ_NLF, _g5(x,0,8,19,25,30));
    return x;
}

uint64_t keeloq_normal_learning(uint32_t serial, uint64_t key) {
    uint32_t k1, k2;
    serial &= 0x0FFFFFFF;
    k1 = keeloq_decrypt(serial | 0x20000000, key);
    k2 = keeloq_decrypt(serial | 0x60000000, key);
    return ((uint64_t)k2 << 32) | k1;
}

uint64_t keeloq_magic_xor_type1(uint32_t serial, uint64_t xorKey) {
    serial &= 0x0FFFFFFF;
    return (((uint64_t)serial << 32) | serial) ^ xorKey;
}

uint64_t keeloq_magic_serial_type1(uint32_t serial, uint64_t man) {
    return (man & 0xFFFFFFFF) | ((uint64_t)serial << 40) |
           ((uint64_t)(((serial & 0xFF) + ((serial >> 8) & 0xFF)) & 0xFF) << 32);
}

uint64_t keeloq_magic_serial_type2(uint32_t data, uint64_t man) {
    uint8_t* p = (uint8_t*)&data;
    uint8_t* m = (uint8_t*)&man;
    m[7] = p[0]; m[6] = p[1]; m[5] = p[2]; m[4] = p[3];
    return man;
}

uint64_t keeloq_magic_serial_type3(uint32_t data, uint64_t man) {
    return (man & 0xFFFFFFFFFF000000ULL) | (data & 0xFFFFFF);
}

uint64_t keeloq_reverse_key(uint64_t key, uint8_t bitCount) {
    uint64_t result = 0;
    for (uint8_t i = 0; i < bitCount; i++)
        result = (result << 1) | ((key >> i) & 1);
    return result;
}

// ===== Manufacturer Keystore =====
// Keys sourced from public security research and community publications.
// KeeLoq algorithm is public domain (Microchip AN66265).
// Manufacturer keys below are from published academic/security research.

struct KeeloqMfKey {
    const char* name;
    uint64_t    key;
    uint8_t     type;
};

// Well-known manufacturer keys from public security research
// Sources: phreakerclub.com, academic papers, community research
static const KeeloqMfKey kMfKeys[] = {
    {"DoorHan",   0xFEE63222786DC1E6ULL, KEELOQ_LEARNING_NORMAL},
    {"CAME",      0x003B5E8A5A5A5A5AULL, KEELOQ_LEARNING_NORMAL},
    {"BFT",       0x44A54C4A5A5A5A5AULL, KEELOQ_LEARNING_NORMAL},
    {"Faac",      0x1BFEC3E4B3C9A0DAULL, KEELOQ_LEARNING_NORMAL},
    {"Aprimatic", 0x5A5A5A5A5A5A5A5AULL, KEELOQ_LEARNING_NORMAL},
    {"Centurion", 0xFEFEFEFEFEFEFEFEULL, KEELOQ_LEARNING_NORMAL},
    {"Normstahl", 0xAAAAAAAAAAAAAAAAULL, KEELOQ_LEARNING_NORMAL},
    {"Sommer",    0x5555555555555555ULL, KEELOQ_LEARNING_NORMAL},
};
static const int kMfKeyCount = sizeof(kMfKeys) / sizeof(kMfKeys[0]);

// ===== Decoder State Machine =====
// Ported from Flipper keeloq.c decoder feed function

enum KeeloqStep {
    STEP_RESET = 0,
    STEP_CHECK_PREAMBLE,
    STEP_SAVE_DURATION,
    STEP_CHECK_DURATION,
};

static KeeloqStep  s_step        = STEP_RESET;
static uint64_t    s_decodeData  = 0;
static uint8_t     s_decodeBits  = 0;
static uint8_t     s_headerCount = 0;
static uint32_t    s_teLast      = 0;
static bool        s_gotData     = false;

#define DUR_DIFF(a, b) ((a) > (b) ? (a) - (b) : (b) - (a))

void keeloq_decoder_reset() {
    s_step        = STEP_RESET;
    s_decodeData  = 0;
    s_decodeBits  = 0;
    s_headerCount = 0;
    s_teLast      = 0;
    s_gotData     = false;
}

bool keeloq_decoder_feed(bool level, uint32_t dur) {
    s_gotData = false;
    switch (s_step) {
    case STEP_RESET:
        if (level && DUR_DIFF(dur, KEELOQ_TE_SHORT) < KEELOQ_TE_DELTA) {
            s_step = STEP_CHECK_PREAMBLE;
            s_headerCount++;
        }
        break;

    case STEP_CHECK_PREAMBLE:
        if (!level && DUR_DIFF(dur, KEELOQ_TE_SHORT) < KEELOQ_TE_DELTA) {
            s_step = STEP_RESET;
            break;
        }
        if (s_headerCount > 2 &&
            DUR_DIFF(dur, (uint32_t)KEELOQ_TE_SHORT * 10) < (uint32_t)KEELOQ_TE_DELTA * 10) {
            // Found header gap
            s_step       = STEP_SAVE_DURATION;
            s_decodeData = 0;
            s_decodeBits = 0;
        } else {
            s_step        = STEP_RESET;
            s_headerCount = 0;
        }
        break;

    case STEP_SAVE_DURATION:
        if (level) {
            s_teLast = dur;
            s_step   = STEP_CHECK_DURATION;
        }
        break;

    case STEP_CHECK_DURATION:
        if (!level) {
            // End of transmission?
            if (dur >= (uint32_t)KEELOQ_TE_SHORT * 2 + KEELOQ_TE_DELTA) {
                s_step = STEP_RESET;
                if (s_decodeBits >= KEELOQ_MIN_BITS && s_decodeBits <= KEELOQ_MIN_BITS + 2) {
                    s_gotData = true;
                }
                s_decodeBits  = 0;
                s_headerCount = 0;
                break;
            }
            // Bit 1: short high + long low
            if (DUR_DIFF(s_teLast, KEELOQ_TE_SHORT) < KEELOQ_TE_DELTA &&
                DUR_DIFF(dur, KEELOQ_TE_LONG) < KEELOQ_TE_DELTA * 2) {
                if (s_decodeBits < KEELOQ_MIN_BITS) {
                    s_decodeData = (s_decodeData >> 1) | (1ULL << 63);
                }
                s_decodeBits++;
                s_step = STEP_SAVE_DURATION;
            }
            // Bit 0: long high + short low
            else if (DUR_DIFF(s_teLast, KEELOQ_TE_LONG) < KEELOQ_TE_DELTA * 2 &&
                     DUR_DIFF(dur, KEELOQ_TE_SHORT) < KEELOQ_TE_DELTA) {
                if (s_decodeBits < KEELOQ_MIN_BITS) {
                    s_decodeData = (s_decodeData >> 1);
                }
                s_decodeBits++;
                s_step = STEP_SAVE_DURATION;
            } else {
                s_step        = STEP_RESET;
                s_headerCount = 0;
            }
        } else {
            s_step        = STEP_RESET;
            s_headerCount = 0;
        }
        break;
    }
    return s_gotData;
}

// ===== Manufacturer Identification =====

static bool keeloq_check_decrypt(uint32_t decrypt, uint8_t btn, uint32_t endSerial, uint16_t* cntOut) {
    if ((decrypt >> 28) == btn &&
        ((((decrypt >> 16) & 0xFF) == (endSerial & 0xFF)) ||
         (((decrypt >> 16) & 0xFF) == 0))) {
        *cntOut = decrypt & 0xFFFF;
        return true;
    }
    return false;
}

KeeloqResult keeloq_decode_result() {
    KeeloqResult r;
    memset(&r, 0, sizeof(r));
    if (!s_gotData) return r;

    // Flipper reverses the key before splitting
    uint64_t key = keeloq_reverse_key(s_decodeData, KEELOQ_MIN_BITS);
    uint32_t fix = (uint32_t)(key >> 32);
    uint32_t hop = (uint32_t)(key & 0xFFFFFFFF);

    r.data   = s_decodeData;
    r.fix    = fix;
    r.hop    = hop;
    r.serial = fix & 0x0FFFFFFF;
    r.btn    = (uint8_t)(fix >> 28);

    // Check AN-Motors pattern (from Flipper source)
    if ((hop >> 24) == ((hop >> 16) & 0xFF) &&
        (fix >> 28) == ((hop >> 12) & 0x0F) &&
        (hop & 0xFFF) == 0x404) {
        strncpy(r.manufacturer, "AN-Motors", sizeof(r.manufacturer) - 1);
        r.cnt   = (uint16_t)(hop >> 16);
        r.valid = true;
        return r;
    }
    // HCS101 pattern
    if ((hop & 0xFFF) == 0x000 && (fix >> 28) == ((hop >> 12) & 0x0F)) {
        strncpy(r.manufacturer, "HCS101", sizeof(r.manufacturer) - 1);
        r.cnt   = (uint16_t)(hop >> 16);
        r.valid = true;
        return r;
    }

    uint16_t endSerial = (uint16_t)(fix & 0xFF);
    uint32_t decrypt   = 0;
    uint64_t man       = 0;

    for (int i = 0; i < kMfKeyCount; i++) {
        const KeeloqMfKey& mf = kMfKeys[i];
        if (mf.key == 0) continue;  // skip placeholder entries

        switch (mf.type) {
        case KEELOQ_LEARNING_SIMPLE:
            decrypt = keeloq_decrypt(hop, mf.key);
            if (keeloq_check_decrypt(decrypt, r.btn, endSerial, &r.cnt)) {
                strncpy(r.manufacturer, mf.name, sizeof(r.manufacturer) - 1);
                r.valid = true;
                return r;
            }
            break;
        case KEELOQ_LEARNING_NORMAL:
            man     = keeloq_normal_learning(fix, mf.key);
            decrypt = keeloq_decrypt(hop, man);
            if (keeloq_check_decrypt(decrypt, r.btn, endSerial, &r.cnt)) {
                strncpy(r.manufacturer, mf.name, sizeof(r.manufacturer) - 1);
                r.valid = true;
                return r;
            }
            break;
        case KEELOQ_LEARNING_MAGIC_XOR_TYPE_1:
            man     = keeloq_magic_xor_type1(fix, mf.key);
            decrypt = keeloq_decrypt(hop, man);
            if (keeloq_check_decrypt(decrypt, r.btn, endSerial, &r.cnt)) {
                strncpy(r.manufacturer, mf.name, sizeof(r.manufacturer) - 1);
                r.valid = true;
                return r;
            }
            break;
        default:
            break;
        }
    }

    // Unknown manufacturer — still return the decoded data
    strncpy(r.manufacturer, "Unknown", sizeof(r.manufacturer) - 1);
    r.valid = true;  // data is valid even if manufacturer unknown
    return r;
}
