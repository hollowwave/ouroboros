#include "BLESnifferJammer.h"
#include <esp_gap_ble_api.h>

// ─────────────────────────────────────────
//  Static Members
// ─────────────────────────────────────────
BLESnifferJammer* BLESnifferJammer::_instance = nullptr;

// ─────────────────────────────────────────
//  Constructor & Lifecycle
// ─────────────────────────────────────────
BLESnifferJammer::BLESnifferJammer(Display& display)
    : _display(display) {
    _instance = this;
    memset(_discovered, 0, sizeof(_discovered));
}

BLESnifferJammer::~BLESnifferJammer() {
    stop();
}

void BLESnifferJammer::begin() {
    Serial.println("[BLESnifferJammer] Initializing...");
    _mode = BLEJammerMode::IDLE;
    _last_sniff_ms = millis();
    
    // Initialize BLE device
    BLEDevice::init("");
    
    Serial.println("[BLESnifferJammer] Ready");
}

void BLESnifferJammer::tick() {
    if (_mode == BLEJammerMode::IDLE) {
        return;
    }

    uint32_t elapsed = millis() - _last_sniff_ms;

    // ── State machine logic ──────────────────────
    switch (_mode) {
        case BLEJammerMode::SNIFFING:
            // Passive scanning handled by BLE stack callbacks
            break;

        case BLEJammerMode::JAM_BROADCAST:
        case BLEJammerMode::JAM_SELECTIVE:
            // Jamming handled by background tasks
            if (elapsed > 100) {
                _transmitJamPulse();
                _last_sniff_ms = millis();
            }
            break;

        case BLEJammerMode::REPLAY:
            // Replay captured packets
            break;

        default:
            break;
    }
}

void BLESnifferJammer::stop() {
    stopSniffing();
    stopJam();
    _mode = BLEJammerMode::IDLE;
    BLEDevice::deinit(false);
    Serial.println("[BLESnifferJammer] Stopped");
}

// ─────────────────────────────────────────
//  Sniffing Control
// ─────────────────────────────────────────
void BLESnifferJammer::startSniffing() {
    Serial.println("[BLESnifferJammer] Starting BLE scan...");
    _mode = BLEJammerMode::SNIFFING;
    _discovered_count = 0;
    _packet_count = 0;
    _last_sniff_ms = millis();

    // Enable promiscuous mode for raw packet capture
    esp_ble_gap_register_callback(_blePacketCallback);
    
    // Start BLE scan
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(nullptr, false);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(100);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(0, nullptr, false);

    Serial.println("[BLESnifferJammer] BLE scan started");
}

void BLESnifferJammer::stopSniffing() {
    if (_mode == BLEJammerMode::SNIFFING) {
        BLEScan* pBLEScan = BLEDevice::getScan();
        pBLEScan->stop();
        _mode = BLEJammerMode::IDLE;
        Serial.printf("[BLESnifferJammer] Scan stopped. Devices found: %d, Packets: %lu\n",
                     _discovered_count, _packet_count);
    }
}

// ─────────────────────────────────────────
//  Jamming Control
// ─────────────────────────────────────────
void BLESnifferJammer::startBroadcastJam() {
    Serial.println("[BLESnifferJammer] Starting broadcast jam...");
    _mode = BLEJammerMode::JAM_BROADCAST;
    _jammer_running = true;
    _jammer_packets_sent = 0;
    _last_jam_ms = millis();
}

void BLESnifferJammer::startSelectiveJam(const uint8_t* target_addr) {
    if (!target_addr) return;
    
    Serial.printf("[BLESnifferJammer] Starting selective jam on %02X:%02X:%02X:%02X:%02X:%02X...\n",
                 target_addr[0], target_addr[1], target_addr[2],
                 target_addr[3], target_addr[4], target_addr[5]);
    
    _mode = BLEJammerMode::JAM_SELECTIVE;
    memcpy(_jammer_target.addr, target_addr, 6);
    _jammer_target.active = true;
    _jammer_running = true;
    _jammer_packets_sent = 0;
    _last_jam_ms = millis();
}

