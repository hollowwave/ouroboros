#include "Menu.h"

static const MenuItem MAIN_ITEMS[] = {
    { "WiFi",    Screen::WIFI_MENU,   CLR_TEXT },
    { "BLE",     Screen::BLE_MENU,    CLR_TEXT },
    { "Sub-GHz", Screen::SUBGHZ_MENU, CLR_TEXT },
};
static const MenuItem WIFI_ITEMS[] = {
    { "Scan",        Screen::WIFI_SCAN,          CLR_TEXT   },
    { "Deauth",      Screen::WIFI_DEAUTH_PICKER, CLR_ACCENT },
    { "Beacon Spam", Screen::WIFI_BEACON_SPAM,   CLR_ACCENT },
    { "Probe Sniff", Screen::WIFI_PROBE_SNIFF,   CLR_TEXT   },
    { "RSSI Mapper", Screen::WIFI_MAPPER,         CLR_TEXT   },
};
static const MenuItem BLE_ITEMS[] = {
    { "Scan",  Screen::BLE_SCAN,  CLR_TEXT   },
    { "Spam",  Screen::BLE_SPAM,  CLR_ACCENT },
};
static const MenuItem SUBGHZ_ITEMS[] = {
    { "Scan",        Screen::SUBGHZ_SCAN,    CLR_TEXT },
    { "Capture",     Screen::SUBGHZ_CAPTURE, CLR_TEXT },
    { "Replay",      Screen::SUBGHZ_REPLAY,  CLR_TEXT },
    { "Code Detect", Screen::SUBGHZ_ROLLING, CLR_TEXT },
    { "Config",      Screen::SUBGHZ_CONFIG,  CLR_DIM  },
};

Menu::Menu(Display& display, Buttons& buttons)
    : _display(display), _buttons(buttons) {}

void Menu::begin() { _navigate(Screen::BOOT); }

void Menu::tick() {
    if (_screen == Screen::SUBGHZ_CONFIG      ||
        _screen == Screen::SUBGHZ_ROLLING     ||
        _screen == Screen::WIFI_DEAUTH_PICKER ||
        _screen == Screen::WIFI_MAPPER) return;

    BtnEvent e = _buttons.consume();

    if (_screen == Screen::BOOT) {
        if (e != BtnEvent::NONE) _navigate(Screen::MAIN_MENU);
        return;
    }

    int16_t count = 0;
    switch (_screen) {
        case Screen::MAIN_MENU:   count = 3; break;
        case Screen::WIFI_MENU:   count = 5; break;
        case Screen::BLE_MENU:    count = 2; break;
        case Screen::SUBGHZ_MENU: count = 5; break;
        default: count = 0; break;
    }

    switch (e) {
        case BtnEvent::UP:        _prev(count);   break;
        case BtnEvent::DOWN:      _next(count);   break;
        case BtnEvent::SELECT:    _select(count); break;
        case BtnEvent::BACK:      _back();         break;
        case BtnEvent::BACK_LONG: _navigate(Screen::MAIN_MENU); break;
        default: break;
    }

    if (_needsRedraw) { _redraw(); _needsRedraw = false; }
}

// ─────────────────────────────────────────
//  Navigation
// ─────────────────────────────────────────
void Menu::_navigate(Screen to) {
    _prevScreen  = _screen;
    _screen      = to;
    _cursor      = 0;
    _needsRedraw = true;
}

void Menu::_back() {
    switch (_screen) {
        case Screen::WIFI_MENU:
        case Screen::BLE_MENU:
        case Screen::SUBGHZ_MENU:
            _navigate(Screen::MAIN_MENU); break;
        case Screen::WIFI_SCAN:
        case Screen::WIFI_DEAUTH:
        case Screen::WIFI_DEAUTH_PICKER:
        case Screen::WIFI_BEACON_SPAM:
        case Screen::WIFI_PROBE_SNIFF:
        case Screen::WIFI_MAPPER:
            _navigate(Screen::WIFI_MENU); break;
        case Screen::BLE_SCAN:
        case Screen::BLE_SPAM:
            _navigate(Screen::BLE_MENU); break;
        case Screen::SUBGHZ_SCAN:
        case Screen::SUBGHZ_CAPTURE:
        case Screen::SUBGHZ_REPLAY:
        case Screen::SUBGHZ_ROLLING:
        case Screen::SUBGHZ_CONFIG:
            _navigate(Screen::SUBGHZ_MENU); break;
        default:
            _navigate(Screen::MAIN_MENU); break;
    }
}

void Menu::_prev(int16_t count) {
    _cursor = (_cursor - 1 + count) % count;
    _needsRedraw = true;
}

void Menu::_next(int16_t count) {
    _cursor = (_cursor + 1) % count;
    _needsRedraw = true;
}

void Menu::_select(int16_t count) {
    if (_cursor >= count) return;
    switch (_screen) {
        case Screen::MAIN_MENU:   _navigate(MAIN_ITEMS[_cursor].target);   break;
        case Screen::WIFI_MENU:   _navigate(WIFI_ITEMS[_cursor].target);   break;
        case Screen::BLE_MENU:    _navigate(BLE_ITEMS[_cursor].target);    break;
        case Screen::SUBGHZ_MENU: _navigate(SUBGHZ_ITEMS[_cursor].target); break;
        default: break;
    }
}

