#include "ConfigScreen.h"
#include <stdio.h>

// ─────────────────────────────────────────
//  Enum label lookups
// ─────────────────────────────────────────
const char* ConfigScreen::_modName(int v) {
    switch (v) {
        case 0: return "2-FSK";
        case 1: return "GFSK";
        case 2: return "ASK/OOK";
        case 3: return "4-FSK";
        case 4: return "MSK";
        default: return "?";
    }
}

const char* ConfigScreen::_bwName(int v) {
    switch (v) {
        case 0: return "58 kHz";
        case 1: return "203 kHz";
        case 2: return "406 kHz";
        case 3: return "812 kHz";
        default: return "?";
    }
}

const char* ConfigScreen::_txPowerName(int v) {
    // CC1101 PA table index → approximate dBm
    switch (v) {
        case 0: return "-30 dBm";
        case 1: return "-20 dBm";
        case 2: return "-15 dBm";
        case 3: return "-10 dBm";
        case 4: return "  0 dBm";
        case 5: return " +5 dBm";
        case 6: return " +7 dBm";
        case 7: return "+10 dBm";
        default: return "?";
    }
}

// ─────────────────────────────────────────

ConfigScreen::ConfigScreen(Display& display, Buttons& buttons, SubGHz& subghz)
    : _display(display), _buttons(buttons), _subghz(subghz) {}

void ConfigScreen::enter() {
    _done        = false;
    _editing     = false;
    _cursor      = 0;
    _needsRedraw = true;
    // Pre-load current SubGHz values into fields
    _fields[0].value = _subghz.getFrequency();
}

void ConfigScreen::tick() {
    if (_done) return;

    BtnEvent e = _buttons.consume();

    if (!_editing) {
        // ── Navigation mode ─────────────────────
        switch (e) {
            case BtnEvent::UP:
                if (_cursor > 0) { _cursor--; _needsRedraw = true; }
                break;
            case BtnEvent::DOWN:
                if (_cursor < FIELD_COUNT - 1) { _cursor++; _needsRedraw = true; }
                break;
            case BtnEvent::UP_DBL:
                _cursor = 0;
                _needsRedraw = true;
                break;
            case BtnEvent::DOWN_DBL:
                _cursor = FIELD_COUNT - 1;
                _needsRedraw = true;
                break;
            case BtnEvent::SELECT:
                _editing     = true;
                _needsRedraw = true;
                break;
            case BtnEvent::BACK:
            case BtnEvent::BACK_LONG:
                _applyAll();
                _done = true;
                break;
            default: break;
        }
    } else {
        // ── Edit mode ───────────────────────────
        switch (e) {
            case BtnEvent::UP:
                _increment(_fields[_cursor].step);
                break;
            case BtnEvent::UP_LONG:
                _increment(_fields[_cursor].stepFast);
                break;
            case BtnEvent::DOWN:
                _decrement(_fields[_cursor].step);
                break;
            case BtnEvent::DOWN_LONG:
                _decrement(_fields[_cursor].stepFast);
                break;
            case BtnEvent::SELECT:
            case BtnEvent::SELECT_LONG:
            case BtnEvent::BACK:
                // Confirm edit, return to navigation
                _editing     = false;
                _needsRedraw = true;
                break;
            case BtnEvent::BACK_LONG:
                // Discard all changes and exit
                _done = true;
                break;
            default: break;
        }
    }

    if (_needsRedraw) {
        _redraw();
        _needsRedraw = false;
    }
}

// ─────────────────────────────────────────
//  Value editing
// ─────────────────────────────────────────

void ConfigScreen::_increment(float step) {
    ConfigField& f = _fields[_cursor];
    f.value = constrain(f.value + step, f.minVal, f.maxVal);
    _needsRedraw = true;
}

void ConfigScreen::_decrement(float step) {
    ConfigField& f = _fields[_cursor];
    f.value = constrain(f.value - step, f.minVal, f.maxVal);
    _needsRedraw = true;
}

void ConfigScreen::_applyAll() {
    _subghz.setFrequency(_fields[0].value);
    _subghz.setModulation((uint8_t)_fields[1].value);
    _subghz.setBandwidth((uint8_t)_fields[2].value);
    // TX power and packet length applied via raw CC1101 calls
    Serial.printf("[Config] Applied: %.2f MHz, mod=%d, bw=%d, pwr=%d, len=%d\n",
        _fields[0].value,
        (int)_fields[1].value,
        (int)_fields[2].value,
        (int)_fields[3].value,
        (int)_fields[4].value);
}

