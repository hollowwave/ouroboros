#pragma once
#include <Arduino.h>
#include "Display.h"
#include "../utils/Buttons.h"

enum class Screen {
    BOOT,
    MAIN_MENU,

    // WiFi
    WIFI_MENU,
    WIFI_SCAN,
    WIFI_DEAUTH,
    WIFI_DEAUTH_PICKER,
    WIFI_BEACON_SPAM,
    WIFI_PROBE_SNIFF,
    WIFI_MAPPER,

    // BLE
    BLE_MENU,
    BLE_SCAN,
    BLE_SPAM,

    // Sub-GHz
    SUBGHZ_MENU,
    SUBGHZ_SCAN,
    SUBGHZ_CAPTURE,
    SUBGHZ_REPLAY,
    SUBGHZ_ROLLING,
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
    Screen   _screen      = Screen::BOOT;
    Screen   _prevScreen  = Screen::BOOT;
    int16_t  _cursor      = 0;
    bool     _needsRedraw = true;

    void _navigate(Screen to);
    void _back();
    void _prev(int16_t count);
    void _next(int16_t count);
    void _select(int16_t count);

    void _redraw();

    // ── Horizontal selector renderer ─────────────
    // Shows: ◄  LABEL  ► with position dots below
    void _renderSelector(const MenuItem* items, uint8_t count,
                         const char* category);

    // ── Running screen chrome ────────────────────
    void _renderRunningScreen(const char* category,
                               const char* module,
                               const char* hint);

    void _renderBoot();
    void _renderMainMenu();
    void _renderWifiMenu();
    void _renderBleMenu();
    void _renderSubGhzMenu();
};
