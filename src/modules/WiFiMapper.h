#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "../ui/Display.h"
#include "../utils/Buttons.h"
#include "config.h"

#define MAPPER_MAX_APS    16
#define MAPPER_HISTORY    20   // RSSI samples per AP

struct MapperAP {
    char    ssid[33];
    uint8_t bssid[6];
    uint8_t channel;
    int8_t  rssiHistory[MAPPER_HISTORY];
    uint8_t historyIdx;
    int8_t  rssiMin;
    int8_t  rssiMax;
    int8_t  rssiLast;
    bool    used;
};

// View modes — toggle with SELECT
enum class MapperView {
    AP_LIST,      // Scrollable list of APs with current RSSI
    CHANNEL_GRAPH // Bar graph of signal strength per channel 1-13
};

class WiFiMapper {
public:
    WiFiMapper(Display& display, Buttons& buttons);

    void enter();
    void tick();
    void stop();

    bool isDone() const { return _done; }

private:
    Display& _display;
    Buttons& _buttons;

    bool       _done        = false;
    bool       _needsRedraw = true;
    MapperView _view        = MapperView::AP_LIST;
    uint32_t   _lastScan    = 0;
    uint32_t   _scanInterval = 3000;   // Rescan every 3s
    int8_t     _cursor      = 0;
    int8_t     _scrollOffset = 0;
    uint8_t    _apCount     = 0;
    bool       _scanning    = false;

    MapperAP _aps[MAPPER_MAX_APS];

    // Channel RSSI — max signal seen per channel
    int8_t _channelRssi[14] = {0};   // index 1-13

    void _doScan();
    void _updateAP(const char* ssid, const uint8_t* bssid,
                   uint8_t channel, int8_t rssi);

    // ── Views ────────────────────────────────────
    void _drawAPList();
    void _drawChannelGraph();
    void _drawScanning();
    void _drawHintBar();

    // ── Helpers ──────────────────────────────────
    int8_t _avgRssi(const MapperAP& ap);
    uint16_t _rssiColor(int8_t rssi);
};
