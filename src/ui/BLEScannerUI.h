#pragma once

#include <Arduino.h>
#include "../ui/Display.h"
#include "../utils/Buttons.h"
#include "BLESnifferJammer.h"

// ─────────────────────────────────────────
//  BLE Sniffer Screen - Device Discovery
// ─────────────────────────────────────────
class BLEScanScreen {
public:
    BLEScanScreen(Display& display, Buttons& buttons, BLESnifferJammer& jammer);

    void enter();           // Call when navigating to this screen
    void tick();            // Call every loop()
    bool isDone() const { return _done; }

    int8_t getSelectedDevice() const { return _selected_idx; }

private:
    Display&            _display;
    Buttons&            _buttons;
    BLESnifferJammer&   _jammer;

    bool    _done = false;
    int8_t  _selected_idx = 0;
    bool    _needsRedraw = true;
    uint32_t _last_update_ms = 0;

    void _redraw();
    void _drawDeviceList();
    void _drawDeviceDetails(int8_t idx);
};

// ─────────────────────────────────────────
//  BLE Jammer Control Screen
// ─────────────────────────────────────────
class BLEJammerScreen {
public:
    BLEJammerScreen(Display& display, Buttons& buttons, BLESnifferJammer& jammer);

    void enter();
    void tick();
    bool isDone() const { return _done; }

    void setTargetMAC(const uint8_t* mac);  // Set selective jam target
    void setMode(BLEJammerMode mode) { _jam_mode = mode; }

private:
    Display&            _display;
    Buttons&            _buttons;
    BLESnifferJammer&   _jammer;

    bool            _done = false;
    uint8_t         _target_mac[6] = {0};
    BLEJammerMode   _jam_mode = BLEJammerMode::JAM_BROADCAST;
    uint32_t        _start_time_ms = 0;
    bool            _jammer_active = false;

    void _redraw();
    void _drawJammerStats();
    void _updateStats();
};

// ─────────────────────────────────────────
//  BLE Statistics & Diagnostics Screen
// ─────────────────────────────────────────
class BLEStatsScreen {
public:
    BLEStatsScreen(Display& display, Buttons& buttons, BLESnifferJammer& jammer);

    void enter();
    void tick();
    bool isDone() const { return _done; }

private:
    Display&            _display;
    Buttons&            _buttons;
    BLESnifferJammer&   _jammer;

    bool        _done = false;
    uint8_t     _stat_page = 0;  // 0 = summary, 1 = jam details, 2 = device list

    void _redraw();
    void _drawSummary();
    void _drawJamDetails();
    void _drawDeviceList();
};
