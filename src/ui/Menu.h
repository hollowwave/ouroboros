#pragma once
#include <Arduino.h>
#include "Display.h"
#include "../utils/Buttons.h"

enum class Screen {
    SPLASH,
    MAIN_MENU,

    // WiFi
    WIFI_MENU,
    WIFI_SCAN,
    WIFI_DEAUTH,
    WIFI_DEAUTH_PICKER,   // NEW — target picker before deauth
    WIFI_BEACON_SPAM,
    WIFI_PROBE_SNIFF,
    WIFI_MAPPER,          // NEW

    // BLE
    BLE_MENU,
    BLE_SCAN,
    BLE_SPAM,

    // Sub-GHz
    SUBGHZ_MENU,
    SUBGHZ_SCAN,
    SUBGHZ_CAPTURE,
    SUBGHZ_REPLAY,
    SUBGHZ_ROLLING,       // NEW — rolling code detector
    SUBGHZ_CONFIG,
};

struct MenuItem {
    const char* label;
    Screen      target;
    uint16_t    labelColor;
};

class Menu {
public:
    Menu(Display& display, Buttons& buttons);

    void   begin();
    void   tick();
    Screen currentScreen() const { return _screen; }
    void   forceScreen(Screen s) { _navigate(s); }

private:
    Display& _display;
    Buttons& _buttons;
    Screen   _screen       = Screen::SPLASH;
    Screen   _prevScreen   = Screen::SPLASH;
    int16_t  _cursor       = 0;
    int16_t  _scrollOffset = 0;
    bool     _needsRedraw  = true;

    void _navigate(Screen to);
    void _back();
    void _cursorUp();
    void _cursorDown(int16_t count);
    void _select(int16_t count);
    void _redraw();
    void _renderMenu(const MenuItem* items, uint8_t count, const char* title);
    void _renderSplash();
    void _renderMainMenu();
    void _renderWifiMenu();
    void _renderBleMenu();
    void _renderSubGhzMenu();
    void _renderRunningScreen(const char* title, const char* hint);
};
