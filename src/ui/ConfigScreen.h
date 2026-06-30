#pragma once
#include <Arduino.h>
#include "../ui/Display.h"
#include "../utils/Buttons.h"
#include "../modules/SubGHz.h"
#include "config.h"

// ─────────────────────────────────────────
//  A single editable config field
// ─────────────────────────────────────────
struct ConfigField {
    const char* label;
    float       value;
    float       minVal;
    float       maxVal;
    float       step;       // Normal UP/DOWN increment
    float       stepFast;   // Long-press UP/DOWN increment
    uint8_t     decimals;   // Display precision
    const char* unit;       // e.g. "MHz", "dBm"
    Glyph       icon = Glyph::CONFIG;
};

// ─────────────────────────────────────────
//  Config screen — scrollable field editor
//  UP/DOWN     → move cursor between fields
//  SELECT      → enter edit mode for field
//  UP/DOWN     → increment/decrement value (in edit mode)
//  UP/DOWN long→ fast increment/decrement
//  SELECT/BACK → confirm and exit edit mode
// ─────────────────────────────────────────
class ConfigScreen {
public:
    ConfigScreen(Display& display, Buttons& buttons, SubGHz& subghz);

    void enter();           // Call when navigating to this screen
    void tick();            // Call every loop()
    bool isDone() const { return _done; }   // True when user pressed BACK to exit

private:
    Display& _display;
    Buttons& _buttons;
    SubGHz&  _subghz;

    bool    _done      = false;
    bool    _editing   = false;   // Are we editing a field or navigating?
    int8_t  _cursor    = 0;
    bool    _needsRedraw = true;

    // ── Fields ───────────────────────────────────
    static const uint8_t FIELD_COUNT = 5;
    ConfigField _fields[FIELD_COUNT] = {
        { "Frequency", 433.92f, 300.0f, 928.0f, 0.01f, 1.0f,  2, "MHz", Glyph::SUBGHZ },
        { "Modulation",  2.0f,   0.0f,   4.0f,  1.0f,  1.0f,  0, "",    Glyph::CONFIG },
        { "Bandwidth",   0.0f,   0.0f,   3.0f,  1.0f,  1.0f,  0, "",    Glyph::CONFIG },
        { "TX Power",    0.0f,   0.0f,   7.0f,  1.0f,  1.0f,  0, "",    Glyph::CONFIG },
        { "Pkt Length", 61.0f,   1.0f,  64.0f,  1.0f,  4.0f,  0, "B",   Glyph::CONFIG },
    };

    // ── Label lookups for enum fields ────────────
    static const char* _modName(int v);
    static const char* _bwName(int v);
    static const char* _txPowerName(int v);

    // ── Rendering ────────────────────────────────
    void _redraw();
    void _drawField(uint8_t idx, bool selected, bool editing);
    void _drawHints();
    void _drawValueBar(float val, float minV, float maxV, int16_t y);

    // ── Actions ──────────────────────────────────
    void _increment(float step);
    void _decrement(float step);
    void _applyAll();
};
