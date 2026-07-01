## OUROBOROS Firmware Framework - Complete Implementation Guide

### 📋 Overview

This document describes the three major wireless research modules integrated into the OUROBOROS ESP32 firmware:

1. **FrameConstructionModule** - IEEE 802.11 frame builder & transmission engine
2. **EvilTwinModule** - Rogue AP (Evil Twin) attack implementation  
3. **Frame Analysis Tests** - Protocol compliance & stability testing

All modules use **non-blocking state machines** and integrate seamlessly with the existing menu-driven UI.

---

## 🏗️ Architecture

### Module Stack

```
┌─────────────────────────────────────────┐
│        OUROBOROS Main Loop              │
│  (Non-blocking tick-based design)       │
└─────────────────────────────────────────┘
         ↓          ↓          ↓
    ┌────────┐ ┌─────────┐ ┌──────────┐
    │ WiFi   │ │ BLE     │ │ Sub-GHz  │
    │ Attack │ │ Module  │ │ Module   │
    └────────┘ └─────────┘ └──────────┘
         ↓          ↓          ↓
    ┌────────────────────────────────────┐
    │  FrameConstructionModule           │
    │  (IEEE 802.11 frame engine)        │
    └────────────────────────────────────┘
         ↓          ↓
    ┌─────────┐ ┌──────────────┐
    │ WiFi    │ │ EvilTwinModule
    │ 80211TX │ │ (Rogue AP)    
    └─────────┘ └──────────────┘
```

### Memory Usage

| Component | RAM | Flash | Notes |
|-----------|-----|-------|-------|
| FrameConstructionModule | 2 KB | 12 KB | Static buffers (512B frame max) |
| EvilTwinModule | 8 KB | 15 KB | NVS storage (no SD card needed) |
| Frame Analysis Tests | <1 KB | - | Runtime state only |
| **Total Overhead** | **~10 KB** | **~27 KB** | Fits in 500 KB budget |

---

## 🔧 FrameConstructionModule

### Purpose
Build and transmit IEEE 802.11 management frames with full protocol compliance. Useful for:
- Testing device association/authentication flows
- Probing AP stability under rapid frame injection
- GATT characteristic influence on Wi-Fi state machine

### Quick Start

```cpp
// Initialize
FrameConstructionModule frameBuilder(display);
frameBuilder.begin();

// Build beacon frame
frameBuilder.buildBeaconFrame(
    bssid,                    // 6-byte BSSID
    "TestSSID",               // SSID string
    6,                        // Channel
    100,                      // Beacon interval (100 TU)
    0x0401                    // Capabilities
);

// Transmit on channel 6
frameBuilder.transmitFrame(6);

// Non-blocking tick (call every loop iteration)
frameBuilder.tick();
```

### Supported Frame Types

#### Management Frames
- **Beacon** - AP advertisement
- **Probe Request/Response** - Station discovery
- **Authentication** - Open System / Shared Key
- **Association Request** - Station joins AP
- **Deauthentication** - Disconnect client
- **Disassociation** - Force client off AP

#### Control Frames
- **RTS/CTS** - Medium access negotiation
- **ACK** - Positive acknowledgment

### State Machine

```
IDLE → BUILDING → TRANSMITTING → COMPLETE → IDLE
         ↓           ↓
       ERROR      RETRY_LOOP
```

**Configurable Retry Policy:**
```cpp
frameBuilder.setRetryPolicy(
    10,     // max_retries
    100     // retry_interval_ms
);
```

### GATT Integration

Link BLE characteristics to frame assembly state:

```cpp
GATTCharacteristic batteryLevel;
batteryLevel.uuid = 0x2A19;           // Battery Level UUID
batteryLevel.value[0] = 75;            // 75% battery
frameBuilder.updateGATTCharacteristic(batteryLevel);
```

This allows testing how cross-protocol state (BLE ↔ Wi-Fi) affects device behavior.

### Protocol Compliance Details

All frames include:
- **IEEE 802.11 MAC Headers** (24 bytes minimum)
  - Frame Control (2 bytes): Type/Subtype/Flags
  - Duration/ID (2 bytes): NAV field
  - MAC Addresses (18 bytes): DA/SA/BSSID
  - Sequence Control (2 bytes): Sequence # / Fragment #

- **Information Elements (IEs)**
  - SSID tag (Element ID 0)
  - Supported Rates (Element ID 1)
  - DS Parameter Set (Element ID 3) - channel

- **Sequence Number Tracking**
  - Auto-incremented per frame
  - Prevents duplicate detection

---

## 👹 EvilTwinModule

