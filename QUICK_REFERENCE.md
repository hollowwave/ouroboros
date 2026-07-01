# OUROBOROS Quick Reference Card

## 🚀 Quick Start

### Initialize All Modules (in setup())
```cpp
display.begin();
buttons.begin();
nvs.begin();
ble.begin();
frameBuilder.begin();      // NEW
evilTwin.begin();          // NEW
subghz.begin();
```

### Main Loop Pattern
```cpp
void loop() {
    buttons.tick();
    
    // Non-blocking state machines
    frameBuilder.tick();    // NEW
    evilTwin.tick();        // NEW
    
    // Existing modules...
    wifi.tick();
    ble.tick();
}
```

---

## 📡 FrameConstructionModule API

### Build Frames
```cpp
// Beacon
frameBuilder.buildBeaconFrame(bssid, "SSID", channel, beacon_interval, capabilities);

// Probe Request
frameBuilder.buildProbeRequestFrame(client_mac, "SSID");

// Authentication
frameBuilder.buildAuthenticationFrame(bssid, client_mac, auth_alg, auth_seq, status);

// Deauthentication
frameBuilder.buildDeauthenticationFrame(dst_mac, bssid, reason_code);

// Association
frameBuilder.buildAssociationRequestFrame(bssid, client_mac, "SSID", capabilities);

// Control Frames
frameBuilder.buildRTSFrame(receiver, src_mac, duration);
frameBuilder.buildCTSFrame(receiver, duration);
frameBuilder.buildACKFrame(receiver);
```

### Transmit
```cpp
frameBuilder.transmitFrame(channel);                           // Single
frameBuilder.transmitFrameRepeat(channel, repeat_count, interval_ms);  // Burst
```

### Query State
```cpp
frameBuilder.getState();               // FrameState enum
frameBuilder.getStateString();         // "IDLE", "TX", etc
frameBuilder.getFrameCount();          // Total built
frameBuilder.getTransmitCount();       // Total sent
frameBuilder.getErrorCount();          // Failures
frameBuilder.getLastFrameTimestamp();  // ms
```

### Configuration
```cpp
frameBuilder.setWiFiMode(WIFI_STA);
frameBuilder.setChannel(6);
frameBuilder.setRetryPolicy(max_retries, interval_ms);
frameBuilder.updateGATTCharacteristic(gatt_char);  // BLE sync
frameBuilder.stop();
```

---

## 👹 EvilTwinModule API

### Attack Workflow
```cpp
// Step 1: Scan
evilTwin.startScan();
int count = evilTwin.getScanCount();
const TargetAPInfo* results = evilTwin.getScanResults();

// Step 2: Select
evilTwin.selectTarget(index);

// Step 3: Attack
evilTwin.startEvilTwin(EvilTwinMode::OPEN_NETWORK);
evilTwin.startEvilTwin(EvilTwinMode::WPA2_MIMIC);
evilTwin.startEvilTwin(EvilTwinMode::CAPTIVE_PORTAL);

// Step 4: Stop
evilTwin.stopEvilTwin();
```

### Capture Configuration
```cpp
evilTwin.setCaptureFilter(true, false);   // EAPOL only
evilTwin.setCaptureFilter(false, true);   // DHCP only
evilTwin.setCaptureFilter(true, true);    // Both
```

### Data Access
```cpp
evilTwin.getSelectedTarget();             // TargetAPInfo*
evilTwin.getConnectedClientCount();       // int
evilTwin.getCapturedCredentialCount();    // int
evilTwin.getCapturedCredentials();        // CapturedCredential*
evilTwin.getPacketsCaptured();            // uint32_t
evilTwin.getFramesSent();                 // uint32_t
evilTwin.getConnectionAttempts();         // uint32_t
```

### NVS Persistence
```cpp
evilTwin.saveCredentials();               // Write to flash
evilTwin.loadCredentials();               // Read from flash
evilTwin.clearStoredCredentials();        // Erase all
```

### Configuration
```cpp
evilTwin.setChannel(6);
evilTwin.setSSIDRotation(true);
evilTwin.getState();
evilTwin.getStateString();
evilTwin.stop();
```

---

## 🧪 Frame Analysis Tests

### Start a Test
```cpp
frameAnalysis.mode = FrameAnalysisMode::BEACON_TEST;
frameAnalysis.start_time_ms = millis();
frameAnalysis.frame_count = 0;
frameAnalysis.test_sequence = 0;

// Then call the appropriate test function in loop()
frameAnalysis_BeaconTest();
```

