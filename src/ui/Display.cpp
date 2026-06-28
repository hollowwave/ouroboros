#include "Display.h"

void Display::begin() {
    _tft.init();
    _tft.setRotation(0);
    _tft.fillScreen(CLR_BG);
}

void Display::clear(uint16_t color) {
    _tft.fillScreen(color);
}

void Display::drawText(const char* text, int16_t x, int16_t y,
                       uint8_t size, uint16_t color) {
    _tft.setTextColor(color, CLR_BG);
    _tft.setTextSize(size);
    _tft.setCursor(x, y);
    _tft.print(text);
}

void Display::drawTextRight(const char* text, int16_t y, uint16_t color) {
    int16_t w = strlen(text) * 6;
    drawText(text, SCREEN_W - w - 2, y, 1, color);
}

void Display::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    _tft.drawLine(x0, y0, x1, y1, color);
}

void Display::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    _tft.drawRect(x, y, w, h, color);
}

void Display::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    _tft.fillRect(x, y, w, h, color);
}

// ─────────────────────────────────────────
//  Menu item — red selection bar with
//  left accent line for selected state
// ─────────────────────────────────────────
void Display::drawMenuItem(const char* label, int16_t index, bool selected,
                           uint16_t labelColor) {
    int16_t y = STATUSBAR_H + 1 + (index * MENU_ITEM_H);

    if (selected) {
        // Dark red row background
        _tft.fillRect(0,  y, SCREEN_W, MENU_ITEM_H, CLR_SUBTLE);
        // Left accent bar — 2px red stripe
        _tft.fillRect(0,  y, 2, MENU_ITEM_H, CLR_ACCENT);
        // Label in red
        _tft.setTextColor(CLR_ACCENT, CLR_SUBTLE);
        _tft.setTextSize(1);
        _tft.setCursor(6, y + 3);
        _tft.print(label);
        // Arrow
        _tft.setTextColor(CLR_ACCENT_DIM, CLR_SUBTLE);
        _tft.setCursor(SCREEN_W - 8, y + 3);
        _tft.print(">");
    } else {
        _tft.fillRect(0, y, SCREEN_W, MENU_ITEM_H, CLR_BG);
        _tft.setTextColor(labelColor, CLR_BG);
        _tft.setTextSize(1);
        _tft.setCursor(6, y + 3);
        _tft.print(label);
    }

    // Subtle row divider
    _tft.drawFastHLine(0, y + MENU_ITEM_H - 1, SCREEN_W, CLR_SUBTLE);
}

void Display::drawScrollIndicator(int16_t current, int16_t total) {
    if (total <= MENU_MAX_ITEMS) return;
    int16_t barH  = SCREEN_H - STATUSBAR_H - 1;
    int16_t segH  = max(3, barH / total);
    int16_t posY  = STATUSBAR_H + (current * barH / total);
    _tft.fillRect(SCREEN_W - 2, STATUSBAR_H, 2, barH, CLR_SUBTLE);
    _tft.fillRect(SCREEN_W - 2, posY, 2, segH, CLR_ACCENT);
}

// ─────────────────────────────────────────
//  Status bar — red top bar with module
//  name and a right-side indicator dot
// ─────────────────────────────────────────
void Display::drawStatusBar(const char* moduleName, bool active) {
    // Bar background
    _tft.fillRect(0, 0, SCREEN_W, STATUSBAR_H, CLR_ACCENT);

    // Module name in black on red
    _tft.setTextColor(CLR_BG, CLR_ACCENT);
    _tft.setTextSize(1);
    _tft.setCursor(4, 3);
    _tft.print(moduleName);

    // Activity indicator dot — solid if active, outline if idle
    if (active) {
        _tft.fillCircle(SCREEN_W - 6, STATUSBAR_H / 2, 3, CLR_BG);
    } else {
        _tft.drawCircle(SCREEN_W - 6, STATUSBAR_H / 2, 3, CLR_BG);
    }

    // Bottom border line — slightly brighter red
    _tft.drawFastHLine(0, STATUSBAR_H, SCREEN_W, CLR_ACCENT_GLOW);
}

// ─────────────────────────────────────────
//  Hint bar — bottom strip
// ─────────────────────────────────────────
void Display::drawHintBar(const char* hint) {
    int16_t y = SCREEN_H - 11;
    _tft.fillRect(0, y, SCREEN_W, 11, CLR_SUBTLE);
    _tft.drawFastHLine(0, y, SCREEN_W, CLR_ACCENT_DIM);
    _tft.setTextColor(CLR_DIM, CLR_SUBTLE);
    _tft.setTextSize(1);
    _tft.setCursor(3, y + 2);
    _tft.print(hint);
}

// ─────────────────────────────────────────
//  Centered text
// ─────────────────────────────────────────
void Display::drawCenteredText(const char* text, int16_t y, uint16_t color) {
    int16_t w = strlen(text) * 6;
    drawText(text, (SCREEN_W - w) / 2, y, 1, color);
}

// ─────────────────────────────────────────
//  RSSI signal bars (5 bars like WiFi icon)
// ─────────────────────────────────────────
void Display::drawSignalBars(int16_t x, int16_t y, int8_t rssi) {
    // Map rssi to 0-5 bars
    uint8_t bars = 0;
    if      (rssi >= -55) bars = 5;
    else if (rssi >= -65) bars = 4;
    else if (rssi >= -75) bars = 3;
    else if (rssi >= -85) bars = 2;
    else if (rssi >= -95) bars = 1;

    for (uint8_t i = 0; i < 5; i++) {
        int16_t bh = 2 + i * 2;   // Bar heights: 2,4,6,8,10
        int16_t bx = x + i * 4;
        int16_t by = y + (10 - bh);
        uint16_t c = (i < bars) ? CLR_ACCENT : CLR_SUBTLE;
        _tft.fillRect(bx, by, 3, bh, c);
    }
}

// ─────────────────────────────────────────
//  Splash — drawn by Ouroboros class,
//  this is just a fallback static version
// ─────────────────────────────────────────
void Display::drawSplash() {
    _tft.fillScreen(CLR_BG);

    // Title — split size for style
    _tft.setTextColor(CLR_ACCENT, CLR_BG);
    _tft.setTextSize(2);
    int16_t tx = (SCREEN_W - 48) / 2;
    _tft.setCursor(tx, 44);
    _tft.print("OURO");

    _tft.setTextColor(CLR_DIM, CLR_BG);
    _tft.setTextSize(1);
    _tft.setCursor((SCREEN_W - 30) / 2, 62);
    _tft.print("BOROS");

    _tft.drawFastHLine(24, 74, SCREEN_W - 48, CLR_ACCENT_DIM);
    drawCenteredText("press any key", 80, CLR_DIM);
}
