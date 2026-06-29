#include "WiFiMapper.h"

WiFiMapper::WiFiMapper(Display& display, Buttons& buttons)
    : _display(display), _buttons(buttons) {}

void WiFiMapper::enter() {
    _done         = false;
    _needsRedraw  = true;
    _view         = MapperView::AP_LIST;
    _cursor       = 0;
    _scrollOffset = 0;
    _apCount      = 0;
    _lastScan     = 0;
    memset(_aps, 0, sizeof(_aps));
    memset(_channelRssi, 0, sizeof(_channelRssi));
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    Serial.println("[Mapper] Started");
}

void WiFiMapper::stop() {
    WiFi.mode(WIFI_OFF);
    _done = false;
}

void WiFiMapper::tick() {
    if (_done) return;

    BtnEvent e = _buttons.consume();

    switch (e) {
        case BtnEvent::SELECT:
            // Toggle view
            _view = (_view == MapperView::AP_LIST)
                    ? MapperView::CHANNEL_GRAPH
                    : MapperView::AP_LIST;
            _needsRedraw = true;
            break;
        case BtnEvent::UP:
            if (_view == MapperView::AP_LIST && _cursor > 0) {
                _cursor--;
                if (_cursor < _scrollOffset) _scrollOffset--;
                _needsRedraw = true;
            }
            break;
        case BtnEvent::DOWN:
            if (_view == MapperView::AP_LIST && _cursor < _apCount - 1) {
                _cursor++;
                if (_cursor >= _scrollOffset + MENU_MAX_ITEMS) _scrollOffset++;
                _needsRedraw = true;
            }
            break;
        case BtnEvent::BACK:
        case BtnEvent::BACK_LONG:
            stop();
            _done = true;
            return;
        default: break;
    }

    // Auto rescan
    if (millis() - _lastScan >= _scanInterval) {
        _doScan();
        _needsRedraw = true;
    }

    if (_needsRedraw) {
        switch (_view) {
            case MapperView::AP_LIST:      _drawAPList();      break;
            case MapperView::CHANNEL_GRAPH: _drawChannelGraph(); break;
        }
        _needsRedraw = false;
    }
}

// ─────────────────────────────────────────
//  Scan
// ─────────────────────────────────────────

void WiFiMapper::_doScan() {
    _scanning = true;
    _drawScanning();

    int n = WiFi.scanNetworks(false, true);
    _lastScan = millis();

    // Reset channel RSSI
    memset(_channelRssi, 0, sizeof(_channelRssi));

    for (int i = 0; i < n; i++) {
        uint8_t ch   = WiFi.channel(i);
        int8_t  rssi = (int8_t)WiFi.RSSI(i);

        _updateAP(WiFi.SSID(i).c_str(), WiFi.BSSID(i), ch, rssi);

        // Track strongest signal per channel
        if (ch >= 1 && ch <= 13) {
            if (rssi > _channelRssi[ch]) _channelRssi[ch] = rssi;
        }

        Serial.printf("[Mapper] %s CH%d %ddBm\n",
            WiFi.SSID(i).c_str(), ch, rssi);
    }

    WiFi.scanDelete();
    _scanning = false;
}

void WiFiMapper::_updateAP(const char* ssid, const uint8_t* bssid,
                            uint8_t channel, int8_t rssi) {
    // Find existing entry by BSSID
    for (int i = 0; i < _apCount; i++) {
        if (memcmp(_aps[i].bssid, bssid, 6) == 0) {
            // Update history
            _aps[i].rssiHistory[_aps[i].historyIdx] = rssi;
            _aps[i].historyIdx = (_aps[i].historyIdx + 1) % MAPPER_HISTORY;
            _aps[i].rssiLast   = rssi;
            if (rssi < _aps[i].rssiMin) _aps[i].rssiMin = rssi;
            if (rssi > _aps[i].rssiMax) _aps[i].rssiMax = rssi;
            return;
        }
    }

    // New AP
    if (_apCount >= MAPPER_MAX_APS) return;
    MapperAP& ap = _aps[_apCount++];
    strncpy(ap.ssid, ssid, 32);
    ap.ssid[32] = '\0';
    memcpy(ap.bssid, bssid, 6);
    ap.channel    = channel;
    ap.rssiLast   = rssi;
    ap.rssiMin    = rssi;
    ap.rssiMax    = rssi;
    ap.historyIdx = 0;
    ap.used       = true;
    memset(ap.rssiHistory, rssi, sizeof(ap.rssiHistory));
}

