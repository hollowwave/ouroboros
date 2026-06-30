#include "Menu.h"

static const MenuItem MAIN_ITEMS[] = {
    { "WiFi",    Screen::WIFI_MENU,   CLR_TEXT, Glyph::WIFI   },
    { "BLE",     Screen::BLE_MENU,    CLR_TEXT, Glyph::BLE    },
    { "Sub-GHz", Screen::SUBGHZ_MENU, CLR_TEXT, Glyph::SUBGHZ },
};
static const MenuItem WIFI_ITEMS[] = {
    { "Scan",        Screen::WIFI_SCAN,          CLR_TEXT,   Glyph::SCAN   },
    { "Deauth",      Screen::WIFI_DEAUTH_PICKER, CLR_ACCENT, Glyph::ATTACK },
    { "Beacon Spam", Screen::WIFI_BEACON_SPAM,   CLR_ACCENT, Glyph::ATTACK },
    { "Probe Sniff", Screen::WIFI_PROBE_SNIFF,   CLR_TEXT,   Glyph::SCAN   },
    { "RSSI Mapper", Screen::WIFI_MAPPER,        CLR_TEXT,   Glyph::MAPPER },
};
static const MenuItem BLE_ITEMS[] = {
    { "Scan",  Screen::BLE_SCAN,  CLR_TEXT,   Glyph::SCAN   },
    { "Spam",  Screen::BLE_SPAM,  CLR_ACCENT, Glyph::ATTACK },
};
static const MenuItem SUBGHZ_ITEMS[] = {
    { "Scan",        Screen::SUBGHZ_SCAN,    CLR_TEXT, Glyph::SCAN   },
    { "Capture",     Screen::SUBGHZ_CAPTURE, CLR_TEXT, Glyph::SCAN   },
    { "Replay",      Screen::SUBGHZ_REPLAY,  CLR_TEXT, Glyph::ATTACK },
    { "Code Detect", Screen::SUBGHZ_ROLLING, CLR_TEXT, Glyph::CONFIG },
    { "Config",      Screen::SUBGHZ_CONFIG,  CLR_DIM,  Glyph::CONFIG },
};

static uint32_t bootStartTime = 0;
static bool bootTransitioned = false;

Menu::Menu(Display& display, Buttons& buttons)
    : _display(display), _buttons(buttons) {}

void Menu::begin() {
    _navigate(Screen::BOOT);
    bootStartTime = millis();
    bootTransitioned = false;
}