### Available Tests
| Test | Mode | Duration | Purpose |
|------|------|----------|---------|
| Beacon | `BEACON_TEST` | 30s | Verify beacon TX |
| Auth Seq | `AUTH_SEQUENCE` | 500ms | Full handshake |
| Deauth Burst | `DEAUTH_BURST` | 5s | Stress attack |
| Probe | `PROBE_TEST` | 10s | Channel hopping |
| GATT | `GATT_INTEGRATION` | 15s | Cross-protocol sync |

### Test Functions
```cpp
void frameAnalysis_BeaconTest();           // Auto channel rotation
void frameAnalysis_AuthSequence();         // 4-way handshake sim
void frameAnalysis_DeauthBurst();          // Rapid deauth spam
void frameAnalysis_ProbeTest();            // Probe on 1/6/11
void frameAnalysis_GATTIntegration();      // BLE state sync
```

---

## 🎛️ UI Screen Enum

```cpp
enum class Screen {
    // Existing...
    WIFI_MENU, WIFI_SCAN, WIFI_DEAUTH,
    BLE_MENU, BLE_SCAN,
    SUBGHZ_MENU, SUBGHZ_SCAN,
    
    // NEW: Frame Analysis
    FRAME_ANALYSIS,
    FRAME_ANALYSIS_BEACON,
    FRAME_ANALYSIS_AUTH,
    FRAME_ANALYSIS_DEAUTH,
    FRAME_ANALYSIS_PROBE,
    FRAME_ANALYSIS_GATT,
    
    // NEW: Evil Twin
    EVIL_TWIN_MENU,
    EVIL_TWIN_SCAN,
    EVIL_TWIN_RUNNING,
    EVIL_TWIN_CREDENTIALS,
};
```

---

## 📊 Data Structures

### FrameBuffer
```cpp
struct FrameBuffer {
    uint8_t   data[512];              // Raw frame bytes
    size_t    length;                 // Bytes used
    uint8_t   channel;                // Wi-Fi channel
    uint8_t   retry_count;            // Transmission attempt
    uint32_t  timestamp_ms;           // When built
};
```

### TargetAPInfo
```cpp
struct TargetAPInfo {
    char     ssid[33];
    uint8_t  bssid[6];
    int8_t   rssi;
    uint8_t  channel;
    uint8_t  encryption;
    bool     hidden;
    bool     is_active;
};
```

### CapturedCredential
```cpp
struct CapturedCredential {
    char     ssid[33];
    uint8_t  client_mac[6];
    char     username[65];
    char     password[65];
    uint32_t timestamp;
    bool     is_valid;
};
```

### GATTCharacteristic
```cpp
struct GATTCharacteristic {
    uint16_t uuid;                    // BLE UUID
    uint8_t  properties;              // R/W/Notify/Indicate
    uint8_t  value[20];               // Data
    uint8_t  value_length;            // Bytes used
};
```

---

## 🔢 Important Constants

```cpp
// FrameConstructionModule
#define FRAME_MAX_SIZE          512       // IEEE 802.11 MPDU max
#define SEQUENCE_NUMBER_MASK    0x0FFF    // 12-bit seq number

// EvilTwinModule
#define EVIL_TWIN_MAX_TARGETS   10        // AP scan limit
#define EVIL_TWIN_CREDENTIALS   50        // Stored captures
#define EVIL_TWIN_MAX_CLIENTS   8         // Concurrent stations
#define EVIL_TWIN_BROADCAST_INTERVAL 100  // ms

// WiFi Channels
#define WIFI_CHANNEL_MIN        1
#define WIFI_CHANNEL_MAX        13

// Common Channels
#define WIFI_CH_1G              {1, 6, 11}  // 2.4 GHz only
#define WIFI_CH_5G              {36, 40, 44, 48, ...}  // 5 GHz (varies by country)
```

---

## 📈 State Machine Diagrams

