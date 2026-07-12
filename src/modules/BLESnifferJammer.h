#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include "../ui/Display.h"

// ─────────────────────────────────────────
//  BLE Packet Info Structure
// ─────────────────────────────────────────
struct BLEPacketInfo {
    uint8_t   addr[6];           // MAC address
    char      name[32];          // Device name (if available)
    int8_t    rssi;              // Signal strength
    uint16_t  iBeaconUUID[2];    // iBeacon UUID (if detected)
    uint8_t   flags;             // BLE flags
    bool      connectable;       // Is connectable?
    bool      scannable;         // Is scannable?
    uint32_t  timestamp;         // When captured
    uint16_t  packetCount;       // Total packets from this device
};

// ─────────────────────────────────────────
//  Jammer Target Configuration
// ─────────────────────────────────────────
struct JammerTarget {
    uint8_t   addr[6];           // MAC to jam (all FF's = broadcast jam)
    bool      active;
    uint32_t  packetsSent;
    uint32_t  startTime;
};

// ─────────────────────────────────────────
//  Jammer Modes
// ─────────────────────────────────────────
enum class BLEJammerMode {
    IDLE,
    SNIFFING,           // Passive packet capture
    JAM_BROADCAST,      // Jam all BLE traffic
    JAM_SELECTIVE,      // Jam specific device
    REPLAY              // Replay captured packets (future)
};

// ─────────────────────────────────────────
//  BLE Sniffer & Jammer Module
// ─────────────────────────────────────────
class BLESnifferJammer {
public:
    BLESnifferJammer(Display& display);
    ~BLESnifferJammer();

    // ── Lifecycle ────────────────────────────────
    void begin();
    void tick();
    void stop();

    // ── Sniffing ─────────────────────────────────
    void startSniffing();
    void stopSniffing();

    // ── Jamming ──────────────────────────────────
    void startBroadcastJam();      // Jam all BLE devices
    void startSelectiveJam(const uint8_t* target_addr);  // Jam specific MAC
    void stopJam();

    // ── Data Access ──────────────────────────────
    int                         getDiscoveredCount() const { return _discovered_count; }
    const BLEPacketInfo*        getDiscoveredDevices() const { return _discovered; }
    int                         getCapturedPacketCount() const { return _packet_count; }
    
    // ── State & Diagnostics ──────────────────────
    BLEJammerMode               getMode() const { return _mode; }
    const char*                 getModeString() const;
    uint32_t                    getPacketsSent() const { return _jammer_packets_sent; }
    uint32_t                    getInterruptionAttempts() const { return _interruption_attempts; }

    // ── Configuration ────────────────────────────
    void setChannel(uint8_t channel);  // 37, 38, 39 (BLE advertising channels)
    void setJamIntensity(uint8_t percent);  // 0-100% jam strength

private:
    Display&            _display;
    BLEJammerMode       _mode = BLEJammerMode::IDLE;
    
    // ── Sniffing State ───────────────────────────
    static const uint8_t MAX_DISCOVERED = 50;
    BLEPacketInfo       _discovered[MAX_DISCOVERED];
    int                 _discovered_count = 0;
    uint32_t            _packet_count = 0;
    uint32_t            _last_sniff_ms = 0;

    // ── Jamming State ────────────────────────────
    JammerTarget        _jammer_target;
    uint8_t             _current_channel = 37;      // BLE adv channel
    uint8_t             _jam_intensity = 100;       // 0-100%
    uint32_t            _jammer_packets_sent = 0;
    uint32_t            _interruption_attempts = 0;
    uint32_t            _last_jam_ms = 0;
    bool                _jammer_running = false;

    // ── Internal Helpers ─────────────────────────
    void _handleBLEPacket(const uint8_t* addr, int rssi, const uint8_t* payload, uint8_t len);
    void _generateJamFrame();      // Generate interference pattern
    void _transmitJamPulse();      // Send jam packet
    void _updateDiscoveredList(const BLEPacketInfo& info);
    
    // ── BLE Radio Control ────────────────────────
    void _setRadioChannel(uint8_t ch);
    void _enablePromiscuousMode();
    void _disablePromiscuousMode();

    // ── Static callbacks for BLE stack ───────────
    static void _blePacketCallback(esp_ble_gap_cb_event_t event, esp_ble_gap_cb_param_t* param);
    static BLESnifferJammer* _instance;
};