// ─────────────────────────────────────────
//  Rendering
// ─────────────────────────────────────────
void Menu::_redraw() {
    switch (_screen) {
        case Screen::BOOT:        _renderBoot();       break;
        case Screen::MAIN_MENU:   _renderMainMenu();   break;
        case Screen::WIFI_MENU:   _renderWifiMenu();   break;
        case Screen::BLE_MENU:    _renderBleMenu();    break;
        case Screen::SUBGHZ_MENU: _renderSubGhzMenu(); break;

        case Screen::WIFI_SCAN:
            _renderRunningScreen("WIFI", "Scan",        "SEL:rescan  BCK:back");       break;
        case Screen::WIFI_DEAUTH:
            _renderRunningScreen("WIFI", "Deauth",      "L-SEL:toggle  DBL:retarget"); break;
        case Screen::WIFI_BEACON_SPAM:
            _renderRunningScreen("WIFI", "Beacon Spam", "L-SEL:toggle  BCK:stop");     break;
        case Screen::WIFI_PROBE_SNIFF:
            _renderRunningScreen("WIFI", "Probe Sniff", "BCK:stop");                   break;
        case Screen::BLE_SCAN:
            _renderRunningScreen("BLE",  "Scan",        "SEL:rescan  BCK:back");       break;
        case Screen::BLE_SPAM:
            _renderRunningScreen("BLE",  "Spam",        "L-SEL:toggle  BCK:stop");     break;
        case Screen::SUBGHZ_SCAN:
            _renderRunningScreen("SUB-GHZ", "Scan",     "BCK:stop");                   break;
        case Screen::SUBGHZ_CAPTURE:
            _renderRunningScreen("SUB-GHZ", "Capture",  "BCK:cancel");                 break;
        case Screen::SUBGHZ_REPLAY:
            _renderRunningScreen("SUB-GHZ", "Replay",   "SEL:again  BCK:back");        break;
        default: break;
    }
}

// ─────────────────────────────────────────
//  Horizontal selector
//
//  ┌──────────────────────────────────┐
//  │  CATEGORY              [dot]     │  13px status bar
//  ├──────────────────────────────────┤
//  │                                  │
//  │                                  │
//  │  <         LABEL         >       │  vertically centered
//  │                                  │
//  │                                  │
//  ├──────────────────────────────────┤
//  │           ○ ○ ● ○ ○              │  10px position dots
//  ├──────────────────────────────────┤
//  │  SEL:enter  BCK:back             │  11px hint bar
//  └──────────────────────────────────┘
// ─────────────────────────────────────────
void Menu::_renderSelector(const MenuItem* items, uint8_t count,
                            const char* category) {
    _display.clear();
    _display.drawStatusBar(category);

    const MenuItem& item = items[_cursor];

    // Content area height — between status bar and dots row
    int16_t contentTop = STATUSBAR_H + 1;
    int16_t contentBot = SCREEN_H - 22;   // 11px hint + 11px dots
    int16_t labelY     = contentTop + (contentBot - contentTop) / 2 - 4;

    // Label — centered, colored
    int16_t labelW = strlen(item.label) * 6;
    int16_t labelX = (SCREEN_W - labelW) / 2;
    _display.drawText(item.label, labelX, labelY, 1, item.labelColor);

    // Arrows
    _display.drawText("<", 4,             labelY, 1, CLR_ACCENT);
    _display.drawText(">", SCREEN_W - 10, labelY, 1, CLR_ACCENT);

    // Position dots — centered row
    int16_t dotY      = SCREEN_H - 20;
    int16_t dotStep   = 10;
    int16_t dotsW     = (count - 1) * dotStep;
    int16_t dotStartX = (SCREEN_W - dotsW) / 2;

    for (uint8_t i = 0; i < count; i++) {
        int16_t dx = dotStartX + i * dotStep;
        if (i == (uint8_t)_cursor)
            _display.raw().fillCircle(dx, dotY, 3, CLR_ACCENT);
        else
            _display.raw().drawCircle(dx, dotY, 2, CLR_SUBTLE);
    }

    // Hint bar
    _display.drawHintBar("SEL:enter  BCK:back");
}

// ─────────────────────────────────────────
//  Running screen chrome
//
//  Status bar shows breadcrumb: CATEGORY > MODULE
//  Active dot shows module is running
//  Content area owned by the module itself
//  Hint bar at bottom
// ─────────────────────────────────────────
void Menu::_renderRunningScreen(const char* category,
                                 const char* module,
                                 const char* hint) {
    char breadcrumb[32];
    snprintf(breadcrumb, sizeof(breadcrumb), "%s > %s", category, module);
    _display.drawStatusBar(breadcrumb, true);
    _display.drawHintBar(hint);
}

// ─────────────────────────────────────────
//  Screens
// ─────────────────────────────────────────
void Menu::_renderBoot() {
    _display.clear();

    _display.raw().setTextColor(CLR_ACCENT, CLR_BG);
    _display.raw().setTextSize(2);
    _display.raw().setCursor((SCREEN_W - 72) / 2, 34);
    _display.raw().print("OURO");

    _display.raw().setTextColor(CLR_ACCENT_DIM, CLR_BG);
    _display.raw().setCursor((SCREEN_W - 72) / 2, 52);
    _display.raw().print("BOROS");

    _display.raw().drawFastHLine(20, 72, SCREEN_W - 40, CLR_ACCENT_DIM);

    _display.drawCenteredText("v1.0-beta",     80, CLR_DIM);
    _display.drawCenteredText("by hollowwave",  92, CLR_DIM);
    _display.drawCenteredText("press any key", 110, CLR_SUBTLE);
}

void Menu::_renderMainMenu()   { _renderSelector(MAIN_ITEMS,   3, "OUROBOROS"); }
void Menu::_renderWifiMenu()   { _renderSelector(WIFI_ITEMS,   5, "WIFI"); }
void Menu::_renderBleMenu()    { _renderSelector(BLE_ITEMS,    2, "BLUETOOTH"); }
void Menu::_renderSubGhzMenu() { _renderSelector(SUBGHZ_ITEMS, 5, "SUB-GHZ"); }
