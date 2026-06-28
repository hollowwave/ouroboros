#include "Menu.h"

// ─────────────────────────────────────────
//  Menu definitions
// ─────────────────────────────────────────
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

void Menu::begin() { _navigate(Screen::SPLASH); }

void Menu::tick() {
    if (_screen == Screen::SUBGHZ_CONFIG      ||
        _screen == Screen::SUBGHZ_ROLLING     ||
        _screen == Screen::WIFI_DEAUTH_PICKER ||
        _screen == Screen::WIFI_MAPPER) return;

    BtnEvent e = _buttons.consume();

    if (_screen == Screen::SPLASH) {
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
        case BtnEvent::UP:
            _cursorUp(); break;
        case BtnEvent::UP_DBL:
            _cursor = 0; _scrollOffset = 0; _needsRedraw = true; break;
        case BtnEvent::DOWN:
            _cursorDown(count); break;
        case BtnEvent::DOWN_DBL:
            _cursor = max(0, count - 1);
            _scrollOffset = max(0, count - MENU_MAX_ITEMS);
            _needsRedraw = true; break;
        case BtnEvent::SELECT:
            _select(count); break;
        case BtnEvent::BACK:
            _back(); break;
        case BtnEvent::BACK_LONG:
            _navigate(Screen::MAIN_MENU); break;
        default: break;
    }

    if (_needsRedraw) { _redraw(); _needsRedraw = false; }
}

// ─────────────────────────────────────────
//  Navigation
// ─────────────────────────────────────────
void Menu::_navigate(Screen to) {
    _prevScreen = _screen; _screen = to;
    _cursor = 0; _scrollOffset = 0; _needsRedraw = true;
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

void Menu::_cursorUp() {
    if (_cursor > 0) {
        _cursor--;
        if (_cursor < _scrollOffset) _scrollOffset--;
        _needsRedraw = true;
    }
}

void Menu::_cursorDown(int16_t count) {
    if (_cursor < count - 1) {
        _cursor++;
        if (_cursor >= _scrollOffset + MENU_MAX_ITEMS) _scrollOffset++;
        _needsRedraw = true;
    }
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
        case Screen::SPLASH:      _renderSplash();     break;
        case Screen::MAIN_MENU:   _renderMainMenu();   break;
        case Screen::WIFI_MENU:   _renderWifiMenu();   break;
        case Screen::BLE_MENU:    _renderBleMenu();    break;
        case Screen::SUBGHZ_MENU: _renderSubGhzMenu(); break;

        case Screen::WIFI_SCAN:
            _renderRunningScreen("WIFI SCAN",    "SEL:rescan"); break;
        case Screen::WIFI_DEAUTH:
            _renderRunningScreen("DEAUTH",       "L-SEL:toggle  DBL:retarget"); break;
        case Screen::WIFI_BEACON_SPAM:
            _renderRunningScreen("BEACON SPAM",  "L-SEL:toggle"); break;
        case Screen::WIFI_PROBE_SNIFF:
            _renderRunningScreen("PROBE SNIFF",  "BCK:stop"); break;
        case Screen::BLE_SCAN:
            _renderRunningScreen("BLE SCAN",     "SEL:rescan"); break;
        case Screen::BLE_SPAM:
            _renderRunningScreen("BLE SPAM",     "L-SEL:toggle"); break;
        case Screen::SUBGHZ_SCAN:
            _renderRunningScreen("SUB-GHZ SCAN", "BCK:stop"); break;
        case Screen::SUBGHZ_CAPTURE:
            _renderRunningScreen("CAPTURE",      "BCK:cancel"); break;
        case Screen::SUBGHZ_REPLAY:
            _renderRunningScreen("REPLAY",       "SEL:again  BCK:back"); break;
        default: break;
    }
}

void Menu::_renderMenu(const MenuItem* items, uint8_t count, const char* title) {
    _display.clear();
    _display.drawStatusBar(title);

    if (_cursor >= count) _cursor = count - 1;
    if (_cursor >= _scrollOffset + MENU_MAX_ITEMS) _scrollOffset = _cursor - MENU_MAX_ITEMS + 1;
    if (_cursor < _scrollOffset) _scrollOffset = _cursor;

    for (int16_t i = 0; i < MENU_MAX_ITEMS && (i + _scrollOffset) < count; i++) {
        int16_t idx = i + _scrollOffset;
        _display.drawMenuItem(items[idx].label, i,
                              idx == _cursor,
                              items[idx].labelColor);
    }

    _display.drawScrollIndicator(_cursor, count);
}

void Menu::_renderSplash()     { _display.drawSplash(); }
void Menu::_renderMainMenu()   { _renderMenu(MAIN_ITEMS,   3, "OUROBOROS"); }
void Menu::_renderWifiMenu()   { _renderMenu(WIFI_ITEMS,   5, "WIFI"); }
void Menu::_renderBleMenu()    { _renderMenu(BLE_ITEMS,    2, "BLUETOOTH"); }
void Menu::_renderSubGhzMenu() { _renderMenu(SUBGHZ_ITEMS, 5, "SUB-GHZ"); }

void Menu::_renderRunningScreen(const char* title, const char* hint) {
    _display.drawStatusBar(title, true);   // true = active dot
    _display.drawHintBar(hint);
}
