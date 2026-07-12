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
    BLE_SNIFFER_SCAN,      // NEW - BLE device discovery
    BLE_JAMMER_CONTROL,    // NEW - BLE jammer active
    BLE_STATS,             // NEW - Statistics & diagnostics

    // Sub-GHz
    SUBGHZ_MENU,
    SUBGHZ_SCAN,
    SUBGHZ_CAPTURE,
    SUBGHZ_REPLAY,
    SUBGHZ_ROLLING,
    SUBGHZ_CONFIG,

    // Frame Analysis
    FRAME_ANALYSIS,
    FRAME_ANALYSIS_BEACON,
    FRAME_ANALYSIS_AUTH,
    FRAME_ANALYSIS_DEAUTH,
    FRAME_ANALYSIS_PROBE,
    FRAME_ANALYSIS_GATT,

    // Evil Twin (NEW)
    EVIL_TWIN_MENU,
    EVIL_TWIN_SCAN,
    EVIL_TWIN_RUNNING,
    EVIL_TWIN_CREDENTIALS,
};

struct MenuItem {
    const char* label;
    Screen      target;
    uint16_t    labelColor;
    Glyph       icon = Glyph::NONE;
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
    // Shows: ◄  ICON  LABEL  ► with position dots below
    void _renderSelector(const MenuItem* items, uint8_t count,
                         const char* category, Glyph categoryIcon);

    // ── Running screen chrome ────────────────────
    void _renderRunningScreen(const char* category, Glyph categoryIcon,
                               const char* module,
                               Glyph leftHint, const char* leftLabel,
                               Glyph rightHint, const char* rightLabel);

    void _renderBoot();
    void _renderMainMenu();
    void _renderWifiMenu();
    void _renderBleMenu();
    void _renderSubGhzMenu();
    void _renderFrameAnalysisMenu();
    void _renderEvilTwinMenu();
};
