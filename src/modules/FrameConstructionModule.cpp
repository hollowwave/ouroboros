#include "FrameConstructionModule.h"
#include <stdio.h>

// ─────────────────────────────────────────
//  Static Members
// ─────────────────────────────────────────
uint16_t FrameConstructionModule::_sequence_counter = 0;

// ─────────────────────────────────────────
//  Constructor & Lifecycle
// ─────────────────────────────────────────
FrameConstructionModule::FrameConstructionModule(Display& display)
    : _display(display) {
    memset(&_buffer, 0, sizeof(_buffer));
}

FrameConstructionModule::~FrameConstructionModule() {
    stop();
}

void FrameConstructionModule::begin() {
    Serial.println("[FrameConstruct] Initializing...");
    _state = FrameState::IDLE;
    _state_start_ms = millis();
    
    // Initialize Wi-Fi in STA mode for raw frame TX
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    // Enable raw 802.11 frame transmission
    esp_wifi_set_promiscuous(false);
    
    Serial.println("[FrameConstruct] Ready");
}

void FrameConstructionModule::tick() {
    if (_state == FrameState::IDLE || _state == FrameState::ERROR) {
        return;
    }

    uint32_t elapsed = millis() - _state_start_ms;

    // ── State machine transitions ────────────────
    switch (_state) {
        case FrameState::BUILDING:
            // Automatic transition to complete after frame built
            // (handled in builder methods)
            break;

        case FrameState::TRANSMITTING:
            // Check if retry interval has elapsed
            if (elapsed >= _retry_interval_ms && _current_retry < _max_retries) {
                _current_retry++;
                if (transmitFrame(_buffer.channel) == ESP_OK) {
                    _state_start_ms = millis();
                } else {
                    _state = FrameState::ERROR;
                    _error_count++;
                }
            }
            // Timeout after max retries
            if (_current_retry >= _max_retries) {
                _state = FrameState::COMPLETE;
                _state_start_ms = millis();
            }
            break;

        case FrameState::COMPLETE:
            // Hold for 100ms then return to idle
            if (elapsed > 100) {
                _state = FrameState::IDLE;
            }
            break;

        default:
            break;
    }
}

// ─────────────────────────────────────────
//  MAC Header Initialization
// ─────────────────────────────────────────
void FrameConstructionModule::_initMACHeader(
    MACHeader& hdr,
    FrameType type,
    uint8_t subtype,
    const uint8_t* dst,
    const uint8_t* src,
    const uint8_t* bssid) {

    memset(&hdr, 0, sizeof(hdr));

    // Frame Control: 2 bytes
    // Bits 0-1: Protocol version (00)
    // Bits 2-3: Type (management=0, control=1, data=2)
    // Bits 4-7: Subtype
    // Bit 8: To DS
    // Bit 9: From DS
    // Bit 10: More Frag
    // Bit 11: Retry
    // Bit 12: Power Mgmt
    // Bit 13: More Data
    // Bit 14: Protected Frame
    // Bit 15: Order
    
    uint16_t fc = 0;
    fc |= (static_cast<uint16_t>(type) & 0x03) << 2;
    fc |= (static_cast<uint16_t>(subtype) & 0x0F) << 4;
    hdr.frame_control = fc;

    // Duration/ID: 2 bytes (typically 0x3A01 for 314 µs)
    hdr.duration_id = 0x3A01;

    // MAC Addresses
    memcpy(hdr.addr1, dst, 6);
    memcpy(hdr.addr2, src, 6);
    memcpy(hdr.addr3, bssid, 6);

    // Sequence Control: High 12 bits = sequence number, low 4 bits = fragment number
    hdr.sequence_control = (_next_sequence_number() << 4) | 0;
}

uint16_t FrameConstructionModule::_next_sequence_number() {
    _sequence_counter = (_sequence_counter + 1) & 0x0FFF;
    return _sequence_counter;
}

