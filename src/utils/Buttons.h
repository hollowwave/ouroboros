#pragma once
#include <Arduino.h>
#include <OneButton.h>
#include "config.h"

// ─────────────────────────────────────────
//  Button event types
//
//  4 physical buttons × multiple press types
//  = rich input without chording
// ─────────────────────────────────────────
enum class BtnEvent {
    NONE,

    // Navigation
    UP,
    DOWN,
    SELECT,
    BACK,

    // Long press — held ~800ms
    UP_LONG,        // Fast increment in config screens
    DOWN_LONG,      // Fast decrement in config screens
    SELECT_LONG,    // Start / stop active module
    BACK_LONG,      // Jump to main menu from anywhere

    // Double click
    UP_DBL,         // Page up (skip multiple items)
    DOWN_DBL,       // Page down
    SELECT_DBL,     // Secondary action (e.g. target-select in deauth)
};

class Buttons {
public:
    void begin();
    void tick();          // Call every loop()
    BtnEvent consume();   // Returns and clears pending event

private:
    OneButton _up     { PIN_BTN_UP,     true };
    OneButton _down   { PIN_BTN_DOWN,   true };
    OneButton _select { PIN_BTN_SELECT, true };
    OneButton _back   { PIN_BTN_BACK,   true };

    volatile BtnEvent _pending = BtnEvent::NONE;
    void _set(BtnEvent e) { if (_pending == BtnEvent::NONE) _pending = e; }

    static Buttons* _instance;

    static void _onUp()         { _instance->_set(BtnEvent::UP); }
    static void _onUpLong()     { _instance->_set(BtnEvent::UP_LONG); }
    static void _onUpDbl()      { _instance->_set(BtnEvent::UP_DBL); }
    static void _onDown()       { _instance->_set(BtnEvent::DOWN); }
    static void _onDownLong()   { _instance->_set(BtnEvent::DOWN_LONG); }
    static void _onDownDbl()    { _instance->_set(BtnEvent::DOWN_DBL); }
    static void _onSelect()     { _instance->_set(BtnEvent::SELECT); }
    static void _onSelectLong() { _instance->_set(BtnEvent::SELECT_LONG); }
    static void _onSelectDbl()  { _instance->_set(BtnEvent::SELECT_DBL); }
    static void _onBack()       { _instance->_set(BtnEvent::BACK); }
    static void _onBackLong()   { _instance->_set(BtnEvent::BACK_LONG); }
};