### Purpose
Create a rogue access point that mimics legitimate APs. Useful for:
- Testing client roaming behavior
- Capturing EAPOL/WPA handshakes (packet sniffing)
- Testing credential/DHCP capture
- Studying device association logic

### Quick Start

```cpp
// Initialize
EvilTwinModule evilTwin(display);
evilTwin.begin();

// Step 1: Scan for target APs
evilTwin.startScan();

// Step 2: Select target (index 0-9)
evilTwin.selectTarget(0);

// Step 3: Start evil twin (open network)
evilTwin.startEvilTwin(EvilTwinMode::OPEN_NETWORK);

// Non-blocking tick
evilTwin.tick();

// Stop and retrieve captured data
evilTwin.stop();
int captured = evilTwin.getCapturedCredentialCount();
```

### Attack Modes

#### 1. OPEN_NETWORK
No encryption - simulates unprotected public AP
```cpp
evilTwin.startEvilTwin(EvilTwinMode::OPEN_NETWORK);
```

#### 2. WPA2_MIMIC
Mimics WPA2 frame structure (no real encryption)
```cpp
evilTwin.startEvilTwin(EvilTwinMode::WPA2_MIMIC);
```

#### 3. CAPTIVE_PORTAL
Redirects HTTP traffic to fake portal (DNS spoofing)
```cpp
evilTwin.startEvilTwin(EvilTwinMode::CAPTIVE_PORTAL);
```

### Capture Mechanisms

#### EAPOL Capture
Sniffs 802.1X authentication packets:
```cpp
evilTwin.setCaptureFilter(true, false);  // EAPOL only
```

#### DHCP Capture
Intercepts DHCP requests (client hostnames):
```cpp
evilTwin.setCaptureFilter(false, true);  // DHCP only
```

### Data Persistence (NVS)

Credentials stored in ESP32's NVS flash (no SD card):

```cpp
// Manually save captured credentials
evilTwin.saveCredentials();

// Load previously captured data
evilTwin.loadCredentials();

// Clear all stored data
evilTwin.clearStoredCredentials();
```

**Storage Format:**
- Per credential: SSID (32B) + MAC (6B) + Username (65B) + Password (65B) = ~168 bytes
- Max 50 credentials = ~8.4 KB NVS usage

### State Machine

```
IDLE
  ↓ startScan()
SCANNING → IDLE (results stored)
  ↓ selectTarget()
CLONING → BROADCASTING
  ↓ (beacon spam)
CAPTURING (promiscuous mode, packet sniff)
  ↓ timeout / stop()
COMPLETE → IDLE
```

### Connected Client Tracking

```cpp
// Get count of clients associated with fake AP
int clients = evilTwin.getConnectedClientCount();

// Captured EAPOL packets
int packets = evilTwin.getPacketsCaptured();
```

### UI Screens

| Screen | Purpose |
|--------|---------|
| `EVIL_TWIN_SCAN` | List target APs, select one |
| `EVIL_TWIN_RUNNING` | Live stats (beacons, clients, captures) |
| `EVIL_TWIN_CREDENTIALS` | Display captured data |

---

## 📊 Frame Analysis Tests

Five built-in test scenarios for protocol compliance research:

### Test 1: Beacon Transmission Test
**Purpose:** Verify beacon frame generation & channel rotation

- Generates beacon on current channel
- Transmits every 500ms (standard beacon interval)
- Rotates channels: 1 → 6 → 11 → 1
- Duration: 30 seconds

**Use Case:** Test whether station firmware correctly parses beacon IEs across channels

```
Button: SELECT_LONG to start
Button: UP/DOWN to change channel
Button: BACK to exit
```

### Test 2: Authentication Sequence Test
**Purpose:** Full 4-way association handshake simulation

Sequence:
1. Probe Request → AP (discovers AP, broadcasts if no SSID specified)
2. Probe Response ← AP (responds with beacon info)
3. Authentication → AP (Open System, Auth Seq 1)
4. Association Request → AP (includes SSID + capabilities)
5. Deauthentication → AP (forces disconnection)

**Use Case:** Measure association timing, test retry logic, verify state transitions

**Timing:**
- 100ms between each step
- 200ms after deauth to complete

### Test 3: Deauthentication Burst
**Purpose:** Stress-test AP stability under attack conditions

- Sends broadcast deauth frames (all clients) every 50ms
- Duration: 5 seconds (~100 frames total)
- Measures frame transmission reliability

**Use Case:** Test whether target AP/station recovers from rapid disconnection spam

**Metrics:**
- Frames transmitted successfully
- Error count

### Test 4: Probe Request Injection
**Purpose:** Test discovery/roaming behavior during active scanning

