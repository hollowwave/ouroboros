#include "Buttons.h"

Buttons* Buttons::_instance = nullptr;

void Buttons::begin() {
    _instance = this;

    _up.attachClick(_onUp);
    _up.attachLongPressStop(_onUpLong);
    _up.attachDoubleClick(_onUpDbl);

    _down.attachClick(_onDown);
    _down.attachLongPressStop(_onDownLong);
    _down.attachDoubleClick(_onDownDbl);

    _select.attachClick(_onSelect);
    _select.attachLongPressStop(_onSelectLong);
    _select.attachDoubleClick(_onSelectDbl);

    _back.attachClick(_onBack);
    _back.attachLongPressStop(_onBackLong);
    // No double-click on back — too easy to trigger accidentally
}

void Buttons::tick() {
    _up.tick();
    _down.tick();
    _select.tick();
    _back.tick();
}

BtnEvent Buttons::consume() {
    BtnEvent e = _pending;
    _pending = BtnEvent::NONE;
    return e;
}
