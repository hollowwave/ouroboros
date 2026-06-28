#pragma once
#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include "../ui/Display.h"
#include "../utils/Buttons.h"
#include "config.h"

#define MAX_CAPTURES 10
#define MAX_CAPTURE_LEN 64

enum class CodeType {
    UNKNOWN,
    FIXED,        // Every capture identical — fixed code remote
    ROLLING,      // Captures differ — likely rolling code
    INCONSISTENT  // Too noisy to determine
};

struct CaptureEntry {
    uint8_t data[MAX_CAPTURE_LEN];
    uint8_t length;
    int     rssi;
    bool    used;
};

class RollingCodeDetector {
public:
    RollingCodeDetector(Display& display, Buttons& buttons);

    void enter(float frequency);
    void tick();
    void stop();

    bool isDone() const { return _done; }

private:
    Display& _display;
    Buttons& _buttons;

    float       _frequency  = 433.92f;
    bool        _done       = false;
    bool        _capturing  = false;
    bool        _needsRedraw = true;
    uint8_t     _captureCount = 0;

    CaptureEntry _captures[MAX_CAPTURES];

    CodeType    _result     = CodeType::UNKNOWN;

    // ── Analysis ─────────────────────────────────
    CodeType  _analyze();
    bool      _capturesMatch(const CaptureEntry& a, const CaptureEntry& b);
    uint8_t   _hammingDiff(const uint8_t* a, const uint8_t* b, uint8_t len);

    // ── Display ──────────────────────────────────
    void _drawWaiting();
    void _drawCapturing();
    void _drawResult(CodeType type);
    void _drawProgress();
};