- Sends probe request every 200ms
- Rotates channels: 1 → 6 → 11 (common Wi-Fi channels)
- Duration: 10 seconds

**Use Case:** Verify station correctly identifies AP across band transitions

**Channel Rotation Pattern:**
```
1 (200ms) → 6 (200ms) → 11 (200ms) → 1 (repeat)
```

### Test 5: GATT Integration Test
**Purpose:** Cross-protocol state synchronization (Wi-Fi ↔ BLE)

- Builds beacon frame with GATT characteristics
- Simulates changing BLE battery level (50-100%)
- Transmits every 1 second
- Duration: 15 seconds

**Use Case:** Research how Wi-Fi association state reacts to BLE connection quality changes

**BLE Characteristics Used:**
- `0x2A19` - Battery Level (changes every 100ms)

---

## 🎛️ UI Integration

### Main Menu Structure

```
Main Menu
├── WiFi
│   ├── Scan
│   ├── Deauth
│   ├── Beacon Spam
│   ├── Probe Sniff
│   └── RSSI Mapper
├── BLE
│   ├── Scan
│   └── Advertisement Spam
├── Sub-GHz
│   ├── Scan
│   ├── Capture
│   ├── Config
│   └── Rolling Code Detector
├── Frame Analysis (NEW)
│   ├── Beacon Test
│   ├── Auth Sequence
│   ├── Deauth Burst
│   ├── Probe Test
│   └── GATT Integration
└── Evil Twin (NEW)
    ├── Scan & Select
    ├── Running Attack
    └── View Credentials
```

### Button Controls

#### Universal Controls
- **SELECT_LONG**: Start/stop attack module
- **BACK_LONG**: Jump to main menu
- **UP/DOWN**: Navigate / adjust channel

#### Frame Analysis Screens
- **SELECT_LONG**: Start test (from idle) or stop test (running)
- **UP**: Decrease channel
- **DOWN**: Increase channel
- **BACK**: Return to main menu

#### Evil Twin Screens
- **UP/DOWN**: Scroll AP list
- **SELECT_LONG**: Initiate attack on selected AP
- **SELECT**: Refresh scan
- **BACK**: Return to main menu

---

## 🔍 Diagnostic Output

### Serial Logging

All modules log to Serial @ 115200 baud:

```
[FrameConstruct] Beacon frame constructed (74 bytes, seq: 0001)
[FrameConstruct] Frame transmitted successfully
[EvilTwin] Target selected: FreeWiFi (00:11:22:33:44:55)
[EvilTwin] AP running (OPEN)
[EvilTwin] Client connected: AA:BB:CC:DD:EE:FF (total: 1)
[EvilTwin] EAPOL captured from AA:BB:CC:DD:EE:FF
[Frame Analysis] Step 2: Authentication sent
```

### On-Display Metrics

**Frame Analysis Screen:**
- State: IDLE | BUILDING | TX | COMPLETE | ERROR
- Frames: Total frames constructed
- TX: Total transmissions
- Errors: Buffer overflow / transmission failures
- Mode: Current test
- Channel: Wi-Fi channel (1-13)

**Evil Twin Screen:**
- SSID: Target AP name
- Channel: Operating channel
- Beacons: Frames sent
- Clients: Connected count
- Captured: Credential/packet count
- Packets: EAPOL/DHCP sniffed

---

## 🛡️ Safety & Compliance

### Buffer Overflow Protection

```cpp
// Automatic validation in all builders
if (!_validateFrameLength(pos)) {
    _handleBufferOverflow();  // Sets ERROR state, increments error counter
    return 0;
}
```

### Radio State Management

- **Automatic mode switching**: STA ↔ AP as needed
- **Channel enforcement**: 1-13 valid channels
- **Graceful shutdown**: `stop()` cleans up all radio state

### Responsible Use

⚠️ **Important:**
- This firmware is for **authorized security testing only**
- Test only on networks/devices you own or have written permission to test
- Do not deploy in public networks
- Follow all local wireless regulations (FCC Part 15, etc.)

---

## 📈 Performance Metrics

### Typical Values

| Metric | Value | Notes |
|--------|-------|-------|
| Frame build time | <5ms | Depends on frame size |
| TX latency | <10ms | After transmission queued |
| Memory peak | ~20 KB | All modules active |
| Loop cycle time | 2-5ms | Non-blocking design |
| Frame rate | 100-500 fps | Mode-dependent |

### Optimization Tips

1. **Reduce UI redraws** - Only update display on state change
2. **Batch frame builds** - Pre-build frames before loop
3. **Limit promiscuous mode** - Only enable when capturing

---

## 🔗 Integration Examples

### Example 1: Programmatic Evil Twin Attack

