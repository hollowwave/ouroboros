#include <Arduino.h>
#include "config.h"
#include "ui/Display.h"
#include "ui/Menu.h"
#include "ui/Ouroboros.h"
#include "ui/ConfigScreen.h"
#include "utils/Buttons.h"
#include "modules/WiFiAttack.h"
#include "modules/WiFiMapper.h"
#include "modules/BLEModule.h"
#include "modules/SubGHz.h"
#include "modules/NVSConfig.h"
#include "modules/DeauthPicker.h"
#include "modules/RollingCodeDetector.h"
#include "modules/FrameConstructionModule.h"

// ─────────────────────────────────────────
//  Global instances
// ─────────────────────────────────────────
Display                   display;
Buttons                   buttons;
Ouroboros                 ouro(display);
Menu                      menu(display, buttons);
WiFiAttack                wifi(display);
WiFiMapper                mapper(display, buttons);
BLEModule                 ble(display);
SubGHz                    subghz(display);
NVSConfig                 nvs;
ConfigScreen              config(display, buttons, subghz);
DeauthPicker              deauthPicker(display, buttons, wifi);
RollingCodeDetector       rollingCode(display, buttons);
FrameConstructionModule   frameBuilder(display);

bool moduleRunning = false;

// ─────────────────────────────────────────
//  Frame Analysis State
// ─────────────────────────────────────────
enum class FrameAnalysisMode {
    IDLE,
    BEACON_TEST,
    AUTH_SEQUENCE,
    DEAUTH_BURST,
    PROBE_TEST,
    GATT_INTEGRATION
};

struct FrameAnalysisState {
    FrameAnalysisMode mode = FrameAnalysisMode::IDLE;
    uint32_t          start_time_ms = 0;
    uint8_t           target_channel = 6;
    uint8_t           test_sequence = 0;
    uint8_t           frame_count = 0;
    uint32_t          last_frame_ms = 0;
    bool              show_diagnostics = false;
};

FrameAnalysisState frameAnalysis;

