#pragma once

#include <Arduino.h>
#include <esp_wifi.h>
#include <esp_err.h>
#include <string.h>
#include "../ui/Display.h"
#include "config.h"

// ─────────────────────────────────────────
//  IEEE 802.11 Frame Types & Subtypes
// ─────────────────────────────────────────
enum class FrameType : uint8_t {
    MANAGEMENT  = 0x00,
    CONTROL     = 0x01,
    DATA        = 0x02,
    QOS_DATA    = 0x03
};

enum class ManagementSubtype : uint8_t {
    ASSOC_REQ    = 0x00,
    ASSOC_RESP   = 0x01,
    REASSOC_REQ  = 0x02,
    REASSOC_RESP = 0x03,
    PROBE_REQ    = 0x04,
    PROBE_RESP   = 0x05,
    BEACON       = 0x08,
    ATIM         = 0x09,
    DISASSOC     = 0x0A,
    AUTH         = 0x0B,
    DEAUTH       = 0x0C,
    ACTION       = 0x0D
};

enum class ControlSubtype : uint8_t {
    TRIGGER      = 0x02,
    TACK         = 0x03,
    BEAMFORM_RPT = 0x04,
    NDP_ANNOUNCE = 0x05,
    CONTROL_WRAP = 0x07,
    BLOCK_ACK_REQ= 0x08,
    BLOCK_ACK    = 0x09,
    PS_POLL      = 0x0A,
    RTS          = 0x0B,
    CTS          = 0x0C,
    ACK          = 0x0D,
    CF_END       = 0x0E,
    CF_END_ACK   = 0x0F
};

// ─────────────────────────────────────────
//  Frame Construction State Machine
// ─────────────────────────────────────────
enum class FrameState : uint8_t {
    IDLE          = 0,
    BUILDING      = 1,
    TRANSMITTING  = 2,
    COMPLETE      = 3,
    ERROR         = 4
};

// ─────────────────────────────────────────
//  Frame Buffer & Metadata
// ─────────────────────────────────────────
struct FrameBuffer {
    static constexpr size_t MAX_SIZE = 512;  // IEEE 802.11 max MPDU
    
    uint8_t   data[MAX_SIZE];
    size_t    length;
    uint8_t   channel;
    uint8_t   retry_count;
    uint32_t  timestamp_ms;
};

// ─────────────────────────────────────────
//  MAC Header Structure (24 bytes minimum)
// ─────────────────────────────────────────
struct __attribute__((packed)) MACHeader {
    uint16_t frame_control;           // Offset 0-1
    uint16_t duration_id;              // Offset 2-3
    uint8_t  addr1[6];                 // Destination MAC
    uint8_t  addr2[6];                 // Source MAC
    uint8_t  addr3[6];                 // BSSID or TA/RA
    uint16_t sequence_control;         // Offset 22-23
};

// ─────────────────────────────────────────
//  Management Frame Bodies
// ─────────────────────────────────────────
struct __attribute__((packed)) BeaconFrameBody {
    uint64_t timestamp;                // TSF
    uint16_t beacon_interval;
    uint16_t capability_info;
};

struct __attribute__((packed)) AuthFrameBody {
    uint16_t auth_alg;
    uint16_t auth_seq;
    uint16_t status_code;
};

struct __attribute__((packed)) DeauthFrameBody {
    uint16_t reason_code;
};

// ─────────────────────────────────────────
//  Information Elements (IEs)
// ─────────────────────────────────────────
struct IE {
    uint8_t  element_id;
    uint8_t  length;
    uint8_t  data[250];
};

// ─────────────────────────────────────────
//  GATT Characteristics Reference
// ─────────────────────────────────────────
struct GATTCharacteristic {
    uint16_t uuid;
    uint8_t  properties;       // read/write/notify/indicate
    uint8_t  value[20];
    uint8_t  value_length;
};

// ─────────────────────────────────────────
//  Frame Construction Module
// ─────────────────────────────────────────
class FrameConstructionModule {
public:
    FrameConstructionModule(Display& display);
    ~FrameConstructionModule();

    // ── Lifecycle ────────────────────────────────
    void begin();
    void tick();        // Non-blocking state machine tick

    // ── Frame Builders (Management Frames) ────────
    // Returns frame length on success, 0 on failure
    size_t buildBeaconFrame(
        const uint8_t* bssid,
        const char* ssid,
        uint8_t channel,
        uint16_t beacon_interval = 100,
        uint16_t capabilities = 0x0401
    );