// ─────────────────────────────────────────
//  Information Elements (IE) Builder
// ─────────────────────────────────────────
size_t FrameConstructionModule::_addInformationElements(
    uint8_t* payload_start,
    size_t max_space,
    const char* ssid,
    uint8_t channel) {

    size_t pos = 0;

    // SSID IE (Element ID = 0)
    if (ssid) {
        size_t ssid_len = strlen(ssid);
        if (ssid_len > 32) ssid_len = 32;
        
        if (pos + 2 + ssid_len > max_space) return pos;
        
        payload_start[pos++] = 0x00;           // Element ID: SSID
        payload_start[pos++] = ssid_len;       // Length
        memcpy(&payload_start[pos], ssid, ssid_len);
        pos += ssid_len;
    }

    // Supported Rates IE (Element ID = 1)
    if (pos + 10 > max_space) return pos;
    payload_start[pos++] = 0x01;               // Element ID: Supported Rates
    payload_start[pos++] = 0x08;               // Length: 8 rates
    // Rates in 500kbps units, MSB=1 for basic rates
    payload_start[pos++] = 0x82;               // 1 Mbps (basic)
    payload_start[pos++] = 0x84;               // 2 Mbps (basic)
    payload_start[pos++] = 0x8B;               // 5.5 Mbps (basic)
    payload_start[pos++] = 0x96;               // 11 Mbps (basic)
    payload_start[pos++] = 0x0C;               // 6 Mbps
    payload_start[pos++] = 0x12;               // 9 Mbps
    payload_start[pos++] = 0x18;               // 12 Mbps
    payload_start[pos++] = 0x24;               // 18 Mbps

    // DS Parameter Set IE (Element ID = 3) — channel info
    if (channel > 0 && pos + 3 <= max_space) {
        payload_start[pos++] = 0x03;           // Element ID: DS Parameter Set
        payload_start[pos++] = 0x01;           // Length: 1 byte
        payload_start[pos++] = channel;        // Current Channel
    }

    return pos;
}

bool FrameConstructionModule::_validateFrameLength(size_t length) const {
    if (length > FrameBuffer::MAX_SIZE) {
        _error_count++;
        return false;
    }
    return true;
}

void FrameConstructionModule::_handleBufferOverflow() {
    Serial.println("[FrameConstruct] Buffer overflow detected!");
    _state = FrameState::ERROR;
    _error_count++;
    _display.drawCenteredText("Buffer overflow!", SCREEN_H / 2, CLR_DANGER);
}

// ─────────────────────────────────────────
//  Frame Builders — Management Frames
// ─────────────────────────────────────────

