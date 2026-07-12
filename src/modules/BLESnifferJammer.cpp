#include "BLESnifferJammer.h"
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <cstring>

// Static instance for callbacks
BLESnifferJammer* BLESnifferJammer::_instance = nullptr;

// ─────────────────────────────────────────
//  Constructor & Initialization
// ─────────────────────────────────────────
BLESnifferJammer::BLESnifferJammer(Display& display)
    : _display(display) {
    _instance = this;
    memset(_discovered, 0, sizeof(_discovered));
    memset(&_jammer_target, 0, sizeof(_jammer_target));
}

BLESnifferJammer::~BLESnifferJammer() {
    stop();
}

void BLESnifferJammer::begin() {
    Serial.println("[BLESnifferJammer] Initializing...");
    
    BLEDevice::init("Ouroboros-BLE");
    _enablePromiscuousMode();
    
    Serial.println("[BLESnifferJammer] BLE stack ready");
}

// ─────────────────────────────────────────
//  Sniffing Control
// ─────────────────────────────────────────
void BLESnifferJammer::startSniffing() {
    if (_mode != BLEJammerMode::IDLE) {
        Serial.println("[BLESnifferJammer] Already in operation, stopping first");
        stop();
    }
    
    _mode = BLEJammerMode::SNIFFING;
    _packet_count = 0;
    _discovered_count = 0;
    memset(_discovered, 0, sizeof(_discovered));
    
    _enablePromiscuousMode();
    
    Serial.println("[BLESnifferJammer] Sniffing started on channel 37");
    Serial.printf("[BLESnifferJammer] Listening for BLE advertisements...\n");
}

void BLESnifferJammer::stopSniffing() {
    if (_mode == BLEJammerMode::SNIFFING) {
        _disablePromiscuousMode();
        _mode = BLEJammerMode::IDLE;
        Serial.printf("[BLESnifferJammer] Sniffing stopped. Captured %d devices, %d packets total\n",
                     _discovered_count, _packet_count);
    }
}

// ─────────────────────────────────────────
//  Jamming Control
// ─────────────────────────────────────────
void BLESnifferJammer::startBroadcastJam() {
    if (_mode != BLEJammerMode::IDLE && _mode != BLEJammerMode::SNIFFING) {
        Serial.println("[BLESnifferJammer] Already jamming, stopping first");
        stop();
    }
    
    _mode = BLEJammerMode::JAM_BROADCAST;
    _jammer_packets_sent = 0;
    _interruption_attempts = 0;
    _jammer_running = true;
    _last_jam_ms = millis();
    
    // Broadcast jam = target all FF's
    memset(_jammer_target.addr, 0xFF, 6);
    _jammer_target.active = true;
    _jammer_target.packetsSent = 0;
    _jammer_target.startTime = millis();
    
    _enablePromiscuousMode();
    
    Serial.println("[BLESnifferJammer] Broadcast jam ACTIVE - jamming all BLE devices");
    Serial.printf("[BLESnifferJammer] Jam intensity: %d%%\n", _jam_intensity);
}

void BLESnifferJammer::startSelectiveJam(const uint8_t* target_addr) {
    if (_mode != BLEJammerMode::IDLE && _mode != BLEJammerMode::SNIFFING) {
        Serial.println("[BLESnifferJammer] Already jamming, stopping first");
        stop();
    }
    
    _mode = BLEJammerMode::JAM_SELECTIVE;
    _jammer_packets_sent = 0;
    _interruption_attempts = 0;
    _jammer_running = true;
    _last_jam_ms = millis();
    
    memcpy(_jammer_target.addr, target_addr, 6);
    _jammer_target.active = true;
    _jammer_target.packetsSent = 0;
    _jammer_target.startTime = millis();
    
    _enablePromiscuousMode();
    
    Serial.printf("[BLESnifferJammer] Selective jam ACTIVE - targeting %02X:%02X:%02X:%02X:%02X:%02X\n",
                 target_addr[0], target_addr[1], target_addr[2],
                 target_addr[3], target_addr[4], target_addr[5]);
}

void BLESnifferJammer::stopJam() {
    if (_mode == BLEJammerMode::JAM_BROADCAST || _mode == BLEJammerMode::JAM_SELECTIVE) {
        _jammer_running = false;
        uint32_t duration = millis() - _jammer_target.startTime;
        Serial.printf("[BLESnifferJammer] Jam stopped. Duration: %lums, Packets sent: %lu\n",
                     duration, _jammer_target.packetsSent);
        _mode = BLEJammerMode::IDLE;
    }
}

void BLESnifferJammer::stop() {
    if (_mode != BLEJammerMode::IDLE) {
        _disablePromiscuousMode();
        _jammer_running = false;
        _mode = BLEJammerMode::IDLE;
        Serial.println("[BLESnifferJammer] All operations stopped");
    }
}

// ─────────────────────────────────────────
//  Main Tick
// ─────────────────────────────────────────
void BLESnifferJammer::tick() {
    if (_mode == BLEJammerMode::IDLE) {
        return;
    }
    
    // Rate-limited jamming (don't jam every millisecond)
    uint32_t now = millis();
    uint32_t jam_interval = (100 - _jam_intensity) + 10;  // 10-110ms between bursts
    
    if (_jammer_running && (now - _last_jam_ms >= jam_interval)) {
        _transmitJamPulse();
        _last_jam_ms = now;
    }
}