    size_t buildProbeRequestFrame(
        const uint8_t* src_mac,
        const char* ssid = nullptr  // nullptr = broadcast probe
    );

    size_t buildProbeResponseFrame(
        const uint8_t* bssid,
        const uint8_t* dst_mac,
        const char* ssid,
        uint8_t channel
    );

    size_t buildAuthenticationFrame(
        const uint8_t* bssid,
        const uint8_t* src_mac,
        uint16_t auth_alg,
        uint16_t auth_seq,
        uint16_t status_code = 0
    );

    size_t buildDeauthenticationFrame(
        const uint8_t* dst_mac,
        const uint8_t* bssid,
        uint16_t reason_code = 0x0007,
        uint16_t seq_num = 0
    );

    size_t buildDisassociationFrame(
        const uint8_t* dst_mac,
        const uint8_t* bssid,
        uint16_t reason_code = 0x0008,
        uint16_t seq_num = 0
    );

    size_t buildAssociationRequestFrame(
        const uint8_t* bssid,
        const uint8_t* src_mac,
        const char* ssid,
        uint16_t capabilities = 0x0401
    );

    // ── Control Frames ───────────────────────────
    size_t buildRTSFrame(
        const uint8_t* receiver_addr,
        const uint8_t* src_mac,
        uint16_t duration_us = 0x13A9
    );

    size_t buildCTSFrame(
        const uint8_t* receiver_addr,
        uint16_t duration_us = 0x13A9
    );

    size_t buildACKFrame(
        const uint8_t* receiver_addr
    );

    // ── Transmission ─────────────────────────────
    esp_err_t transmitFrame(uint8_t channel, bool wait_ack = false);
    esp_err_t transmitFrameRepeat(uint8_t channel, uint8_t repeat_count = 5, uint32_t interval_ms = 100);

    // ── GATT Integration ─────────────────────────
    // Update device state based on GATT characteristic
    void updateGATTCharacteristic(const GATTCharacteristic& gatt_char);
    
    // Get current association state
    uint8_t getAssociationState() const { return static_cast<uint8_t>(_state); }
    uint32_t getLastFrameTimestamp() const { return _buffer.timestamp_ms; }

    // ── State & Diagnostics ─────────────────────
    FrameState getState() const { return _state; }
    const char* getStateString() const;
    const FrameBuffer& getBuffer() const { return _buffer; }
    uint32_t getFrameCount() const { return _frame_count; }
    uint32_t getTransmitCount() const { return _transmit_count; }
    uint32_t getErrorCount() const { return _error_count; }

    // ── Configuration ────────────────────────────
    void setWiFiMode(wifi_mode_t mode);
    void setChannel(uint8_t channel);
    void setRetryPolicy(uint8_t max_retries, uint32_t retry_interval_ms);

    // ── Cleanup ──────────────────────────────────
    void stop();

private:
    Display&       _display;
    FrameBuffer    _buffer;
    FrameState     _state = FrameState::IDLE;
    
    // ── State Machine Internals ──────────────────
    uint32_t       _state_start_ms = 0;
    uint32_t       _last_transmit_ms = 0;
    uint8_t        _current_retry = 0;
    uint8_t        _max_retries = 3;
    uint32_t       _retry_interval_ms = 100;
    
    // ── Diagnostics ──────────────────────────────
    uint32_t       _frame_count = 0;
    uint32_t       _transmit_count = 0;
    uint32_t       _error_count = 0;
    
    // ── GATT State Tracking ──────────────────────
    uint8_t        _gatt_connection_state = 0;
    uint16_t       _gatt_mtu = 23;
    
    // ── Internal Helpers ─────────────────────────
    void _initMACHeader(
        MACHeader& hdr,
        FrameType type,
        uint8_t subtype,
        const uint8_t* dst,
        const uint8_t* src,
        const uint8_t* bssid
    );

    size_t _addInformationElements(
        uint8_t* payload_start,
        size_t max_space,
        const char* ssid = nullptr,
        uint8_t channel = 0
    );

    bool _validateFrameLength(size_t length) const;
    void _handleBufferOverflow();
    void _displayFrameInfo(const char* frame_type, size_t length);
    
    // ── Sequence Number Management ───────────────
    static uint16_t _next_sequence_number();
    static uint16_t _sequence_counter;
};