// Test MAC addresses (use random on real deployment)
static const uint8_t TEST_AP_BSSID[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
static const uint8_t TEST_CLIENT_MAC[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

// ─────────────────────────────────────────
//  Stop all modules
// ─────────────────────────────────────────
void stopAll() {
    wifi.stopDeauth();
    wifi.stopBeaconSpam();
    wifi.stopProbeSniff();
    mapper.stop();
    ble.stopSpam();
    subghz.stopScan();
    subghz.stopCapture();
    frameBuilder.stop();
    frameAnalysis.mode = FrameAnalysisMode::IDLE;
    moduleRunning = false;	
}

// ─────────────────────────────────────────
//  Frame Analysis Routines
// ─────────────────────────────────────────

/**
 * Test Mode 1: Beacon Frame Construction & Transmission
 * Tests basic IEEE 802.11 beacon frame generation and timing
 */
void frameAnalysis_BeaconTest() {
    if (frameAnalysis.test_sequence == 0) {
        // Build beacon frame
        frameBuilder.buildBeaconFrame(
            TEST_AP_BSSID,
            "OUROBOROS_TEST",
            frameAnalysis.target_channel,
            100,      // beacon interval
            0x0401    // capabilities: ESS + Privacy
        );
        frameAnalysis.test_sequence = 1;
        frameAnalysis.last_frame_ms = millis();
    }

    // Transmit and repeat every 500ms (mimic beacon interval)
    if (millis() - frameAnalysis.last_frame_ms >= 500) {
        frameBuilder.transmitFrame(frameAnalysis.target_channel);
        frameAnalysis.frame_count++;
        frameAnalysis.last_frame_ms = millis();

        // Rotate channels after 3 beacons
        if (frameAnalysis.frame_count % 3 == 0) {
            frameAnalysis.target_channel++;
            if (frameAnalysis.target_channel > 13) frameAnalysis.target_channel = 1;
        }
    }
}

/**
 * Test Mode 2: IEEE 802.11 Authentication Sequence
 * Simulates full 4-way handshake probe → auth → assoc → deauth
 */
void frameAnalysis_AuthSequence() {
    uint32_t elapsed = millis() - frameAnalysis.start_time_ms;

    switch (frameAnalysis.test_sequence) {
        case 0:
            // Step 1: Probe Request (find AP)
            frameBuilder.buildProbeRequestFrame(TEST_CLIENT_MAC, "OUROBOROS_TEST");
            frameBuilder.transmitFrame(frameAnalysis.target_channel);
            frameAnalysis.test_sequence = 1;
            frameAnalysis.start_time_ms = millis();
            Serial.println("[Frame Analysis] Step 1: Probe Request sent");
            break;

        case 1:
            if (elapsed >= 100) {
                // Step 2: Authentication frame (Open System)
                frameBuilder.buildAuthenticationFrame(
                    TEST_AP_BSSID,
                    TEST_CLIENT_MAC,
                    0,    // auth_alg: Open System
                    1,    // auth_seq: 1
                    0     // status: Success
                );
                frameBuilder.transmitFrame(frameAnalysis.target_channel);
                frameAnalysis.test_sequence = 2;
                frameAnalysis.start_time_ms = millis();
                Serial.println("[Frame Analysis] Step 2: Authentication sent");
            }
            break;

        case 2:
            if (elapsed >= 100) {
                // Step 3: Association Request
                frameBuilder.buildAssociationRequestFrame(
                    TEST_AP_BSSID,
                    TEST_CLIENT_MAC,
                    "OUROBOROS_TEST",
                    0x0401
                );
                frameBuilder.transmitFrame(frameAnalysis.target_channel);
                frameAnalysis.test_sequence = 3;
                frameAnalysis.start_time_ms = millis();
                Serial.println("[Frame Analysis] Step 3: Association Request sent");
            }
            break;

        case 3:
            if (elapsed >= 100) {
                // Step 4: Deauthentication (terminate connection)
                frameBuilder.buildDeauthenticationFrame(
                    TEST_CLIENT_MAC,
                    TEST_AP_BSSID,
                    0x0007,  // Class 3 frame received from non-assoc STA
                    0
                );
                frameBuilder.transmitFrame(frameAnalysis.target_channel);
                frameAnalysis.test_sequence = 4;
                Serial.println("[Frame Analysis] Step 4: Deauthentication sent");
            }
            break;

        case 4:
            // Complete after deauth
            if (elapsed >= 200) {
                frameAnalysis.mode = FrameAnalysisMode::IDLE;
                Serial.println("[Frame Analysis] Auth Sequence Complete");
            }
            break;
    }

    frameAnalysis.frame_count++;
}

/**
 * Test Mode 3: Deauthentication Burst Attack Analysis
 * Sends repeated deauth frames to test protocol stability under stress
 */
void frameAnalysis_DeauthBurst() {
    uint32_t elapsed = millis() - frameAnalysis.start_time_ms;

    // Send burst of deauth frames every 50ms
    if (millis() - frameAnalysis.last_frame_ms >= 50) {
        frameBuilder.buildDeauthenticationFrame(
            (const uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF",  // broadcast (all clients)
            TEST_AP_BSSID,
            0x0007,
            0
        );
        frameBuilder.transmitFrame(frameAnalysis.target_channel);
        frameAnalysis.frame_count++;
        frameAnalysis.last_frame_ms = millis();
    }

    // 5-second burst window
    if (elapsed >= 5000) {
        frameAnalysis.mode = FrameAnalysisMode::IDLE;
        Serial.printf("[Frame Analysis] Deauth burst complete: %d frames sent\n", frameAnalysis.frame_count);
    }
}

/**
 * Test Mode 4: Probe Request Injection
 * Tests channel hopping with probe requests (discovery phase)
 */
void frameAnalysis_ProbeTest() {
    uint32_t elapsed = millis() - frameAnalysis.start_time_ms;

    // Send probe request every 200ms, rotating channels
    if (millis() - frameAnalysis.last_frame_ms >= 200) {
        frameBuilder.buildProbeRequestFrame(TEST_CLIENT_MAC, "OUROBOROS_TEST");
        frameBuilder.transmitFrame(frameAnalysis.target_channel);
        
        frameAnalysis.frame_count++;
        frameAnalysis.last_frame_ms = millis();

        // Rotate channels: 1→6→11→1 (common Wi-Fi channels)
        if (frameAnalysis.frame_count % 3 == 0) {
            frameAnalysis.target_channel = (frameAnalysis.target_channel == 1) ? 6 :
                                           (frameAnalysis.target_channel == 6) ? 11 : 1;
        }
    }

    // Run for 10 seconds
    if (elapsed >= 10000) {
        frameAnalysis.mode = FrameAnalysisMode::IDLE;
        Serial.printf("[Frame Analysis] Probe test complete: %d frames\n", frameAnalysis.frame_count);
    }
}

/**
 * Test Mode 5: GATT Characteristic Influence on Frame State
 * Simulates BLE characteristic updates affecting Wi-Fi frame assembly
 */
void frameAnalysis_GATTIntegration() {
    // Simulate BLE GATT characteristic updates
    GATTCharacteristic batteryLevel;
    batteryLevel.uuid = 0x2A19;        // Battery Level
    batteryLevel.value_length = 1;
    batteryLevel.value[0] = (uint8_t)(50 + (millis() / 100) % 50);  // Simulate changing battery

    frameBuilder.updateGATTCharacteristic(batteryLevel);

    // Build beacon frame whose state is influenced by GATT
    if (frameAnalysis.test_sequence == 0) {
        frameBuilder.buildBeaconFrame(
            TEST_AP_BSSID,
            "GATT_TEST",
            frameAnalysis.target_channel,
            100,
            0x0401
        );
        frameAnalysis.test_sequence = 1;
        frameAnalysis.start_time_ms = millis();
    }

    // Transmit every 1 second
    if (millis() - frameAnalysis.last_frame_ms >= 1000) {
        frameBuilder.transmitFrame(frameAnalysis.target_channel);
        frameAnalysis.frame_count++;
        frameAnalysis.last_frame_ms = millis();
    }

    // Run for 15 seconds
    if (millis() - frameAnalysis.start_time_ms >= 15000) {
        frameAnalysis.mode = FrameAnalysisMode::IDLE;
        Serial.printf("[Frame Analysis] GATT integration test complete\n");
    }
}

// ─────────────────────────────────────────
//  Display Frame Diagnostics
// ─────────────────────────────────────────
void displayFrameDiagnostics() {
    display.clear();
    display.drawStatusBar("Frame Analysis");

    int16_t y = STATUSBAR_H + 4;

    // Frame builder state
    display.drawText("State:", 2, y, 1, CLR_TEXT);
    display.drawTextRight(frameBuilder.getStateString(), y, CLR_ACCENT);
    y += MENU_ITEM_H;

    // Frame count
    char buf[24];
    snprintf(buf, sizeof(buf), "Frames: %lu", frameBuilder.getFrameCount());
    display.drawText(buf, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    // Transmit count
    snprintf(buf, sizeof(buf), "TX: %lu", frameBuilder.getTransmitCount());
    display.drawText(buf, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    // Error count
    snprintf(buf, sizeof(buf), "Errors: %lu", frameBuilder.getErrorCount());
    uint16_t errorColor = (frameBuilder.getErrorCount() > 0) ? CLR_DANGER : CLR_OK;
    display.drawText(buf, 2, y, 1, errorColor);
    y += MENU_ITEM_H;

    // Current test mode
    const char* modeStr = "";
    switch (frameAnalysis.mode) {
        case FrameAnalysisMode::BEACON_TEST:       modeStr = "Beacon Test"; break;
        case FrameAnalysisMode::AUTH_SEQUENCE:     modeStr = "Auth Seq"; break;
        case FrameAnalysisMode::DEAUTH_BURST:      modeStr = "Deauth Burst"; break;
        case FrameAnalysisMode::PROBE_TEST:        modeStr = "Probe Test"; break;
        case FrameAnalysisMode::GATT_INTEGRATION:  modeStr = "GATT Test"; break;
        default:                                    modeStr = "Idle"; break;
    }
    snprintf(buf, sizeof(buf), "Mode: %s", modeStr);
    display.drawText(buf, 2, y, 1, CLR_ACCENT);
    y += MENU_ITEM_H;

    // Test frame count
    snprintf(buf, sizeof(buf), "Test: %d frames", frameAnalysis.frame_count);
    display.drawText(buf, 2, y, 1, CLR_DIM);
    y += MENU_ITEM_H;

    // Channel info
    snprintf(buf, sizeof(buf), "Ch: %d", frameAnalysis.target_channel);
    display.drawText(buf, 2, y, 1, CLR_TEXT);

    // Hint bar
    display.drawHintBar("SELECT: Details | BACK: Menu");
}

// ─────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────
void setup() {
    Serial.begin(SERIAL_BAUD);
    Serial.println("\n[OUROBOROS] Booting...");

    display.begin();
    buttons.begin();
    nvs.begin();
    ble.begin();
    frameBuilder.begin();

    // Load saved Sub-GHz config
    SubGHzConfig cfg = nvs.load();

    if (!subghz.begin()) {
        Serial.println("[WARN] CC1101 not found");
    } else {
        subghz.setFrequency(cfg.frequency);
        subghz.setModulation(cfg.modulation);
    }

    // Static boot screen — no animation
    menu.begin();

    Serial.println("[OUROBOROS] Ready");
    Serial.println("[OUROBOROS] Frame Construction Module loaded");
}

// ─────────────────────────────────────────
//  Loop
// ─────────────────────────────────────────
void loop() {
    buttons.tick();
    Screen s = menu.currentScreen();

    // ── Boot screen — static, wait for keypress ──
    if (s == Screen::BOOT) {	
        menu.tick();
        return;
    }

    // ── Frame Analysis Screen (NEW) ──────────────
    if (s == Screen::FRAME_ANALYSIS || s == Screen::FRAME_ANALYSIS_BEACON ||
        s == Screen::FRAME_ANALYSIS_AUTH || s == Screen::FRAME_ANALYSIS_DEAUTH ||
        s == Screen::FRAME_ANALYSIS_PROBE || s == Screen::FRAME_ANALYSIS_GATT) {
        
        // Display diagnostics
        displayFrameDiagnostics();

        // Tick frame builder (non-blocking state machine)
        frameBuilder.tick();

        // Execute current analysis mode
        if (frameAnalysis.mode != FrameAnalysisMode::IDLE) {
            switch (frameAnalysis.mode) {
                case FrameAnalysisMode::BEACON_TEST:
                    frameAnalysis_BeaconTest();
                    break;
                case FrameAnalysisMode::AUTH_SEQUENCE:
                    frameAnalysis_AuthSequence();
                    break;
                case FrameAnalysisMode::DEAUTH_BURST:
                    frameAnalysis_DeauthBurst();
                    break;
                case FrameAnalysisMode::PROBE_TEST:
                    frameAnalysis_ProbeTest();
                    break;
                case FrameAnalysisMode::GATT_INTEGRATION:
                    frameAnalysis_GATTIntegration();
                    break;
                default:
                    break;
            }
        }

        // Button handling
        BtnEvent e = buttons.consume();
        
        if (e == BtnEvent::SELECT_LONG) {
            // Start current test
            if (frameAnalysis.mode == FrameAnalysisMode::IDLE) {
                // Determine which test to run based on current screen
                if (s == Screen::FRAME_ANALYSIS_BEACON) {
                    frameAnalysis.mode = FrameAnalysisMode::BEACON_TEST;
                } else if (s == Screen::FRAME_ANALYSIS_AUTH) {
                    frameAnalysis.mode = FrameAnalysisMode::AUTH_SEQUENCE;
                } else if (s == Screen::FRAME_ANALYSIS_DEAUTH) {
                    frameAnalysis.mode = FrameAnalysisMode::DEAUTH_BURST;
                } else if (s == Screen::FRAME_ANALYSIS_PROBE) {
                    frameAnalysis.mode = FrameAnalysisMode::PROBE_TEST;
                } else if (s == Screen::FRAME_ANALYSIS_GATT) {
                    frameAnalysis.mode = FrameAnalysisMode::GATT_INTEGRATION;
                }
                frameAnalysis.start_time_ms = millis();
                frameAnalysis.frame_count = 0;
                frameAnalysis.test_sequence = 0;
                moduleRunning = true;
                Serial.printf("[Main] Frame analysis started: %d\n", static_cast<int>(frameAnalysis.mode));
            } else {
                // Stop current test
                stopAll();
            }
        }

        if (e == BtnEvent::BACK) {
            stopAll();
            menu.forceScreen(Screen::MAIN_MENU);
        }

        if (e == BtnEvent::UP) {
            frameAnalysis.target_channel--;
            if (frameAnalysis.target_channel < 1) frameAnalysis.target_channel = 13;
        }

        if (e == BtnEvent::DOWN) {
            frameAnalysis.target_channel++;
            if (frameAnalysis.target_channel > 13) frameAnalysis.target_channel = 1;
        }

        return;
    }

    // ── Self-contained screens ───────────────────
    if (s == Screen::SUBGHZ_CONFIG) {
        config.tick();
        if (config.isDone()) {
            SubGHzConfig cfg;
            cfg.frequency  = subghz.getFrequency();
            cfg.modulation = 2;
            nvs.save(cfg);
            menu.forceScreen(Screen::SUBGHZ_MENU);
        }
        return;
    }

    if (s == Screen::SUBGHZ_ROLLING) {
        rollingCode.tick();
        if (rollingCode.isDone())
            menu.forceScreen(Screen::SUBGHZ_MENU);
        return;
    }

    if (s == Screen::WIFI_DEAUTH_PICKER) {
        deauthPicker.tick();
        if (deauthPicker.isDone()) {
            int8_t target = deauthPicker.target();
            if (target >= -1) {
                wifi.startDeauth(target);
                moduleRunning = true;
                menu.forceScreen(Screen::WIFI_DEAUTH);
            } else {
                menu.forceScreen(Screen::WIFI_MENU);
            }
        }
        return;
    }

    if (s == Screen::WIFI_MAPPER) {
        mapper.tick();
        if (mapper.isDone())
            menu.forceScreen(Screen::WIFI_MENU);
        return;
    }

    // ── Menu navigation ──────────────────────────
    menu.tick();
    s = menu.currentScreen();

    // ── Stop everything on screen change ─────────
    static Screen prevScreen = Screen::BOOT;
    if (s != prevScreen) {
        stopAll();
        prevScreen = s;

        switch (s) {
            case Screen::WIFI_SCAN:
                wifi.startScan(); break;
            case Screen::BLE_SCAN:
                ble.startScan(); break;
            case Screen::SUBGHZ_SCAN:
                subghz.startScan(); moduleRunning = true; break;
            case Screen::SUBGHZ_CAPTURE:
                subghz.startCapture(); moduleRunning = true; break;
            case Screen::SUBGHZ_CONFIG:
                config.enter(); break;
            case Screen::SUBGHZ_ROLLING:
                rollingCode.enter(subghz.getFrequency()); break;
            case Screen::WIFI_DEAUTH_PICKER:
                deauthPicker.enter(); break;
            case Screen::WIFI_MAPPER:
                mapper.enter(); break;
            default: break;
        }
    }

    // ── Button actions ───────────────────────────
    BtnEvent e = buttons.consume();

    if (e == BtnEvent::SELECT_LONG) {
        if (!moduleRunning) {
            switch (s) {
                case Screen::WIFI_BEACON_SPAM:
                    wifi.startBeaconSpam(); moduleRunning = true; break;
                case Screen::WIFI_PROBE_SNIFF:
                    wifi.startProbeSniff(); moduleRunning = true; break;
                case Screen::BLE_SPAM:
                    ble.startSpam(BLESpamType::APPLE); moduleRunning = true; break;
                case Screen::SUBGHZ_REPLAY:
                    subghz.replay(); break;
                default: break;
            }
        } else {
            stopAll();
        }
    }

    if (e == BtnEvent::SELECT) {
        if (s == Screen::WIFI_SCAN) wifi.startScan();
        if (s == Screen::BLE_SCAN)  ble.startScan();
    }

    if (e == BtnEvent::SELECT_DBL && s == Screen::WIFI_DEAUTH) {
        stopAll();
        menu.forceScreen(Screen::WIFI_DEAUTH_PICKER);
        deauthPicker.enter();
    }

    // ── Tick active modules ───────────────────────
    if (moduleRunning) {
        switch (s) {
            case Screen::WIFI_DEAUTH:      wifi.tickDeauth();      break;
            case Screen::WIFI_BEACON_SPAM: wifi.tickBeaconSpam();  break;
            case Screen::WIFI_PROBE_SNIFF: wifi.tickProbeSniff();  break;
            case Screen::BLE_SPAM:         ble.tickSpam();         break;
            case Screen::SUBGHZ_SCAN:      subghz.tickScan();      break;
            case Screen::SUBGHZ_CAPTURE:   subghz.tickCapture();   break;
            default: break;
        }
    }

    // ── Frame builder non-blocking tick ──────────
    frameBuilder.tick();

    // ── Ouroboros spinner on waiting screens ─────
    if (s == Screen::SUBGHZ_CAPTURE ||
        s == Screen::WIFI_PROBE_SNIFF) {
        ouro.tick();
        ouro.drawAt(SCREEN_W - 14, STATUSBAR_H + 14, 8, ouro.frame());
    }
}
