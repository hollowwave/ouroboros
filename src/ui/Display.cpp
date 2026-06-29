#include "Display.h"

void Display::begin() {
    _tft.init();
    _tft.setRotation(0);
    _tft.fillScreen(CLR_BG);
}

void Display::clear(uint16_t color) {
    _tft.fillScreen(color);
}

static inline int16_t clampX(int16_t x) { return constrain(x, 0, SCREEN_W - 1); }
static inline int16_t clampY(int16_t y) { return constrain(y, 0, SCREEN_H - 1); }

void Display::drawText(const char* text, int16_t x, int16_t y,
                       uint8_t size, uint16_t color) {
    if (y > SCREEN_H - 8 || x > SCREEN_W) return;
    _tft.setTextColor(color);   // Transparent — no background
    _tft.setTextSize(size);
    _tft.setCursor(clampX(x), clampY(y));
    _tft.print(text);
}

void Display::drawTextRight(const char* text, int16_t y, uint16_t color) {
    int16_t w = strlen(text) * 6;
    drawText(text, SCREEN_W - w - 2, y, 1, color);
}

void Display::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    _tft.drawLine(clampX(x0), clampY(y0), clampX(x1), clampY(y1), color);
}

void Display::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x >= SCREEN_W || y >= SCREEN_H) return;
    _tft.drawRect(clampX(x), clampY(y),
                  min((int16_t)(SCREEN_W - x), w),
                  min((int16_t)(SCREEN_H - y), h), color);
}

void Display::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x >= SCREEN_W || y >= SCREEN_H) return;
    _tft.fillRect(clampX(x), clampY(y),
                  min((int16_t)(SCREEN_W - x), w),
                  min((int16_t)(SCREEN_H - y), h), color);
}

void Display::clearContent() {
    fillRect(0, STATUSBAR_H + 1, SCREEN_W,
             SCREEN_H - STATUSBAR_H - 12, CLR_BG);
}

void Display::drawMenuItem(const char* label, int16_t index, bool selected,
                           uint16_t labelColor) {
    int16_t y = STATUSBAR_H + 1 + (index * MENU_ITEM_H);
    if (y + MENU_ITEM_H > SCREEN_H - 11) return;

    if (selected) {
        fillRect(0, y, SCREEN_W, MENU_ITEM_H, CLR_SUBTLE);
        fillRect(0, y, 2, MENU_ITEM_H, CLR_ACCENT);
        _tft.setTextColor(CLR_ACCENT);   // Transparent
        _tft.setTextSize(1);
        _tft.setCursor(6, y + 3);
        _tft.print(label);
        _tft.setTextColor(CLR_ACCENT_DIM);   // Transparent
        _tft.setCursor(SCREEN_W - 8, y + 3);
        _tft.print(">");
    } else {
        fillRect(0, y, SCREEN_W, MENU_ITEM_H, CLR_BG);
        _tft.setTextColor(labelColor);   // Transparent
        _tft.setTextSize(1);
        _tft.setCursor(6, y + 3);
        _tft.print(label);
    }

    if (y + MENU_ITEM_H < SCREEN_H - 11)
        _tft.drawFastHLine(0, y + MENU_ITEM_H - 1, SCREEN_W, CLR_SUBTLE);
}

void Display::drawScrollIndicator(int16_t current, int16_t total) {
    if (total <= MENU_MAX_ITEMS) return;
    int16_t barTop = STATUSBAR_H + 1;
    int16_t barBot = SCREEN_H - 12;
    int16_t barH   = barBot - barTop;
    int16_t segH   = max(3, barH / total);
    int16_t posY   = barTop + (current * barH / total);
    posY = constrain(posY, barTop, barBot - segH);
    fillRect(SCREEN_W - 2, barTop, 2, barH, CLR_SUBTLE);
    fillRect(SCREEN_W - 2, posY,   2, segH, CLR_ACCENT);
}

void Display::drawStatusBar(const char* moduleName, bool active) {
    _tft.fillRect(0, 0, SCREEN_W, STATUSBAR_H, CLR_ACCENT);
    _tft.setTextColor(CLR_BG, CLR_ACCENT);   // Black text ON red background
    _tft.setTextSize(1);
    _tft.setCursor(4, 3);
    _tft.print(moduleName);

    if (active)
        _tft.fillCircle(SCREEN_W - 6, STATUSBAR_H / 2, 3, CLR_BG);
    else
        _tft.drawCircle(SCREEN_W - 6, STATUSBAR_H / 2, 3, CLR_BG);

    _tft.drawFastHLine(0, STATUSBAR_H, SCREEN_W, CLR_ACCENT_GLOW);
}

void Display::drawHintBar(const char* hint) {
    int16_t y = SCREEN_H - 11;
    _tft.fillRect(0, y, SCREEN_W, 11, CLR_SUBTLE);
    _tft.drawFastHLine(0, y, SCREEN_W, CLR_ACCENT_DIM);
    _tft.setTextColor(CLR_DIM);   // Transparent
    _tft.setTextSize(1);
    _tft.setCursor(3, y + 2);
    _tft.print(hint);
}

void Display::drawCenteredText(const char* text, int16_t y, uint16_t color) {
    if (y < STATUSBAR_H + 1 || y > SCREEN_H - 12) return;
    int16_t w = strlen(text) * 6;
    drawText(text, (SCREEN_W - w) / 2, y, 1, color);
}

void Display::drawSignalBars(int16_t x, int16_t y, int8_t rssi) {
    uint8_t bars = 0;
    if      (rssi >= -55) bars = 5;
    else if (rssi >= -65) bars = 4;
    else if (rssi >= -75) bars = 3;
    else if (rssi >= -85) bars = 2;
    else if (rssi >= -95) bars = 1;

    for (uint8_t i = 0; i < 5; i++) {
        int16_t bh = 2 + i * 2;
        int16_t bx = x + i * 4;
        int16_t by = y + (10 - bh);
        if (by < STATUSBAR_H + 1 || by + bh > SCREEN_H - 12) continue;
        uint16_t c = (i < bars) ? CLR_ACCENT : CLR_SUBTLE;
        _tft.fillRect(bx, by, 3, bh, c);
    }
}

void Display::drawSplash() {
    _tft.fillScreen(CLR_BG);

    _tft.setTextColor(CLR_ACCENT, CLR_BG);
    _tft.setTextSize(2);
    _tft.setCursor((SCREEN_W - 72) / 2, 34);
    _tft.print("OURO");

    _tft.setTextColor(CLR_ACCENT_DIM, CLR_BG);
    _tft.setCursor((SCREEN_W - 72) / 2, 52);
    _tft.print("BOROS");

    _tft.drawFastHLine(20, 72, SCREEN_W - 40, CLR_ACCENT_DIM);

    drawCenteredText("v1.0-beta",     80, CLR_DIM);
    drawCenteredText("by hollowwave",  92, CLR_DIM);
    drawCenteredText("press any key", 110, CLR_SUBTLE);
}
