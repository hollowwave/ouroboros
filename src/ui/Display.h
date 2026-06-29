#pragma once
#include <TFT_eSPI.h>
#include "config.h"

class Display {
public:
    void begin();

    // ── Primitives ──────────────────────────────
    void clear(uint16_t color = CLR_BG);
    void clearContent();   // Wipe only the safe content area
    void drawText(const char* text, int16_t x, int16_t y,
                  uint8_t size = 1, uint16_t color = CLR_TEXT);
    void drawTextRight(const char* text, int16_t y, uint16_t color = CLR_TEXT);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

    // ── Menu ────────────────────────────────────
    void drawMenuItem(const char* label, int16_t index, bool selected,
                      uint16_t labelColor = CLR_TEXT);
    void drawScrollIndicator(int16_t current, int16_t total);

    // ── Chrome ──────────────────────────────────
    void drawStatusBar(const char* moduleName, bool active = false);
    void drawHintBar(const char* hint);

    // ── Helpers ─────────────────────────────────
    void drawCenteredText(const char* text, int16_t y, uint16_t color = CLR_TEXT);
    void drawSignalBars(int16_t x, int16_t y, int8_t rssi);
    void drawSplash();

    TFT_eSPI& raw() { return _tft; }

private:
    TFT_eSPI _tft;
};