void Menu::tick() {
    if (_screen == Screen::SUBGHZ_CONFIG      ||
        _screen == Screen::SUBGHZ_ROLLING     ||
        _screen == Screen::WIFI_DEAUTH_PICKER ||
        _screen == Screen::WIFI_MAPPER) return;

    BtnEvent e = _buttons.consume();

    // Boot screen — auto-transition after 3 seconds, ONLY on first boot
    if (_screen == Screen::BOOT) {
        if (!bootTransitioned && millis() - bootStartTime >= 3000) {
            _navigate(Screen::MAIN_MENU);
            bootTransitioned = true;
        }
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

void Menu::_redraw() {
    switch (_screen) {
        case Screen::BOOT:        _renderBoot();       break;
        case Screen::MAIN_MENU:   _renderMainMenu();   break;
        case Screen::WIFI_MENU:   _renderWifiMenu();   break;
        case Screen::BLE_MENU:    _renderBleMenu();    break;
        case Screen::SUBGHZ_MENU: _renderSubGhzMenu(); break;

        case Screen::WIFI_SCAN:
            _renderRunningScreen("WIFI", Glyph::WIFI, "Scan",
                Glyph::DOT, "rescan", Glyph::CHEVRON_LEFT, "back");        break;
        case Screen::WIFI_DEAUTH:
            _renderRunningScreen("WIFI", Glyph::WIFI, "Deauth",
                Glyph::DOT, "toggle", Glyph::SCAN, "retarget");            break;
        case Screen::WIFI_BEACON_SPAM:
            _renderRunningScreen("WIFI", Glyph::WIFI, "Beacon Spam",
                Glyph::DOT, "toggle", Glyph::CHEVRON_LEFT, "stop");        break;
        case Screen::WIFI_PROBE_SNIFF:
            _renderRunningScreen("WIFI", Glyph::WIFI, "Probe Sniff",
                Glyph::NONE, nullptr, Glyph::CHEVRON_LEFT, "stop");        break;
        case Screen::BLE_SCAN:
            _renderRunningScreen("BLE", Glyph::BLE, "Scan",
                Glyph::DOT, "rescan", Glyph::CHEVRON_LEFT, "back");        break;
        case Screen::BLE_SPAM:
            _renderRunningScreen("BLE", Glyph::BLE, "Spam",
                Glyph::DOT, "toggle", Glyph::CHEVRON_LEFT, "stop");        break;
        case Screen::SUBGHZ_SCAN:
            _renderRunningScreen("SUB-GHZ", Glyph::SUBGHZ, "Scan",
                Glyph::NONE, nullptr, Glyph::CHEVRON_LEFT, "stop");        break;
        case Screen::SUBGHZ_CAPTURE:
            _renderRunningScreen("SUB-GHZ", Glyph::SUBGHZ, "Capture",
                Glyph::NONE, nullptr, Glyph::CHEVRON_LEFT, "cancel");      break;
        case Screen::SUBGHZ_REPLAY:
            _renderRunningScreen("SUB-GHZ", Glyph::SUBGHZ, "Replay",
                Glyph::DOT, "again", Glyph::CHEVRON_LEFT, "back");         break;
        default: break;
    }
}

void Menu::_renderSelector(const MenuItem* items, uint8_t count,
                            const char* category, Glyph categoryIcon) {
    _display.clear();
    _display.drawStatusBar(category, false, categoryIcon);

    const MenuItem& item = items[_cursor];

    int16_t contentTop = STATUSBAR_H + 1;
    int16_t contentBot = SCREEN_H - 22;
    int16_t centerY    = contentTop + (contentBot - contentTop) / 2;

    // Icon sits above the label, both centered as a block
    if (item.icon != Glyph::NONE) {
        _display.drawGlyph(item.icon, (SCREEN_W - 8) / 2, centerY - 14, item.labelColor);
    }

    int16_t labelY = centerY - 4;
    int16_t labelW = strlen(item.label) * 6;
    int16_t labelX = (SCREEN_W - labelW) / 2;
    _display.drawText(item.label, labelX, labelY, 1, item.labelColor);

    _display.drawGlyph(Glyph::CHEVRON_LEFT,  2,             labelY - 1, CLR_ACCENT);
    _display.drawGlyph(Glyph::CHEVRON_RIGHT, SCREEN_W - 10, labelY - 1, CLR_ACCENT);

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

    _display.drawHintGlyphs(Glyph::DOT, "enter", Glyph::CHEVRON_LEFT, "back");
}

void Menu::_renderRunningScreen(const char* category, Glyph categoryIcon,
                                 const char* module,
                                 Glyph leftHint, const char* leftLabel,
                                 Glyph rightHint, const char* rightLabel) {
    char breadcrumb[32];
    snprintf(breadcrumb, sizeof(breadcrumb), "%s > %s", category, module);
    _display.drawStatusBar(breadcrumb, true, categoryIcon);
    _display.drawHintGlyphs(leftHint, leftLabel, rightHint, rightLabel);
}

void Menu::_renderBoot() {
    _display.clear();

    _display.raw().setTextColor(CLR_ACCENT);
    _display.raw().setTextSize(1);
    _display.raw().setCursor(56, 32);
    _display.raw().print("8");

    _display.raw().setTextColor(CLR_ACCENT);
    _display.raw().setTextSize(1);
    int16_t titleW = 9 * 6;
    _display.raw().setCursor((SCREEN_W - titleW) / 2, 50);
    _display.raw().print("OUROBOROS");

    _display.raw().drawFastHLine(20, 62, SCREEN_W - 40, CLR_ACCENT_DIM);

    _display.drawCenteredText("by hollowwave", 72, CLR_ACCENT_DIM);

    _display.drawCenteredText("initializing...", 100, CLR_SUBTLE);
}

void Menu::_renderMainMenu()   { _renderSelector(MAIN_ITEMS,   3, "OUROBOROS", Glyph::NONE); }
void Menu::_renderWifiMenu()   { _renderSelector(WIFI_ITEMS,   5, "WIFI",      Glyph::WIFI); }
void Menu::_renderBleMenu()    { _renderSelector(BLE_ITEMS,    2, "BLUETOOTH", Glyph::BLE); }
void Menu::_renderSubGhzMenu() { _renderSelector(SUBGHZ_ITEMS, 5, "SUB-GHZ",   Glyph::SUBGHZ); }
