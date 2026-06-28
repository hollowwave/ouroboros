#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "../ui/Display.h"
#include "config.h"

struct BLEDeviceInfo {
    char    name[32];
    char    address[18];
    int     rssi;
};

enum class BLESpamType {
    APPLE,
    SAMSUNG,
    WINDOWS
};

class BLEModule {
public:
    BLEModule(Display& display);

    void begin();

    // ── Scanner ──────────────────────────────────
    void startScan();
    void tickScan();
    void stopScan();

    // ── Spam ─────────────────────────────────────
    void startSpam(BLESpamType type = BLESpamType::APPLE);
    void tickSpam();
    void stopSpam();

    bool isRunning() const { return _running; }

private:
    Display&      _display;
    bool          _running    = false;
    BLEScan*      _scanner    = nullptr;
    BLESpamType   _spamType   = BLESpamType::APPLE;
    uint32_t      _lastTick   = 0;
    int           _spamCount  = 0;

    static const int MAX_DEVICES = 20;
    BLEDeviceInfo _devices[MAX_DEVICES];
    int           _deviceCount = 0;

    void _sendAppleAdv();
    void _sendSamsungAdv();
    void _sendWindowsAdv();

    void _drawScanResults();
    void _drawSpamStatus();

    class ScanCallback : public BLEAdvertisedDeviceCallbacks {
    public:
        BLEModule* parent;
        void onResult(BLEAdvertisedDevice dev) override;
    } _scanCb;
};
