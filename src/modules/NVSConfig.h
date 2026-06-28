#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// ─────────────────────────────────────────
//  NVS Config — persists Sub-GHz settings
//  across reboots using ESP32 Preferences
// ─────────────────────────────────────────
struct SubGHzConfig {
    float   frequency  = 433.92f;
    uint8_t modulation = 2;       // ASK/OOK default
    uint8_t bandwidth  = 0;
    uint8_t txPower    = 4;       // 0 dBm default
    uint8_t pktLength  = 61;
};

class NVSConfig {
public:
    void begin();
    void save(const SubGHzConfig& cfg);
    SubGHzConfig load();
    void reset();   // Wipe to defaults

private:
    Preferences _prefs;
    static constexpr const char* NS = "subghz";   // NVS namespace
};
