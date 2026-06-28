#include "DeauthPicker.h"

DeauthPicker::DeauthPicker(Display& display, Buttons& buttons, WiFiAttack& wifi)
    : _display(display), _buttons(buttons), _wifi(wifi) {}

void DeauthPicker::enter() {
    _done         = false;
    _cursor       = 0;
    _target       = -1;
    _scrollOffset = 0;
    _confirming   = false;
    _needsRedraw  = true;
}

void DeauthPicker::tick() {
    if (_done) return;

    BtnEvent e = _buttons.consume();

    if (_wifi.apCount() == 0) {
        if (e == BtnEvent::BACK || e == BtnEvent::SELECT) { _done = true; return; }
        if (_needsRedraw) { _drawNoAPs(); _needsRedraw = false; }
        return;
    }

    if (_confirming) {
        // ── Confirmation screen ──────────────────
        switch (e) {
            case BtnEvent::SELECT:
            case BtnEvent::SELECT_LONG:
                // Confirmed — exit with chosen target
                _done = true;
                break;
            case BtnEvent::BACK:
                // Go back to list
                _confirming  = false;
                _needsRedraw = true;
                break;
            default: break;
        }
        if (_needsRedraw) { _drawConfirm(); _needsRedraw = false; }
        return;
    }

    // ── AP list navigation ───────────────────────
    int8_t count = _wifi.apCount() + 1;   // +1 for "All APs" at top

    switch (e) {
        case BtnEvent::UP:
            if (_cursor > 0) {
                _cursor--;
                if (_cursor < _scrollOffset) _scrollOffset--;
                _needsRedraw = true;
            }
            break;
        case BtnEvent::DOWN:
            if (_cursor < count - 1) {
                _cursor++;
                if (_cursor >= _scrollOffset + MENU_MAX_ITEMS) _scrollOffset++;
                _needsRedraw = true;
            }
            break;
        case BtnEvent::UP_DBL:
            _cursor = 0; _scrollOffset = 0;
            _needsRedraw = true;
            break;
        case BtnEvent::DOWN_DBL:
            _cursor = count - 1;
            _scrollOffset = max(0, count - MENU_MAX_ITEMS);
            _needsRedraw = true;
            break;
        case BtnEvent::SELECT:
            // cursor 0 = broadcast all, 1+ = specific AP (index cursor-1)
            _target     = (_cursor == 0) ? -1 : _cursor - 1;
            _confirming = true;
            _needsRedraw = true;
            break;
        case BtnEvent::BACK:
        case BtnEvent::BACK_LONG:
            _target = -2;   // Signal cancelled
            _done   = true;
            break;
        default: break;
    }

    if (_needsRedraw) { _drawList(); _needsRedraw = false; }
}

// ─────────────────────────────────────────
//  Display
// ─────────────────────────────────────────

void DeauthPicker::_drawList() {
    _display.clear();
    _display.drawStatusBar("Deauth Target");

    int8_t count = _wifi.apCount() + 1;

    for (int8_t i = 0; i < MENU_MAX_ITEMS && (i + _scrollOffset) < count; i++) {
        int8_t  idx      = i + _scrollOffset;
        bool    selected = (idx == _cursor);
        int16_t y        = STATUSBAR_H + 2 + (i * MENU_ITEM_H);

        uint16_t bg  = selected ? CLR_DANGER : CLR_BG;
        uint16_t fg  = selected ? CLR_BG     : CLR_TEXT;

        _display.fillRect(0, y, SCREEN_W, MENU_ITEM_H, bg);

        if (idx == 0) {
            // Broadcast all option
            uint16_t color = selected ? CLR_BG : CLR_DANGER;
            _display.drawText("[ALL APs]", 4, y + 3, 1, color);
        } else {
            const APInfo* ap = &_wifi.apList()[idx - 1];
            char label[18];
            snprintf(label, sizeof(label), "%.14s", ap->ssid[0] ? ap->ssid : "[hidden]");
            _display.drawText(label, 4, y + 3, 1, fg);

            char rssiStr[8];
            snprintf(rssiStr, sizeof(rssiStr), "%d", ap->rssi);
            _display.drawTextRight(rssiStr, y + 3, selected ? CLR_BG : CLR_DIM);
        }
    }

    _display.drawScrollIndicator(_cursor, count);

    // Hint bar
    int16_t y = SCREEN_H - 10;
    _display.fillRect(0, y - 1, SCREEN_W, 11, CLR_HIGHLIGHT);
    _display.drawText("SEL:pick  BCK:cancel", 2, y + 1, 1, CLR_DIM);
}

void DeauthPicker::_drawConfirm() {
    _display.clear();
    _display.drawStatusBar("Confirm");

    _display.drawCenteredText("Target:", STATUSBAR_H + 10, CLR_DIM);

    if (_target == -1) {
        _display.drawCenteredText("ALL APs",      STATUSBAR_H + 24, CLR_DANGER);
        _display.drawCenteredText("Broadcast",    STATUSBAR_H + 36, CLR_DIM);
        _display.drawCenteredText("deauth",       STATUSBAR_H + 46, CLR_DIM);
    } else {
        const APInfo* ap = &_wifi.apList()[_target];
        char label[18];
        snprintf(label, sizeof(label), "%.14s", ap->ssid[0] ? ap->ssid : "[hidden]");
        _display.drawCenteredText(label,          STATUSBAR_H + 24, CLR_DANGER);

        char chStr[16];
        snprintf(chStr, sizeof(chStr), "CH%d  %ddBm", ap->channel, ap->rssi);
        _display.drawCenteredText(chStr,          STATUSBAR_H + 36, CLR_DIM);

        // MAC address
        char macStr[20];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
            ap->bssid[0], ap->bssid[1], ap->bssid[2],
            ap->bssid[3], ap->bssid[4], ap->bssid[5]);
        _display.drawCenteredText(macStr, STATUSBAR_H + 48, CLR_DIM);
    }

    // Hint bar
    int16_t y = SCREEN_H - 10;
    _display.fillRect(0, y - 1, SCREEN_W, 11, CLR_HIGHLIGHT);
    _display.drawText("SEL:start  BCK:back", 2, y + 1, 1, CLR_DIM);
}

void DeauthPicker::_drawNoAPs() {
    _display.clear();
    _display.drawStatusBar("Deauth Target");
    _display.drawCenteredText("No APs found",  SCREEN_H / 2 - 8, CLR_DIM);
    _display.drawCenteredText("Run WiFi scan", SCREEN_H / 2 + 4, CLR_DIM);
    _display.drawCenteredText("first",         SCREEN_H / 2 + 14, CLR_DIM);
}