### FrameConstructionModule
```
┌─────────┐
│  IDLE   │◄────────────────────────┐
└────┬────┘                         │
     │ buildXxxFrame()              │
     ▼                              │
┌──────────┐                        │
│ BUILDING │                        │
└────┬─────┘                        │
     │ (automatic after build)      │
     ▼                              │
┌────────────┐    retransmit?       │
│ COMPLETE   │──────NO──────────────┘
└────┬────────┘
     │ YES (via tick())
     ▼
┌────────────┐
│ TRANSMIT   │
└────┬───────┘
     │ retry_interval elapsed
     ▼
┌────────┐    MAX_RETRIES reached
│ REPEAT │────────YES────────┐
└────┬───┘                   │
     │ NO                    │
     └───────────┬───────────┘
                 ▼
            COMPLETE → IDLE
```

### EvilTwinModule
```
     ┌───────┐
     │ IDLE  │
     └───┬───┘
         │ startScan()
         ▼
     ┌─────────┐
     │ SCANNING│
     └────┬────┘
          │ (WiFi.scanNetworks())
          ▼
     ┌──────────┐
     │ TARGET   │ ◄─── selectTarget()
     │ SELECTED │
     └────┬─────┘
          │ startEvilTwin()
          ▼
     ┌─────────┐
     │ CLONING │ (setup AP)
     └────┬────┘
          │ 200ms
          ▼
     ┌────────────┐
     │BROADCASTING│ (beacon spam)
     └────┬───────┘
          │ ▼ tick()
     ┌─────────────┐
     │ CAPTURING   │ (promiscuous mode)
     └────┬────────┘
          │ timeout
          ▼
     ┌──────────┐
     │ COMPLETE │
     └────┬─────┘
          │ 500ms
          ▼
     ┌───────┐
     │ IDLE  │
     └───────┘
```

---

## 🐛 Common Debugging

### Check Frame State
```cpp
Serial.printf("[Debug] Frame state: %s\n", frameBuilder.getStateString());
Serial.printf("[Debug] Frames built: %lu, TX: %lu, Errors: %lu\n",
    frameBuilder.getFrameCount(),
    frameBuilder.getTransmitCount(),
    frameBuilder.getErrorCount());
```

### Check Evil Twin Status
```cpp
Serial.printf("[Debug] ET state: %s\n", evilTwin.getStateString());
Serial.printf("[Debug] Target: %s\n", evilTwin.getSelectedTarget()->ssid);
Serial.printf("[Debug] Clients: %d, Captured: %d\n",
    evilTwin.getConnectedClientCount(),
    evilTwin.getCapturedCredentialCount());
```

### Monitor Memory
```cpp
#include <esp_heap_caps.h>

Serial.printf("[Heap] Free: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
```

### Enable Verbose Logging
```cpp
// Add to module .cpp files:
#define DEBUG_VERBOSE 1

#ifdef DEBUG_VERBOSE
    #define DBG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
    #define DBG_PRINTF(...)
#endif
```

---

## ⚠️ Common Pitfalls

| Issue | Cause | Fix |
|-------|-------|-----|
| Frames not TX'ing | Wi-Fi mode not STA | Call `WiFi.mode(WIFI_STA)` |
| Crash after 30s | Heap exhaustion | Reduce frame count/size |
| Evil Twin won't scan | NVS not initialized | Call `nvs_flash_init()` |
| EAPOL not captured | Promiscuous callback not set | Check `esp_wifi_set_promiscuous_rx_cb()` |
| State machine stuck | Missing `tick()` call | Add to main loop |
| Buffer overflow | Frame too large | Check `_validateFrameLength()` |

---

## 📞 Getting Help

1. **Serial output** - Check for `[ERROR]` or `[WARN]` tags
2. **State string** - Use `getStateString()` to diagnose position in state machine
3. **Error counters** - Track `getErrorCount()` over time
4. **Memory profiling** - Use `heap_caps_print_heap_info()` before/after operations
5. **Wireshark** - Capture 802.11 frames to verify format compliance

---

## 📋 Checklist: Before Lab Testing

- [ ] Initialize all modules in `setup()`
- [ ] Add `tick()` calls for frame builder & evil twin in `loop()`
- [ ] Test on isolated lab network only
- [ ] Verify frame sizes < 512 bytes
- [ ] Check NVS has space for credentials (min 10KB free)
- [ ] Serial monitor at 115200 baud
- [ ] Wireshark capture on test channel for verification
- [ ] Document test parameters (channel, SSID, duration)
- [ ] Save captured credentials before power cycle
- [ ] Review legal compliance for your jurisdiction

---

**Last Updated:** July 1, 2026 | OUROBOROS v2.0