// ─────────────────────────────────────────
//  Display
// ─────────────────────────────────────────

void WiFiMapper::_drawScanning() {
    _display.drawStatusBar("WiFi Mapper");
    _display.fillRect(0, STATUSBAR_H + 1, SCREEN_W, 20, CLR_BG);
    _display.drawCenteredText("Scanning...", STATUSBAR_H + 6, CLR_DIM);
}

void WiFiMapper::_drawAPList() {
    _display.clear();
    _display.drawStatusBar("WiFi Mapper");

    if (_apCount == 0) {
        _display.drawCenteredText("No APs yet", SCREEN_H / 2, CLR_DIM);
        _drawHintBar();
        return;
    }

    for (int8_t i = 0; i < MENU_MAX_ITEMS - 1 && (i + _scrollOffset) < _apCount; i++) {
        int8_t  idx = i + _scrollOffset;
        MapperAP& ap = _aps[idx];
        bool selected = (idx == _cursor);
        int16_t y = STATUSBAR_H + 2 + (i * MENU_ITEM_H);

        _display.fillRect(0, y, SCREEN_W, MENU_ITEM_H,
                          selected ? CLR_SUBTLE : CLR_BG);

        // SSID
        char label[14];
        snprintf(label, sizeof(label), "%.12s", ap.ssid[0] ? ap.ssid : "[?]");
        _display.drawText(label, 2, y + 2, 1, selected ? CLR_ACCENT : CLR_TEXT);

        // RSSI with color
        char rssiStr[8];
        snprintf(rssiStr, sizeof(rssiStr), "%d", ap.rssiLast);
        _display.drawTextRight(rssiStr, y + 2, _rssiColor(ap.rssiLast));

        // Mini sparkline — 12px wide, 5px tall under the label
        int16_t sparkX = SCREEN_W - 28;
        int16_t sparkY = y + 8;
        for (int s = 0; s < 8; s++) {
            int8_t sample = ap.rssiHistory[
                (ap.historyIdx + s + MAPPER_HISTORY - 8) % MAPPER_HISTORY];
            int16_t h = map(constrain(sample, -100, -30), -100, -30, 1, 5);
            _display.fillRect(sparkX + s * 3, sparkY + (5 - h), 2, h, CLR_DIM);
        }
    }

    _display.drawScrollIndicator(_cursor, _apCount);
    _drawHintBar();
}

void WiFiMapper::_drawChannelGraph() {
    _display.clear();
    _display.drawStatusBar("Channel Map");

    int16_t graphY  = STATUSBAR_H + 4;
    int16_t graphH  = SCREEN_H - graphY - 22;
    int16_t barW    = (SCREEN_W - 4) / 13;

    for (int ch = 1; ch <= 13; ch++) {
        int8_t  rssi = _channelRssi[ch];
        int16_t barH = rssi < 0
            ? (int16_t)map(constrain(rssi, -100, -30), -100, -30, 2, graphH)
            : 0;

        int16_t x = 2 + (ch - 1) * barW;
        int16_t y = graphY + graphH - barH;

        // Background
        _display.fillRect(x, graphY, barW - 1, graphH, CLR_SUBTLE);
        // Bar
        if (barH > 0)
            _display.fillRect(x, y, barW - 1, barH, _rssiColor(rssi));

        // Channel label
        char chStr[4];
        snprintf(chStr, sizeof(chStr), "%d", ch);
        _display.drawText(chStr, x + (ch < 10 ? 1 : 0),
                          graphY + graphH + 2, 1, CLR_DIM);
    }

    _drawHintBar();
}

void WiFiMapper::_drawHintBar() {
    int16_t y = SCREEN_H - 10;
    _display.fillRect(0, y - 1, SCREEN_W, 11, CLR_SUBTLE);
    const char* hint = (_view == MapperView::AP_LIST)
        ? "SEL:channels  BCK:exit"
        : "SEL:list  BCK:exit";
    _display.drawText(hint, 2, y + 1, 1, CLR_DIM);
}

// ─────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────

int8_t WiFiMapper::_avgRssi(const MapperAP& ap) {
    int sum = 0;
    for (int i = 0; i < MAPPER_HISTORY; i++) sum += ap.rssiHistory[i];
    return (int8_t)(sum / MAPPER_HISTORY);
}

uint16_t WiFiMapper::_rssiColor(int8_t rssi) {
    if (rssi >= -60) return CLR_ACCENT;   // Strong — green
    if (rssi >= -75) return CLR_WARN;     // Medium — yellow
    return CLR_DANGER;                     // Weak — red
}
