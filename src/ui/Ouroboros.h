#pragma once
#include <Arduino.h>
#include "Display.h"
#include "config.h"

// ─────────────────────────────────────────
//  Ouroboros — animated snake loading wheel
//
//  8 segments in a circle, one gap (mouth)
//  that rotates clockwise each tick.
//
//  Can be drawn anywhere on screen with
//  a custom center point and radius.
// ─────────────────────────────────────────
class Ouroboros {
public:
    Ouroboros(Display& display);

    // Call every loop() — auto-advances frame on timer
    void tick();

    // Draw at specific position (for loading states)
    void drawAt(int16_t cx, int16_t cy, uint8_t radius,
                uint8_t frame, uint16_t color = CLR_ACCENT);

    // Draw splash — full branded screen
    void drawSplash(uint8_t frame);

    // Reset animation frame
    void reset() { _frame = 0; _lastTick = 0; }

    uint8_t frame() const { return _frame; }

private:
    Display& _display;
    uint8_t  _frame    = 0;
    uint32_t _lastTick = 0;

    // Segment dot positions (precomputed from angles)
    // 8 segments at 45° intervals starting from top
    static const int8_t SEG_X[8];
    static const int8_t SEG_Y[8];

    void _drawSegment(int16_t cx, int16_t cy, uint8_t radius,
                      uint8_t seg, uint16_t color);
};
