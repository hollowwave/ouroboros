#include "RollingCodeDetector.h"

RollingCodeDetector::RollingCodeDetector(Display& display, Buttons& buttons)
    : _display(display), _buttons(buttons) {}

void RollingCodeDetector::enter(float frequency) {
    _frequency    = frequency;
    _done         = false;
    _capturing    = false;
    _captureCount = 0;
    _result       = CodeType::UNKNOWN;
    _needsRedraw  = true;
    memset(_captures, 0, sizeof(_captures));

    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setMHZ(_frequency);
    ELECHOUSE_cc1101.setModulation(2);   // ASK/OOK — most common for remotes
    ELECHOUSE_cc1101.SetRx();

    Serial.printf("[RCD] Listening on %.2f MHz\n", _frequency);
}

void RollingCodeDetector::stop() {
    ELECHOUSE_cc1101.setSidle();
    _capturing = false;
}

void RollingCodeDetector::tick() {
    if (_done) return;

    BtnEvent e = _buttons.consume();

    // BACK exits
    if (e == BtnEvent::BACK || e == BtnEvent::BACK_LONG) {
        stop();
        _done = true;
        return;
    }

    // Already have enough captures — show result
    if (_captureCount >= MAX_CAPTURES && _result == CodeType::UNKNOWN) {
        _result      = _analyze();
        _needsRedraw = true;
    }

    // Show result screen — SELECT or BACK to exit
    if (_result != CodeType::UNKNOWN) {
        if (e == BtnEvent::SELECT) { _done = true; return; }
        if (_needsRedraw) { _drawResult(_result); _needsRedraw = false; }
        return;
    }

    // Capture loop
    if (ELECHOUSE_cc1101.CheckReceiveFlag()) {
        CaptureEntry& entry = _captures[_captureCount];
        int len = ELECHOUSE_cc1101.ReceiveData(entry.data);

        if (len > 0 && len <= MAX_CAPTURE_LEN) {
            entry.length = len;
            entry.rssi   = ELECHOUSE_cc1101.getRssi();
            entry.used   = true;
            _captureCount++;
            _needsRedraw = true;

            Serial.printf("[RCD] Capture %d/%d — %d bytes RSSI:%d\n",
                _captureCount, MAX_CAPTURES, len, entry.rssi);

            // Re-arm receiver
            ELECHOUSE_cc1101.SetRx();
        }
    }

    if (_needsRedraw) {
        _drawProgress();
        _needsRedraw = false;
    }
}

// ─────────────────────────────────────────
//  Analysis
// ─────────────────────────────────────────

CodeType RollingCodeDetector::_analyze() {
    if (_captureCount < 2) return CodeType::INCONSISTENT;

    int matchCount    = 0;
    int mismatchCount = 0;
    int comparisons   = 0;

    for (int i = 0; i < _captureCount - 1; i++) {
        for (int j = i + 1; j < _captureCount; j++) {
            if (!_captures[i].used || !_captures[j].used) continue;
            // Only compare same-length captures
            if (_captures[i].length != _captures[j].length) {
                mismatchCount++;
            } else if (_capturesMatch(_captures[i], _captures[j])) {
                matchCount++;
            } else {
                mismatchCount++;
            }
            comparisons++;
        }
    }

    if (comparisons == 0)       return CodeType::INCONSISTENT;
    if (matchCount == comparisons) return CodeType::FIXED;
    if (mismatchCount == comparisons) return CodeType::ROLLING;
    return CodeType::INCONSISTENT;
}

bool RollingCodeDetector::_capturesMatch(const CaptureEntry& a, const CaptureEntry& b) {
    if (a.length != b.length) return false;
    // Allow 1 bit difference for noise tolerance
    return _hammingDiff(a.data, b.data, a.length) <= 1;
}

uint8_t RollingCodeDetector::_hammingDiff(const uint8_t* a, const uint8_t* b, uint8_t len) {
    uint8_t diff = 0;
    for (uint8_t i = 0; i < len; i++) {
        uint8_t x = a[i] ^ b[i];
        // Count set bits
        while (x) { diff += x & 1; x >>= 1; }
    }
    return diff;
}

// ─────────────────────────────────────────
//  Display
// ─────────────────────────────────────────

void RollingCodeDetector::_drawProgress() {
    _display.clear();
    _display.drawStatusBar("Code Detector");

    char freqStr[24];
    snprintf(freqStr, sizeof(freqStr), "%.2f MHz", _frequency);
    _display.drawCenteredText(freqStr, STATUSBAR_H + 8, CLR_ACCENT);

    _display.drawCenteredText("Press remote", STATUSBAR_H + 24, CLR_DIM);
    _display.drawCenteredText("multiple times", STATUSBAR_H + 34, CLR_DIM);

    // Progress bar
    int16_t barW   = SCREEN_W - 20;
    int16_t fillW  = (int16_t)((float)_captureCount / MAX_CAPTURES * barW);
    int16_t barY   = STATUSBAR_H + 52;
    _display.fillRect(10, barY, barW, 6, CLR_SUBTLE);
    _display.fillRect(10, barY, fillW, 6, CLR_ACCENT);

    char countStr[20];
    snprintf(countStr, sizeof(countStr), "%d / %d captures", _captureCount, MAX_CAPTURES);
    _display.drawCenteredText(countStr, barY + 10, CLR_TEXT);

    _display.drawCenteredText("BACK to cancel", SCREEN_H - 10, CLR_DIM);
}

void RollingCodeDetector::_drawResult(CodeType type) {
    _display.clear();
    _display.drawStatusBar("Code Detector");

    char freqStr[24];
    snprintf(freqStr, sizeof(freqStr), "%.2f MHz", _frequency);
    _display.drawCenteredText(freqStr, STATUSBAR_H + 8, CLR_DIM);

    // Big result
    switch (type) {
        case CodeType::FIXED:
            _display.drawCenteredText("FIXED CODE",   STATUSBAR_H + 28, CLR_ACCENT);
            _display.drawCenteredText("All captures", STATUSBAR_H + 44, CLR_DIM);
            _display.drawCenteredText("identical",    STATUSBAR_H + 54, CLR_DIM);
            _display.drawCenteredText("Replayable!",  STATUSBAR_H + 68, CLR_TEXT);
            break;
        case CodeType::ROLLING:
            _display.drawCenteredText("ROLLING CODE", STATUSBAR_H + 28, CLR_WARN);
            _display.drawCenteredText("Captures",     STATUSBAR_H + 44, CLR_DIM);
            _display.drawCenteredText("differ each",  STATUSBAR_H + 54, CLR_DIM);
            _display.drawCenteredText("time",         STATUSBAR_H + 64, CLR_DIM);
            break;
        case CodeType::INCONSISTENT:
            _display.drawCenteredText("INCONSISTENT", STATUSBAR_H + 28, CLR_DANGER);
            _display.drawCenteredText("Noisy signal", STATUSBAR_H + 44, CLR_DIM);
            _display.drawCenteredText("Try again",    STATUSBAR_H + 54, CLR_DIM);
            break;
        default: break;
    }

    char sampStr[20];
    snprintf(sampStr, sizeof(sampStr), "%d samples", _captureCount);
    _display.drawCenteredText(sampStr,       SCREEN_H - 18, CLR_DIM);
    _display.drawCenteredText("SEL to exit", SCREEN_H - 8,  CLR_DIM);
}
