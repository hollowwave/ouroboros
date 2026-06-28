#pragma once
#include <Arduino.h>
#include "../ui/Display.h"
#include "../utils/Buttons.h"
#include "WiFiAttack.h"
#include "config.h"

class DeauthPicker {
public:
    DeauthPicker(Display& display, Buttons& buttons, WiFiAttack& wifi);

    void enter();     // Load AP list from last WiFi scan
    void tick();
    bool isDone()     const { return _done; }
    int8_t target()   const { return _target; }  // -1 = all, 0+ = specific AP

private:
    Display&    _display;
    Buttons&    _buttons;
    WiFiAttack& _wifi;

    bool    _done        = false;
    bool    _needsRedraw = true;
    int8_t  _cursor      = 0;
    int8_t  _target      = -1;
    int8_t  _scrollOffset = 0;
    bool    _confirming  = false;

    void _drawList();
    void _drawConfirm();
    void _drawNoAPs();
};
