#include "SubGHz.h"

SubGHz::SubGHz(Display& display) : _display(display) {}

bool SubGHz::begin() {
    ELECHOUSE_cc1101.setSpiPin(PIN_CC1101_SCK, PIN_CC1101_MISO,
                               PIN_CC1101_MOSI, PIN_CC1101_CS);
    if (!ELECHOUSE_cc1101.getCC1101()) {
        Serial.println("[SubGHz] CC1101 not detected!");
        return false;
    }
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setMHZ(_frequency);
    ELECHOUSE_cc1101.setModulation(_modulation);
    Serial.printf("[SubGHz] CC1101 ready @ %.2f MHz\n", _frequency);
    return true;
}

// ─────────────────────────────────────────
//  Scan
// ─────────────────────────────────────────

void SubGHz::startScan() {
    _scanning = true;
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setMHZ(_frequency);
    ELECHOUSE_cc1101.SetRx();
    Serial.println("[SubGHz] Scan started");
}

void SubGHz::stopScan() {
    _scanning = false;
    ELECHOUSE_cc1101.setSidle();
    Serial.println("[SubGHz] Scan stopped");
}

void SubGHz::tickScan() {
    if (!_scanning) return;

    int rssi = ELECHOUSE_cc1101.getRssi();

    // Only bother drawing if signal is above noise floor
    if (rssi > -80) {
        _drawScanResult(_frequency, rssi);
        Serial.printf("[SubGHz] RSSI: %d dBm @ %.2f MHz\n", rssi, _frequency);
    }

    delay(100);
}

// ─────────────────────────────────────────
//  Capture
// ─────────────────────────────────────────

bool SubGHz::startCapture() {
    _capturing    = true;
    _captureReady = false;
    memset(&_lastCapture, 0, sizeof(_lastCapture));

    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setMHZ(_frequency);
    ELECHOUSE_cc1101.setModulation(_modulation);
    ELECHOUSE_cc1101.SetRx();

    _drawCaptureStatus(false);
    Serial.println("[SubGHz] Capture ready — waiting for signal...");
    return true;
}

void SubGHz::stopCapture() {
    _capturing = false;
    ELECHOUSE_cc1101.setSidle();
    Serial.println("[SubGHz] Capture stopped");
}

void SubGHz::tickCapture() {
    if (!_capturing) return;

    if (ELECHOUSE_cc1101.CheckReceiveFlag()) {
        uint8_t buf[64];
        int len = ELECHOUSE_cc1101.ReceiveData(buf);

        if (len > 0) {
            _lastCapture.frequency = _frequency;
            _lastCapture.rssi      = ELECHOUSE_cc1101.getRssi();
            _lastCapture.length    = min((int)sizeof(_lastCapture.data), len);
            memcpy(_lastCapture.data, buf, _lastCapture.length);

            _captureReady = true;
            _capturing    = false;

            _drawCaptureStatus(true);

            Serial.printf("[SubGHz] Captured %d bytes, RSSI: %d dBm\n",
                          _lastCapture.length, _lastCapture.rssi);
            for (int i = 0; i < _lastCapture.length; i++)
                Serial.printf("%02X ", _lastCapture.data[i]);
            Serial.println();

            ELECHOUSE_cc1101.setSidle();
        }
    }
}

// ─────────────────────────────────────────
//  Replay
// ─────────────────────────────────────────

bool SubGHz::replay() {
    if (!_captureReady) {
        Serial.println("[SubGHz] No capture to replay");
        return false;
    }

    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setMHZ(_lastCapture.frequency);
    ELECHOUSE_cc1101.setModulation(_modulation);
    ELECHOUSE_cc1101.SetTx();

    ELECHOUSE_cc1101.SendData(_lastCapture.data, _lastCapture.length);

    Serial.printf("[SubGHz] Replayed %d bytes @ %.2f MHz\n",
                  _lastCapture.length, _lastCapture.frequency);

    ELECHOUSE_cc1101.setSidle();
    return true;
}

// ─────────────────────────────────────────
//  Config
// ─────────────────────────────────────────

void SubGHz::setFrequency(float mhz) {
    _frequency = mhz;
    ELECHOUSE_cc1101.setMHZ(mhz);
    Serial.printf("[SubGHz] Frequency set to %.2f MHz\n", mhz);
}

void SubGHz::setModulation(uint8_t mod) {
    _modulation = mod;
    ELECHOUSE_cc1101.setModulation(mod);
}

void SubGHz::setBandwidth(uint8_t bw) {
    _bandwidth = bw;
    // Note: setBandWidth not available in SmartRC-CC1101-Driver-Lib v3.x
    // Bandwidth is controlled via CC1101 register directly if needed
    Serial.printf("[SubGHz] Bandwidth set to index %d (register write not implemented yet)\n", bw);
}

// ─────────────────────────────────────────
//  Display helpers
// ─────────────────────────────────────────

void SubGHz::_drawScanResult(float freq, int rssi) {
    // Simple RSSI bar graph — drawn below status bar
    int16_t barY  = STATUSBAR_H + 20;
    int16_t barMaxW = SCREEN_W - 20;

    // Map rssi (-100 to -30) to bar width
    int16_t barW = map(constrain(rssi, -100, -30), -100, -30, 0, barMaxW);

    char freqStr[20];
    snprintf(freqStr, sizeof(freqStr), "%.2f MHz", freq);
    _display.drawText(freqStr, 2, STATUSBAR_H + 4, 1, CLR_TEXT);

    char rssiStr[16];
    snprintf(rssiStr, sizeof(rssiStr), "%d dBm", rssi);
    _display.drawText(rssiStr, 2, barY - 2, 1, CLR_DIM);

    _display.fillRect(0,    barY, barMaxW, 8, CLR_HIGHLIGHT);
    _display.fillRect(0,    barY, barW,    8, CLR_ACCENT);
}

void SubGHz::_drawCaptureStatus(bool received) {
    int16_t y = STATUSBAR_H + 20;
    _display.fillRect(0, y, SCREEN_W, 60, CLR_BG);

    if (!received) {
        _display.drawCenteredText("Listening...", y + 10, CLR_DIM);

        char freqStr[24];
        snprintf(freqStr, sizeof(freqStr), "%.2f MHz", _frequency);
        _display.drawCenteredText(freqStr, y + 26, CLR_ACCENT);
    } else {
        _display.drawCenteredText("Captured!", y + 10, CLR_ACCENT);

        char lenStr[20];
        snprintf(lenStr, sizeof(lenStr), "%d bytes", _lastCapture.length);
        _display.drawCenteredText(lenStr, y + 26, CLR_TEXT);

        char rssiStr[16];
        snprintf(rssiStr, sizeof(rssiStr), "RSSI %d dBm", _lastCapture.rssi);
        _display.drawCenteredText(rssiStr, y + 42, CLR_DIM);
    }
}
