#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "../ui/Display.h"
#include "config.h"

struct APInfo {
    char     ssid[33];
    uint8_t  bssid[6];
    int32_t  rssi;
    uint8_t  channel;
    uint8_t  encType;
};

class WiFiAttack {
public:
    WiFiAttack(Display& display);

    // ── Scanner ──────────────────────────────────
    void startScan();
    void tickScan();
    void stopScan();

    // ── Deauth ───────────────────────────────────
    // target = -1 → broadcast deauth all found APs
    void startDeauth(int8_t targetIndex = -1);
    void tickDeauth();
    void stopDeauth();

    // ── Beacon Spam ──────────────────────────────
    void startBeaconSpam();
    void tickBeaconSpam();
    void stopBeaconSpam();

    // ── Probe Sniff ──────────────────────────────
    void startProbeSniff();
    void tickProbeSniff();
    void stopProbeSniff();

    // ── State ────────────────────────────────────
    int          apCount()   const { return _apCount; }
    const APInfo* apList()   const { return _aps; }
    bool         isRunning() const { return _running; }

private:
    Display& _display;
    bool     _running   = false;
    int      _apCount   = 0;
    int8_t   _target    = -1;
    uint32_t _lastTick  = 0;
    uint8_t  _channel   = 1;

    static const int MAX_APS = 20;
    APInfo _aps[MAX_APS];

    // ── Raw frame helpers ────────────────────────
    void _sendDeauthFrame(const uint8_t* bssid, const uint8_t* client, uint8_t channel);
    void _sendBeaconFrame(const char* ssid, uint8_t channel);

    // ── Display helpers ──────────────────────────
    void _drawScanResults();
    void _drawAttackStatus(const char* mode, int count);
    void _drawProbeEntry(const char* ssid, const uint8_t* mac);

    // ── Promiscuous callback ─────────────────────
    static void _probeCallback(void* buf, wifi_promiscuous_pkt_type_t type);
    static WiFiAttack* _instance;
};