// ─────────────────────────────────────────
//  Rendering
// ─────────────────────────────────────────

void ConfigScreen::_redraw() {
    _display.clear();
    _display.drawStatusBar(_editing ? "  [ EDITING ]" : "Sub-GHz Config");

    // Scroll window — show 4 fields at a time (128px / ~26px per field)
    const int8_t VISIBLE = 4;
    int8_t scrollTop = 0;
    if (_cursor >= VISIBLE) scrollTop = _cursor - VISIBLE + 1;

    for (int8_t i = 0; i < VISIBLE && (i + scrollTop) < FIELD_COUNT; i++) {
        _drawField(i + scrollTop, (i + scrollTop) == _cursor, _editing && (i + scrollTop) == _cursor);
    }

    // Scroll dots at bottom
    if (FIELD_COUNT > VISIBLE) {
        int16_t dotX = (SCREEN_W - FIELD_COUNT * 6) / 2;
        for (int i = 0; i < FIELD_COUNT; i++) {
            uint16_t c = (i == _cursor) ? CLR_ACCENT : CLR_SUBTLE;
            _display.fillRect(dotX + i * 6, SCREEN_H - 5, 4, 4, c);
        }
    }

    _drawHints();
}

void ConfigScreen::_drawField(uint8_t idx, bool selected, bool editing) {
    const ConfigField& f = _fields[idx];
    // 4 fields in content area (128 - 12 statusbar - 16 hints - 8 dots = 92px / 4 = ~23px each)
    int16_t y = STATUSBAR_H + 2 + ((idx - max(0, _cursor - 3)) * 23);

    // Row background
    uint16_t bgColor = editing  ? CLR_ACCENT    :
                       selected ? CLR_SUBTLE  : CLR_BG;
    _display.fillRect(0, y, SCREEN_W, 22, bgColor);

    // Label
    uint16_t labelColor = editing ? CLR_BG : (selected ? CLR_ACCENT : CLR_DIM);
    _display.drawText(f.label, 3, y + 2, 1, labelColor);

    // Value string
    char valStr[24] = "";
    if (idx == 0) {
        // Frequency — show decimal
        snprintf(valStr, sizeof(valStr), "%.*f %s",
                 f.decimals, f.value, f.unit);
    } else if (idx == 1) {
        snprintf(valStr, sizeof(valStr), "%s", _modName((int)f.value));
    } else if (idx == 2) {
        snprintf(valStr, sizeof(valStr), "%s", _bwName((int)f.value));
    } else if (idx == 3) {
        snprintf(valStr, sizeof(valStr), "%s", _txPowerName((int)f.value));
    } else {
        snprintf(valStr, sizeof(valStr), "%d %s", (int)f.value, f.unit);
    }

    uint16_t valColor = editing ? CLR_BG : (selected ? CLR_TEXT : CLR_TEXT);
    _display.drawText(valStr, 3, y + 12, 1, valColor);

    // Thin progress bar under value (shows position in range)
    if (selected && !editing) {
        _drawValueBar(f.value, f.minVal, f.maxVal, y + 20);
    }
    if (editing) {
        // Animated chevrons as increment hints
        _display.drawText("<", 1, y + 10, 1, CLR_BG);
        _display.drawText(">", SCREEN_W - 8, y + 10, 1, CLR_BG);
    }
}

void ConfigScreen::_drawValueBar(float val, float minV, float maxV, int16_t y) {
    int16_t barW = SCREEN_W - 6;
    int16_t fill = (int16_t)((val - minV) / (maxV - minV) * barW);
    _display.fillRect(3,        y, barW, 2, CLR_SUBTLE);
    _display.fillRect(3,        y, fill, 2, CLR_ACCENT);
}

void ConfigScreen::_drawHints() {
    // Bottom hint bar — context sensitive
    int16_t y = SCREEN_H - 10;
    _display.fillRect(0, y - 1, SCREEN_W, 11, CLR_SUBTLE);

    if (!_editing) {
        _display.drawText("SEL:edit", 2,         y + 1, 1, CLR_DIM);
        _display.drawText("BCK:save", SCREEN_W/2, y + 1, 1, CLR_DIM);
    } else {
        _display.drawText("U/D:value", 2,          y + 1, 1, CLR_ACCENT);
        _display.drawText("SEL:ok",    SCREEN_W/2,  y + 1, 1, CLR_DIM);
    }
}