void BLESnifferJammer::stopJam() {
    if (_jammer_running) {
        _jammer_running = false;
        _jammer_target.active = false;
        _mode = BLEJammerMode::IDLE;
        Serial.printf("[BLESnifferJammer] Jammer stopped. Packets sent: %lu\n",
                     _jammer_packets_sent);
    }
}

// ─────────────────────────────────────────
//  Packet Handling & Discovery
// ─────────────────────────────────────────
void BLESnifferJammer::_handleBLEPacket(const uint8_t* addr, int rssi, 
                                        const uint8_t* payload, uint8_t len) {
    _packet_count++;
    
    BLEPacketInfo info;
    memcpy(info.addr, addr, 6);
    info.rssi = rssi;
    info.timestamp = millis();
    info.flags = 0;
    info.connectable = false;
    info.scannable = false;
    memset(info.name, 0, sizeof(info.name));
    
    _updateDiscoveredList(info);
}

void BLESnifferJammer::_updateDiscoveredList(const BLEPacketInfo& info) {
    // Check if device already exists
    for (int i = 0; i < _discovered_count; i++) {
        if (memcmp(_discovered[i].addr, info.addr, 6) == 0) {
            _discovered[i].packetCount++;
            _discovered[i].rssi = info.rssi;  // Update with latest RSSI
            _discovered[i].timestamp = millis();
            return;
        }
    }

    // Add new device if space available
    if (_discovered_count < MAX_DISCOVERED) {
        _discovered[_discovered_count] = info;
        _discovered[_discovered_count].packetCount = 1;
        _discovered_count++;
    }
}

// ─────────────────────────────────────────
//  Jamming Transmission
// ─────────────────────────────────────────
void BLESnifferJammer::_generateJamFrame() {
    // Generate interference pattern
    // (Simplified — actual jamming would generate specific BLE packets)
}

void BLESnifferJammer::_transmitJamPulse() {
    if (!_jammer_running) return;
    
    _jammer_packets_sent++;
    _interruption_attempts++;
    
    // In a real implementation, transmit jam frames here
    // For now, just count attempts
}

// ─────────────────────────────────────────
//  Radio Control
// ─────────────────────────────────────────
void BLESnifferJammer::_setRadioChannel(uint8_t ch) {
    _current_channel = ch;
    // BLE advertising channels: 37, 38, 39
    if (ch < 37 || ch > 39) {
        ch = 37;  // Default to channel 37
    }
}

void BLESnifferJammer::_enablePromiscuousMode() {
    // Promiscuous mode enabled via BLE stack callback registration
}

void BLESnifferJammer::_disablePromiscuousMode() {
    // Disable promiscuous mode
}

// ─────────────────────────────────────────
//  Configuration
// ─────────────────────────────────────────
void BLESnifferJammer::setChannel(uint8_t channel) {
    _setRadioChannel(channel);
}

void BLESnifferJammer::setJamIntensity(uint8_t percent) {
    _jam_intensity = constrain(percent, 0, 100);
}

// ─────────────────────────────────────────
//  Diagnostics
// ─────────────────────────────────────────
const char* BLESnifferJammer::getModeString() const {
    switch (_mode) {
        case BLEJammerMode::IDLE:          return "IDLE";
        case BLEJammerMode::SNIFFING:      return "SNIFFING";
        case BLEJammerMode::JAM_BROADCAST: return "JAM_BROADCAST";
        case BLEJammerMode::JAM_SELECTIVE: return "JAM_SELECTIVE";
        case BLEJammerMode::REPLAY:        return "REPLAY";
        default:                           return "UNKNOWN";
    }
}

// ─────────────────────────────────────────
//  Static Callback (required for ESP32 BLE)
// ─────────────────────────────────────────
void BLESnifferJammer::_blePacketCallback(esp_gap_ble_cb_event_t event, 
                                          esp_ble_gap_cb_param_t* param) {
    if (!_instance) return;
    
    switch (event) {
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            // Access scan result directly from param
            if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                _instance->_handleBLEPacket(
                    param->scan_rst.bda,
                    param->scan_rst.rssi,
                    param->scan_rst.ble_adv,
                    param->scan_rst.adv_data_len
                );
            }
            break;
        }
        
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            Serial.println("[BLESnifferJammer] BLE scan stopped");
            break;
            
        default:
            break;
    }
}