size_t FrameConstructionModule::buildBeaconFrame(
    const uint8_t* bssid,
    const char* ssid,
    uint8_t channel,
    uint16_t beacon_interval,
    uint16_t capabilities) {

    MACHeader hdr;
    _initMACHeader(
        hdr,
        FrameType::MANAGEMENT,
        static_cast<uint8_t>(ManagementSubtype::BEACON),
        (const uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF",  // broadcast
        bssid,                                        // source
        bssid                                         // BSSID
    );

    size_t pos = 0;

    // Copy MAC header
    memcpy(&_buffer.data[pos], &hdr, sizeof(hdr));
    pos += sizeof(hdr);

    // Beacon frame body
    BeaconFrameBody body;
    body.timestamp = 0;  // Would be set to current TSF
    body.beacon_interval = beacon_interval;
    body.capability_info = capabilities;

    memcpy(&_buffer.data[pos], &body, sizeof(body));
    pos += sizeof(body);

    // Information elements
    size_t ie_len = _addInformationElements(
        &_buffer.data[pos],
        FrameBuffer::MAX_SIZE - pos,
        ssid,
        channel
    );
    pos += ie_len;

    if (!_validateFrameLength(pos)) {
        _handleBufferOverflow();
        return 0;
    }

    _buffer.length = pos;
    _buffer.channel = channel;
    _buffer.timestamp_ms = millis();
    _state = FrameState::COMPLETE;
    _frame_count++;

    _displayFrameInfo("Beacon", pos);
    return pos;
}

size_t FrameConstructionModule::buildProbeRequestFrame(
    const uint8_t* src_mac,
    const char* ssid) {

    MACHeader hdr;
    _initMACHeader(
        hdr,
        FrameType::MANAGEMENT,
        static_cast<uint8_t>(ManagementSubtype::PROBE_REQ),
        (const uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF",  // broadcast
        src_mac,
        (const uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF"   // broadcast BSSID
    );

    size_t pos = 0;
    memcpy(&_buffer.data[pos], &hdr, sizeof(hdr));
    pos += sizeof(hdr);

    // Probe request body is empty — just IEs
    size_t ie_len = _addInformationElements(
        &_buffer.data[pos],
        FrameBuffer::MAX_SIZE - pos,
        ssid,
        0  // no channel for broadcast probe
    );
    pos += ie_len;

    if (!_validateFrameLength(pos)) {
        _handleBufferOverflow();
        return 0;
    }

    _buffer.length = pos;
    _buffer.timestamp_ms = millis();
    _state = FrameState::COMPLETE;
    _frame_count++;

    _displayFrameInfo("Probe Request", pos);
    return pos;
}

size_t FrameConstructionModule::buildProbeResponseFrame(
    const uint8_t* bssid,
    const uint8_t* dst_mac,
    const char* ssid,
    uint8_t channel) {

    MACHeader hdr;
    _initMACHeader(
        hdr,
        FrameType::MANAGEMENT,
        static_cast<uint8_t>(ManagementSubtype::PROBE_RESP),
        dst_mac,
        bssid,
        bssid
    );

    size_t pos = 0;
    memcpy(&_buffer.data[pos], &hdr, sizeof(hdr));
    pos += sizeof(hdr);

    // Probe response body (same as beacon)
    BeaconFrameBody body;
    body.timestamp = 0;
    body.beacon_interval = 100;
    body.capability_info = 0x0401;

    memcpy(&_buffer.data[pos], &body, sizeof(body));
    pos += sizeof(body);

    size_t ie_len = _addInformationElements(
        &_buffer.data[pos],
        FrameBuffer::MAX_SIZE - pos,
        ssid,
        channel
    );
    pos += ie_len;

    if (!_validateFrameLength(pos)) {
        _handleBufferOverflow();
        return 0;
    }

    _buffer.length = pos;
    _buffer.channel = channel;
    _buffer.timestamp_ms = millis();
    _state = FrameState::COMPLETE;
    _frame_count++;

    _displayFrameInfo("Probe Response", pos);
    return pos;
}

size_t FrameConstructionModule::buildAuthenticationFrame(
    const uint8_t* bssid,
    const uint8_t* src_mac,
    uint16_t auth_alg,
    uint16_t auth_seq,
    uint16_t status_code) {

    MACHeader hdr;
    _initMACHeader(
        hdr,
        FrameType::MANAGEMENT,
        static_cast<uint8_t>(ManagementSubtype::AUTH),
        bssid,
        src_mac,
        bssid
    );

    size_t pos = 0;
    memcpy(&_buffer.data[pos], &hdr, sizeof(hdr));
    pos += sizeof(hdr);

    // Authentication frame body
    AuthFrameBody body;
    body.auth_alg = auth_alg;       // 0 = Open System, 1 = Shared Key, 2 = FT, 3 = SAE
    body.auth_seq = auth_seq;
    body.status_code = status_code;

    memcpy(&_buffer.data[pos], &body, sizeof(body));
    pos += sizeof(body);

    if (!_validateFrameLength(pos)) {
        _handleBufferOverflow();
        return 0;
    }

    _buffer.length = pos;
    _buffer.timestamp_ms = millis();
    _state = FrameState::COMPLETE;
    _frame_count++;

    _displayFrameInfo("Authentication", pos);
    return pos;
}

size_t FrameConstructionModule::buildDeauthenticationFrame(
    const uint8_t* dst_mac,
    const uint8_t* bssid,
    uint16_t reason_code,
    uint16_t seq_num) {

    MACHeader hdr;
    _initMACHeader(
        hdr,
        FrameType::MANAGEMENT,
        static_cast<uint8_t>(ManagementSubtype::DEAUTH),
        dst_mac,
        bssid,
        bssid
    );

    size_t pos = 0;
    memcpy(&_buffer.data[pos], &hdr, sizeof(hdr));
    pos += sizeof(hdr);

    // Deauthentication frame body
    DeauthFrameBody body;
    body.reason_code = reason_code;

    memcpy(&_buffer.data[pos], &body, sizeof(body));
    pos += sizeof(body);

    if (!_validateFrameLength(pos)) {
        _handleBufferOverflow();
        return 0;
    }

    _buffer.length = pos;
    _buffer.timestamp_ms = millis();
    _state = FrameState::COMPLETE;
    _frame_count++;

    _displayFrameInfo("Deauthentication", pos);
    return pos;
}

size_t FrameConstructionModule::buildDisassociationFrame(
    const uint8_t* dst_mac,
    const uint8_t* bssid,
    uint16_t reason_code,
    uint16_t seq_num) {

    // Disassociation uses same format as deauthentication
    MACHeader hdr;
    _initMACHeader(
        hdr,
        FrameType::MANAGEMENT,
        static_cast<uint8_t>(ManagementSubtype::DISASSOC),
        dst_mac,
        bssid,
        bssid
    );

    size_t pos = 0;
    memcpy(&_buffer.data[pos], &hdr, sizeof(hdr));
    pos += sizeof(hdr);

    DeauthFrameBody body;
    body.reason_code = reason_code;

    memcpy(&_buffer.data[pos], &body, sizeof(body));
    pos += sizeof(body);

    if (!_validateFrameLength(pos)) {
        _handleBufferOverflow();
        return 0;
    }

    _buffer.length = pos;
    _buffer.timestamp_ms = millis();
    _state = FrameState::COMPLETE;
    _frame_count++;

    _displayFrameInfo("Disassociation", pos);
    return pos;
}

size_t FrameConstructionModule::buildAssociationRequestFrame(
    const uint8_t* bssid,
    const uint8_t* src_mac,
    const char* ssid,
    uint16_t capabilities) {

    MACHeader hdr;
    _initMACHeader(
        hdr,
        FrameType::MANAGEMENT,
        static_cast<uint8_t>(ManagementSubtype::ASSOC_REQ),
        bssid,
        src_mac,
        bssid
    );

    size_t pos = 0;
    memcpy(&_buffer.data[pos], &hdr, sizeof(hdr));
    pos += sizeof(hdr);

    // Association request body
    uint16_t cap = capabilities;
    uint16_t listen_interval = 0x000A;

    memcpy(&_buffer.data[pos], &cap, sizeof(cap));
    pos += sizeof(cap);
    memcpy(&_buffer.data[pos], &listen_interval, sizeof(listen_interval));
    pos += sizeof(listen_interval);

    // Add IEs
    size_t ie_len = _addInformationElements(
        &_buffer.data[pos],
        FrameBuffer::MAX_SIZE - pos,
        ssid,
        0
    );
    pos += ie_len;

    if (!_validateFrameLength(pos)) {
        _handleBufferOverflow();
        return 0;
    }

    _buffer.length = pos;
    _buffer.timestamp_ms = millis();
    _state = FrameState::COMPLETE;
    _frame_count++;

    _displayFrameInfo("Association Request", pos);
    return pos;
}

// ─────────────────────────────────────────
//  Frame Builders — Control Frames
// ─────────────────────────────────────────

size_t FrameConstructionModule::buildRTSFrame(
    const uint8_t* receiver_addr,
    const uint8_t* src_mac,
    uint16_t duration_us) {

    MACHeader hdr;
    _initMACHeader(
        hdr,
        FrameType::CONTROL,
        static_cast<uint8_t>(ControlSubtype::RTS),
        receiver_addr,
        src_mac,
        receiver_addr  // For control frames, addr3 is typically RA
    );

    hdr.duration_id = duration_us;

    size_t pos = 0;
    memcpy(&_buffer.data[pos], &hdr, sizeof(hdr));
    pos += sizeof(hdr);

    if (!_validateFrameLength(pos)) {
        _handleBufferOverflow();
        return 0;
    }

    _buffer.length = pos;
    _buffer.timestamp_ms = millis();
    _state = FrameState::COMPLETE;
    _frame_count++;

    _displayFrameInfo("RTS", pos);
    return pos;
}

size_t FrameConstructionModule::buildCTSFrame(
    const uint8_t* receiver_addr,
    uint16_t duration_us) {

    // CTS has only 14 bytes: FC + Duration + RA
    uint8_t frame[14];
    memset(frame, 0, sizeof(frame));

    // Frame control: CTS subtype = 0x0C
    uint16_t fc = (static_cast<uint16_t>(FrameType::CONTROL) << 2) |
                  (static_cast<uint16_t>(ControlSubtype::CTS) << 4);
    memcpy(&frame[0], &fc, sizeof(fc));

    // Duration
    memcpy(&frame[2], &duration_us, sizeof(duration_us));

    // Receiver address
    memcpy(&frame[4], receiver_addr, 6);

    memcpy(_buffer.data, frame, sizeof(frame));
    _buffer.length = sizeof(frame);
    _buffer.timestamp_ms = millis();
    _state = FrameState::COMPLETE;
    _frame_count++;

    _displayFrameInfo("CTS", _buffer.length);
    return _buffer.length;
}

size_t FrameConstructionModule::buildACKFrame(
    const uint8_t* receiver_addr) {

    // ACK has only 10 bytes: FC + Duration + RA
    uint8_t frame[10];
    memset(frame, 0, sizeof(frame));

    uint16_t fc = (static_cast<uint16_t>(FrameType::CONTROL) << 2) |
                  (static_cast<uint16_t>(ControlSubtype::ACK) << 4);
    memcpy(&frame[0], &fc, sizeof(fc));

    uint16_t duration = 0;
    memcpy(&frame[2], &duration, sizeof(duration));

    memcpy(&frame[4], receiver_addr, 6);

    memcpy(_buffer.data, frame, sizeof(frame));
    _buffer.length = sizeof(frame);
    _buffer.timestamp_ms = millis();
    _state = FrameState::COMPLETE;
    _frame_count++;

    _displayFrameInfo("ACK", _buffer.length);
    return _buffer.length;
}

// ─────────────────────────────────────────
//  Transmission
// ─────────────────────────────────────────

esp_err_t FrameConstructionModule::transmitFrame(uint8_t channel, bool wait_ack) {
    if (_state == FrameState::IDLE) {
        Serial.println("[FrameConstruct] No frame to transmit");
        return ESP_FAIL;
    }

    if (_buffer.length == 0 || _buffer.length > FrameBuffer::MAX_SIZE) {
        _error_count++;
        return ESP_ERR_INVALID_SIZE;
    }

    // Set channel
    if (esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        _error_count++;
        return ESP_FAIL;
    }

    // Transmit raw 802.11 frame
    esp_err_t ret = esp_wifi_80211_tx(WIFI_IF_STA, _buffer.data, _buffer.length, false);

    if (ret == ESP_OK) {
        _transmit_count++;
        _state = FrameState::TRANSMITTING;
        _state_start_ms = millis();
        _current_retry = 0;
        _last_transmit_ms = millis();
    } else {
        _error_count++;
    }

    return ret;
}

esp_err_t FrameConstructionModule::transmitFrameRepeat(
    uint8_t channel,
    uint8_t repeat_count,
    uint32_t interval_ms) {

    _max_retries = repeat_count;
    _retry_interval_ms = interval_ms;
    _current_retry = 0;

    return transmitFrame(channel);
}

// ─────────────────────────────────────────
//  GATT Integration
// ─────────────────────────────────────────

void FrameConstructionModule::updateGATTCharacteristic(const GATTCharacteristic& gatt_char) {
    // Map GATT characteristics to Wi-Fi state
    // UUID examples:
    //  0x2A19 = Battery Level (use for signal strength)
    //  0x2A00 = Device Name
    //  0x2A01 = Appearance

    if (gatt_char.uuid == 0x2A19) {
        // Battery level can map to association confidence
        _gatt_connection_state = (gatt_char.value[0] > 50) ? 1 : 0;
    } else if (gatt_char.uuid == 0x2A00) {
        // Device name characteristic
        Serial.printf("[FrameConstruct] GATT Device: %.20s\n", gatt_char.value);
    }

    _gatt_mtu = gatt_char.value_length;
}

const char* FrameConstructionModule::getStateString() const {
    switch (_state) {
        case FrameState::IDLE:        return "IDLE";
        case FrameState::BUILDING:    return "BUILDING";
        case FrameState::TRANSMITTING:return "TX";
        case FrameState::COMPLETE:    return "COMPLETE";
        case FrameState::ERROR:       return "ERROR";
        default:                      return "UNKNOWN";
    }
}

void FrameConstructionModule::setWiFiMode(wifi_mode_t mode) {
    WiFi.mode(mode);
}

void FrameConstructionModule::setChannel(uint8_t channel) {
    _buffer.channel = channel;
}

void FrameConstructionModule::setRetryPolicy(uint8_t max_retries, uint32_t retry_interval_ms) {
    _max_retries = max_retries;
    _retry_interval_ms = retry_interval_ms;
}

void FrameConstructionModule::_displayFrameInfo(const char* frame_type, size_t length) {
    Serial.printf("[FrameConstruct] %s frame constructed (%d bytes, seq: %04X)\n",
        frame_type, length, _sequence_counter);
}

void FrameConstructionModule::stop() {
    _state = FrameState::IDLE;
    WiFi.mode(WIFI_OFF);
    Serial.println("[FrameConstruct] Stopped");
}