```cpp
// Scan, select first AP, start evil twin
evilTwin.startScan();
delay(2000);

if (evilTwin.getScanCount() > 0) {
    evilTwin.selectTarget(0);
    evilTwin.startEvilTwin(EvilTwinMode::OPEN_NETWORK);
    
    // Run for 30 seconds
    uint32_t start = millis();
    while (millis() - start < 30000) {
        evilTwin.tick();
        frameBuilder.tick();
        delay(10);
    }
    
    evilTwin.stopEvilTwin();
    Serial.printf("Captured: %d credentials\n", 
        evilTwin.getCapturedCredentialCount());
}
```

### Example 2: Frame Construction + GATT Sync

```cpp
// Simulate BLE state influencing Wi-Fi frames
uint8_t bssid[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

for (int channel = 1; channel <= 13; channel++) {
    // Update BLE state
    GATTCharacteristic gatt;
    gatt.uuid = 0x2A19;
    gatt.value[0] = 100 - (channel * 7);  // Simulate signal strength
    frameBuilder.updateGATTCharacteristic(gatt);
    
    // Build + transmit beacon
    frameBuilder.buildBeaconFrame(bssid, "TestSSID", channel, 100, 0x0401);
    frameBuilder.transmitFrame(channel);
    frameBuilder.tick();
    
    delay(100);
}
```

### Example 3: Continuous Frame Analysis

```cpp
// Run all 5 tests sequentially
FrameAnalysisMode tests[] = {
    FrameAnalysisMode::BEACON_TEST,
    FrameAnalysisMode::AUTH_SEQUENCE,
    FrameAnalysisMode::DEAUTH_BURST,
    FrameAnalysisMode::PROBE_TEST,
    FrameAnalysisMode::GATT_INTEGRATION
};

for (auto test : tests) {
    frameAnalysis.mode = test;
    frameAnalysis.start_time_ms = millis();
    frameAnalysis.frame_count = 0;
    frameAnalysis.test_sequence = 0;
    
    // Run until test completes
    while (frameAnalysis.mode != FrameAnalysisMode::IDLE) {
        frameBuilder.tick();
        
        // Execute test logic (existing frameAnalysis_* functions)
        switch (test) {
            case FrameAnalysisMode::BEACON_TEST:
                frameAnalysis_BeaconTest(); break;
            // ... etc
        }
        
        delay(5);
    }
    
    Serial.printf("Test %d: %d frames transmitted\n", 
        (int)test, frameAnalysis.frame_count);
}
```

---

## 📝 Troubleshooting

### Module won't initialize

**Symptom:** Serial output shows `[Module] Failed to initialize`

**Solution:**
1. Check NVS flash is writable: `nvs_flash_init()`
2. Verify ESP32 IRAM/DRAM: `heap_caps_get_free_size(MALLOC_CAP_8BIT)`
3. Restart ESP32 (power cycle)

### Frames not transmitting

**Symptom:** State shows COMPLETE but no beacons seen on scanner

**Solution:**
1. Verify Wi-Fi mode: `WiFi.mode(WIFI_STA)` or `WIFI_AP`
2. Check channel: `esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE)`
3. Enable `esp_wifi_80211_tx()` API in sdkconfig

### Evil Twin not capturing credentials

**Symptom:** Frame count increases but captured credentials = 0

**Solution:**
1. Enable promiscuous mode: `esp_wifi_set_promiscuous(true)`
2. Verify client connects to fake AP
3. Check EAPOL filter is enabled: `setCaptureFilter(true, false)`

### Memory exhaustion

**Symptom:** System crashes after ~30 seconds

**Solution:**
1. Profile heap: Use `heap_caps_print_heap_info()`
2. Reduce NVS storage: `EVIL_TWIN_CREDENTIALS` from 50 → 20
3. Lower frame size limit: `FrameBuffer::MAX_SIZE` from 512 → 256

---

## 📚 References

- IEEE 802.11-2020 Standard (Management Frame Format)
- ESP-IDF API Reference: `esp_wifi`, `nvs_flash`
- WPA2/WPA3 Handshake Analysis
- FCC CFR Title 47 Part 15 (Unlicensed ISM Band Rules)

---

## 📞 Support

For issues or feature requests:
1. Check serial output for error messages
2. Review module state machine (use `getStateString()`)
3. Enable verbose logging in module constructors
4. Test on clean ESP32 flash (`pio run --target erase`)

---

**Last Updated:** July 1, 2026  
**OUROBOROS Version:** 2.0 (Frame Analysis + Evil Twin)  
**ESP32 Framework:** Arduino + ESP-IDF  
**License:** MIT
