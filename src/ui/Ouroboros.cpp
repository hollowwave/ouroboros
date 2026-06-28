#include "Ouroboros.h"
#include <math.h>

// Precomputed segment offsets at radius=1 (scaled at draw time)
// 8 positions at 45° intervals, starting from top (12 o'clock), clockwise
// Values are * 100 for integer math, divide at draw time
const int8_t Ouroboros::SEG_X[8] = {  0,  71, 100,  71,  0, -71, -100, -71 };
const int8_t Ouroboros::SEG_Y[8] = { -100, -71,  0,  71, 100,  71,    0, -71 };

Ouroboros::Ouroboros(Display& display) : _display(display) {}

void Ouroboros::tick() {
    if (millis() - _lastTick >= OURO_ANIM_MS) {
        _lastTick = millis();
        _frame    = (_frame + 1) % OURO_SEGMENTS;
    }
}

void Ouroboros::drawAt(int16_t cx, int16_t cy, uint8_t radius,
                       uint8_t frame, uint16_t color) {
    // Draw all segments except the gap (mouth = current frame position)
    // Tail segment (frame+1) is drawn dimmer — gives depth
    for (uint8_t i = 0; i < OURO_SEGMENTS; i++) {
        uint8_t seg = (frame + i) % OURO_SEGMENTS;

        if (i == 0) continue;   // Gap — mouth eating tail, skip

        uint16_t c;
        if      (i == OURO_SEGMENTS - 1) c = CLR_ACCENT_DIM;   // Tail — darkest
        else if (i == OURO_SEGMENTS - 2) c = CLR_ACCENT_DIM;   // Near tail
        else                              c = color;             // Body

        _drawSegment(cx, cy, radius, seg, c);
    }

    // Erase previous gap position so old segment doesn't linger
    uint8_t prevGap = (frame + OURO_SEGMENTS - 1) % OURO_SEGMENTS;
    _drawSegment(cx, cy, radius, (frame - 1 + OURO_SEGMENTS) % OURO_SEGMENTS, CLR_BG);
}

void Ouroboros::_drawSegment(int16_t cx, int16_t cy, uint8_t radius,
                              uint8_t seg, uint16_t color) {
    // Scale precomputed unit-circle offsets by radius
    int16_t x = cx + (int16_t)(SEG_X[seg] * radius / 100);
    int16_t y = cy + (int16_t)(SEG_Y[seg] * radius / 100);
    // Draw as a filled circle of size 3px
    _display.raw().fillCircle(x, y, 3, color);
}

void Ouroboros::drawSplash(uint8_t frame) {
    // ── Background ───────────────────────────────
    _display.clear();

    // ── Snake ring ───────────────────────────────
    // Draw full ring first in dim red (body base)
    for (uint8_t i = 0; i < OURO_SEGMENTS; i++) {
        _drawSegment(OURO_CX, OURO_CY, OURO_RADIUS, i, CLR_ACCENT_DIM);
    }
    // Overdraw animated segments in bright red
    drawAt(OURO_CX, OURO_CY, OURO_RADIUS, frame, CLR_ACCENT);

    // ── Head dot (slightly larger) ───────────────
    uint8_t headSeg = (frame + 1) % OURO_SEGMENTS;
    int16_t hx = OURO_CX + (int16_t)(SEG_X[headSeg] * OURO_RADIUS / 100);
    int16_t hy = OURO_CY + (int16_t)(SEG_Y[headSeg] * OURO_RADIUS / 100);
    _display.raw().fillCircle(hx, hy, 4, CLR_ACCENT_GLOW);

    // ── Title ────────────────────────────────────
    _display.raw().setTextColor(CLR_ACCENT, CLR_BG);
    _display.raw().setTextSize(2);
    // Center "OURO" — 4 chars × 12px = 48px
    _display.raw().setCursor((SCREEN_W - 48) / 2, 82);
    _display.raw().print("OURO");

    _display.raw().setTextColor(CLR_DIM, CLR_BG);
    _display.raw().setTextSize(1);
    // Center "BOROS"
    _display.raw().setCursor((SCREEN_W - 30) / 2, 98);
    _display.raw().print("BOROS");

    // ── Divider ──────────────────────────────────
    _display.raw().drawFastHLine(24, 110, SCREEN_W - 48, CLR_ACCENT_DIM);

    // ── Hint ─────────────────────────────────────
    _display.drawCenteredText("press any key", 115, CLR_DIM);
}
