#pragma once
#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include "../ui/Display.h"
#include "config.h"

struct SignalCapture {
    float    frequency;
    uint8_t  rssi;
    uint8_t  data[64];
    uint8_t  length;
};

class SubGHz {
public:
    SubGHz(Display& display);

    bool begin();             // Returns false if CC1101 not detected

    // ── Modes ────────────────────────────────────
    void startScan();
    void stopScan();
    void tickScan();          // Call every loop() while scanning

    bool startCapture();
    void stopCapture();
    void tickCapture();

    bool replay();            // Replay last captured signal

    // ── Config ───────────────────────────────────
    void setFrequency(float mhz);
    void setModulation(uint8_t mod);   // 0=2FSK, 1=GFSK, 2=ASK/OOK, 3=4FSK, 4=MSK
    void setBandwidth(uint8_t bw);

    float    getFrequency()   const { return _frequency; }
    bool     hasCapture()     const { return _captureReady; }

private:
    Display&       _display;
    float          _frequency   = 433.92f;
    uint8_t        _modulation  = 2;       // ASK/OOK default — most common for remotes
    uint8_t        _bandwidth   = 0;
    bool           _scanning    = false;
    bool           _capturing   = false;
    bool           _captureReady = false;
    SignalCapture  _lastCapture;

    void _drawScanResult(float freq, int rssi);
    void _drawCaptureStatus(bool receiving);
};
