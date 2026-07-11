#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <nvs_flash.h>
#include "../ui/Display.h"
#include "config.h"

// ─────────────────────────────────────────
//  Evil Twin Configuration
// ─────────────────────────────────────────
#define EVIL_TWIN_MAX_TARGETS   10
#define EVIL_TWIN_CACHE_SIZE    512   // Bytes for NVS cache
#define EVIL_TWIN_MAX_CREDENTIALS   50    // Max captured credentials
#define EVIL_TWIN_MAX_CLIENTS   8     // Concurrent fake AP clients
#define EVIL_TWIN_BROADCAST_INTERVAL 100  // ms

// ─────────────────────────────────────────
//  Target AP Data Structure
// ─────────────────────────────────────────
struct TargetAPInfo {
    char     ssid[33];
    uint8_t  bssid[6];
    int8_t   rssi;
    uint8_t  channel;
    uint8_t  encryption;
    bool     hidden;
    bool     is_active;
};

// ─────────────────────────────────────────
//  Captured Credentials/Packets
// ─────────────────────────────────────────
struct CapturedCredential {
    char     ssid[33];
    uint8_t  client_mac[6];
    char     username[65];    // EAPOL/WPA captures
    char     password[65];
    uint32_t timestamp;
    bool     is_valid;
};

// ─────────────────────────────────────────
//  Evil Twin State Machine
// ─────────────────────────────────────────
enum class EvilTwinState {
    IDLE,
    SCANNING,           // Scan for target AP
    CLONING,            // Create fake AP
    BROADCASTING,       // Beacon spam as fake AP
    CAPTURING,          // Capture client connections
    CREDENTIAL_SNIFF,   // Sniff EAPOL/WPA packets
    COMPLETE
};

enum class EvilTwinMode {
    OPEN_NETWORK,       // No encryption
    WPA2_MIMIC,         // Mimic WPA2 (no real encryption, just frame structure)
    CAPTIVE_PORTAL      // Redirect all traffic to portal
};

// ─────────────────────────────────────────
//  Evil Twin Module
// ─────────────────────────────────────────
class EvilTwinModule {
public:
    EvilTwinModule(Display& display);
    ~EvilTwinModule();

    // ── Lifecycle ────────────────────────────────
    void begin();
    void tick();        // Non-blocking state machine
    void stop();

    // ── Attack Setup ─────────────────────────────
    // Step 1: Scan for real AP to clone
    void startScan();
    void stopScan();

    // Step 2: Select target from scan results
    void selectTarget(int8_t index);
    const TargetAPInfo* getSelectedTarget() const { return &_selected_target; }

    // Step 3: Start evil twin broadcast
    void startEvilTwin(EvilTwinMode mode = EvilTwinMode::OPEN_NETWORK);
    void stopEvilTwin();

    // ── Data Access ──────────────────────────────
    int                             getScanCount() const { return _scan_count; }
    const TargetAPInfo*             getScanResults() const { return _scan_results; }
    int                             getConnectedClientCount() const { return _connected_clients; }
    int                             getCapturedCredentialCount() const { return _captured_creds_count; }
    const CapturedCredential*       getCapturedCredentials() const { return _captured_credentials; }
    
    // ── State & Diagnostics ──────────────────────
    EvilTwinState                   getState() const { return _state; }
    const char*                     getStateString() const;
    uint32_t                        getFramesSent() const { return _frames_sent; }
    uint32_t                        getConnectionAttempts() const { return _connection_attempts; }
    uint32_t                        getPacketsCaptured() const { return _packets_captured; }

    // ── Configuration ─────────────────��──────────
    void setSSIDRotation(bool enabled) { _rotate_ssid = enabled; }
    void setChannel(uint8_t channel);
    void setCaptureFilter(bool capture_eapol = true, bool capture_dhcp = true);

    // ── NVS Persistence ─────────────────────────
    void saveCredentials();
    void loadCredentials();
    void clearStoredCredentials();

private:
    Display& _display;
    EvilTwinState _state = EvilTwinState::IDLE;
    EvilTwinMode  _mode = EvilTwinMode::OPEN_NETWORK;
    
    // ── Scanning State ───────────────────────────
    int          _scan_count = 0;
    TargetAPInfo _scan_results[EVIL_TWIN_MAX_TARGETS];
    TargetAPInfo _selected_target;
    
    // ── Evil Twin AP State ───────────────────────
    uint32_t     _last_beacon_ms = 0;
    uint8_t      _current_channel = 6;
    bool         _ap_running = false;
    bool         _rotate_ssid = false;
    uint8_t      _ssid_rotation_idx = 0;
    
    // ── Client Management ────────────────────────
    uint8_t      _connected_clients = 0;
    uint8_t      _fake_client_macs[EVIL_TWIN_MAX_CLIENTS][6];
    
    // ── Credential Capture ───────────────────────
    CapturedCredential _captured_credentials[EVIL_TWIN_CREDENTIALS];
    int                _captured_creds_count = 0;
    bool               _capture_eapol = true;
    bool               _capture_dhcp = true;
    
    // ── Diagnostics ──────────────────────────────
    uint32_t     _frames_sent = 0;
    uint32_t     _connection_attempts = 0;
    uint32_t     _packets_captured = 0;
    uint32_t     _state_start_ms = 0;
    
    // ── Internal Helpers ─────────────────────────
    void _buildBeaconFrame(uint8_t* frame, size_t& frame_len, const char* ssid, 
                          uint8_t channel, uint8_t encryption_type);
    void _broadcastFakeBeacon();
    void _handleClientConnection(const uint8_t* client_mac);
    void _capturePacket(const uint8_t* payload, size_t length);
    void _updateNVSCache();
    
    // ── Promiscuous callback for packet sniffing ─
    static void _promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type);
    static EvilTwinModule* _instance;

    // ── NVS namespace ───────────────────────────
    static constexpr const char* NVS_NAMESPACE = "evil_twin";
};