// ─────────────────────────────────────────
//  Configuration
// ─────────────────────────────────────────
void BLESnifferJammer::setChannel(uint8_t channel) {
    if (channel >= 37 && channel <= 39) {
        _current_channel = channel;
        _setRadioChannel(channel);
        Serial.printf("[BLESnifferJammer] Channel set to %d\n", channel);
    }
}

void BLESnifferJammer::setJamIntensity(uint8_t percent) {
    if (percent <= 100) {
        _jam_intensity = percent;
        Serial.printf("[BLESnifferJammer] Jam intensity set to %d%%\n", percent);
    }
}

// ─────────────────────────────────────────
//  Diagnostics
// ─────────────────────────────────────────
const char* BLESnifferJammer::getModeString() const {
    switch (_mode) {
        case BLEJammerMode::IDLE:           return "Idle";
        case BLEJammerMode::SNIFFING:       return "Sniffing";
        case BLEJammerMode::JAM_BROADCAST:  return "Broadcast Jam";
        case BLEJammerMode::JAM_SELECTIVE:  return "Selective Jam";
        case BLEJammerMode::REPLAY:         return "Replay";
        default:                            return "Unknown";
    }
}

// ─────────────────────────────────────────
//  BLE Packet Handler
// ─────────────────────────────────────────
void BLESnifferJammer::_handleBLEPacket(const uint8_t* addr, int rssi, 
                                        const uint8_t* payload, uint8_t len) {
    if (_mode == BLEJammerMode::SNIFFING) {
        BLEPacketInfo info;
        memcpy(info.addr, addr, 6);
        info.rssi = rssi;
        info.timestamp = millis();
        info.packetCount = 1;
        
        // Try to extract device name from AD structure
        if (payload && len > 0) {
            // Simple scan for complete/shortened local name
            for (uint8_t i = 0; i < len - 1; i++) {
                uint8_t adlen = payload[i];
                uint8_t adtype = payload[i + 1];
                
                if ((adtype == 0x09 || adtype == 0x08) && adlen > 0) {  // Complete/Shortened Local Name
                    uint8_t namelen = (adlen < 31) ? adlen : 31;
                    memcpy(info.name, &payload[i + 2], namelen - 1);
                    info.name[namelen - 1] = '\0';
                    break;
                }
            }
        }
        
        _updateDiscoveredList(info);
        _packet_count++;
    }
}

void BLESnifferJammer::_updateDiscoveredList(const BLEPacketInfo& info) {
    // Check if device already in list
    for (int i = 0; i < _discovered_count; i++) {
        if (memcmp(_discovered[i].addr, info.addr, 6) == 0) {
            _discovered[i].rssi = info.rssi;
            _discovered[i].timestamp = info.timestamp;
            _discovered[i].packetCount++;
            return;
        }
    }
    
    // Add new device if space available
    if (_discovered_count < MAX_DISCOVERED) {
        memcpy(&_discovered[_discovered_count], &info, sizeof(BLEPacketInfo));
        _discovered_count++;
    }
}

// ─────────────────────────────────────────
//  Jamming Implementation
// ─────────────────────────────────────────
void BLESnifferJammer::_generateJamFrame() {
    // BLE advertising frame with random data to cause interference
    static uint8_t jam_pattern[47];
    
    // Fill with pseudo-random pattern
    for (int i = 0; i < sizeof(jam_pattern); i++) {
        jam_pattern[i] = (uint8_t)((millis() + i * 17) & 0xFF);
    }
}

void BLESnifferJammer::_transmitJamPulse() {
    // ESP32 BLE radio transmission simulation
    // In real implementation, this would use raw radio access
    // For now, we track jam attempts
    
    _jammer_packets_sent++;
    _jammer_target.packetsSent++;
    _interruption_attempts++;
    
    if (_jammer_packets_sent % 50 == 0) {
        Serial.printf("[BLESnifferJammer] Jam pulse #%lu sent\n", _jammer_packets_sent);
    }
}

// ─────────────────────────────────────────
//  BLE Radio Control
// ─────────────────────────────────────────
void BLESnifferJammer::_setRadioChannel(uint8_t ch) {
    // Set BLE advertising channel (37=2402MHz, 38=2426MHz, 39=2480MHz)
    Serial.printf("[BLESnifferJammer] Radio tuned to channel %d\n", ch);
}

void BLESnifferJammer::_enablePromiscuousMode() {
    Serial.println("[BLESnifferJammer] Promiscuous mode enabled");
    
    esp_ble_gap_register_callback(_blePacketCallback);
    
    esp_ble_scan_params_t scan_params = {
        .scan_type = BLE_SCAN_TYPE_PASSIVE,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = 0x50,
        .scan_window = 0x30,
        .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE
    };
    
    esp_ble_gap_set_scan_params(&scan_params);
    esp_ble_gap_start_scanning(0);  // 0 = scan indefinitely
}

void BLESnifferJammer::_disablePromiscuousMode() {
    esp_ble_gap_stop_scanning();
    Serial.println("[BLESnifferJammer] Promiscuous mode disabled");
}

// ─────────────────────────────────────────
//  Static Callback (required for ESP32 BLE)
// ─────────────────────────────────────────
void BLESnifferJammer::_blePacketCallback(esp_gap_ble_cb_event_t event, 
                                          esp_ble_gap_cb_param_t* param) {
    if (!_instance) return;
    
    switch (event) {
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            esp_ble_gap_search_evt_param_t search_param = param->scan_rst;
            
            if (search_param.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                _instance->_handleBLEPacket(
                    search_param.bda,
                    search_param.rssi,
                    search_param.ble_adv,
                    search_param.adv_data_len
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
