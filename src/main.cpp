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
#include "modules/EvilTwinModule.h"

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
EvilTwinModule            evilTwin(display);

bool moduleRunning = false;

// ─────────────────────────────────────────
//  Evil Twin State
// ─────────────────────────────────────────
enum class EvilTwinWorkflow {
    IDLE,
    SCANNING_TARGETS,
    TARGET_SELECTED,
    RUNNING_ATTACK
};

struct EvilTwinWorkflowState {
    EvilTwinWorkflow phase = EvilTwinWorkflow::IDLE;
    int8_t           selected_target = -1;
    uint32_t         start_time_ms = 0;
    bool             capture_enabled = true;
};

EvilTwinWorkflowState evilTwinState;

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
    evilTwin.stop();
    frameAnalysis.mode = FrameAnalysisMode::IDLE;
    evilTwinState.phase = EvilTwinWorkflow::IDLE;
    moduleRunning = false;	
}

// ─────────────────────────────────────────
//  Evil Twin UI & Workflow
// ─────────────────────────────────────────

void displayEvilTwinScanResults() {
    display.clear();
    display.drawStatusBar("Evil Twin - Select Target");

    int scan_count = evilTwin.getScanCount();
    const TargetAPInfo* results = evilTwin.getScanResults();

    int16_t y = STATUSBAR_H + 4;

    if (scan_count == 0) {
        display.drawCenteredText("No APs found", SCREEN_H / 2, CLR_DIM);
        display.drawHintBar("UP/DOWN: Scroll | SELECT: Scan | BACK: Menu");
        return;
    }

    // Show up to 6 APs
    for (int i = 0; i < (scan_count > 6 ? 6 : scan_count); i++) {
        char label[32];
        snprintf(label, sizeof(label), "%.15s [%d]",
            results[i].ssid[0] ? results[i].ssid : "[Hidden]",
            results[i].rssi);

        uint16_t color = (i == evilTwinState.selected_target) ? CLR_ACCENT : CLR_TEXT;
        display.drawText(label, 2, y, 1, color);
        y += MENU_ITEM_H;
    }

    display.drawHintBar("UP/DOWN: Select | SELECT: Clone | BACK: Menu");
}

void displayEvilTwinRunning() {
    display.clear();
    display.drawStatusBar("Evil Twin Running");

    const TargetAPInfo* target = evilTwin.getSelectedTarget();
    int16_t y = STATUSBAR_H + 4;

    // Target info
    char buf[32];
    snprintf(buf, sizeof(buf), "SSID: %.20s", target->ssid);
    display.drawText(buf, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    // Channel
    snprintf(buf, sizeof(buf), "Channel: %d", target->channel);
    display.drawText(buf, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    // Frames sent
    snprintf(buf, sizeof(buf), "Beacons: %lu", evilTwin.getFramesSent());
    display.drawText(buf, 2, y, 1, CLR_ACCENT);
    y += MENU_ITEM_H;

    // Clients connected
    snprintf(buf, sizeof(buf), "Clients: %d", evilTwin.getConnectedClientCount());
    display.drawText(buf, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    // Credentials captured
    snprintf(buf, sizeof(buf), "Captured: %d", evilTwin.getCapturedCredentialCount());
    uint16_t credColor = (evilTwin.getCapturedCredentialCount() > 0) ? CLR_OK : CLR_DIM;
    display.drawText(buf, 2, y, 1, credColor);
    y += MENU_ITEM_H;

    // Packets captured
    snprintf(buf, sizeof(buf), "Packets: %lu", evilTwin.getPacketsCaptured());
    display.drawText(buf, 2, y, 1, CLR_DIM);

    display.drawHintBar("SELECT_LONG: Stop | BACK: Menu");
}

void displayEvilTwinCredentials() {
    display.clear();
    display.drawStatusBar("Captured Credentials");

    int cred_count = evilTwin.getCapturedCredentialCount();
    const CapturedCredential* creds = evilTwin.getCapturedCredentials();

    int16_t y = STATUSBAR_H + 4;

    if (cred_count == 0) {
        display.drawCenteredText("No credentials captured", SCREEN_H / 2, CLR_DIM);
        display.drawHintBar("BACK: Menu");
        return;
    }

    // Show last 6 captures
    for (int i = 0; i < (cred_count > 6 ? 6 : cred_count); i++) {
        const CapturedCredential& cred = creds[i];
        
        char label[32];
        snprintf(label, sizeof(label), "%s", cred.username);
        display.drawText(label, 2, y, 1, CLR_ACCENT);
        y += MENU_ITEM_H;
    }

    char totalStr[16];
    snprintf(totalStr, sizeof(totalStr), "Total: %d", cred_count);
    display.drawText(totalStr, 2, SCREEN_H - 14, 1, CLR_DIM);

    display.drawHintBar("SELECT: Details | BACK: Menu");
}

// ─────────────────────────────────────────
//  Frame Analysis Routines (existing code)
// ─────────────────────────────────────────

void frameAnalysis_BeaconTest() {
    if (frameAnalysis.test_sequence == 0) {
        frameBuilder.buildBeaconFrame(
            TEST_AP_BSSID,
            "OUROBOROS_TEST",
            frameAnalysis.target_channel,
            100,
            0x0401
        );
        frameAnalysis.test_sequence = 1;
        frameAnalysis.last_frame_ms = millis();
    }

    if (millis() - frameAnalysis.last_frame_ms >= 500) {
        frameBuilder.transmitFrame(frameAnalysis.target_channel);
        frameAnalysis.frame_count++;
        frameAnalysis.last_frame_ms = millis();

        if (frameAnalysis.frame_count % 3 == 0) {
            frameAnalysis.target_channel++;
            if (frameAnalysis.target_channel > 13) frameAnalysis.target_channel = 1;
        }
    }
}

void frameAnalysis_AuthSequence() {
    uint32_t elapsed = millis() - frameAnalysis.start_time_ms;

    switch (frameAnalysis.test_sequence) {
        case 0:
            frameBuilder.buildProbeRequestFrame(TEST_CLIENT_MAC, "OUROBOROS_TEST");
            frameBuilder.transmitFrame(frameAnalysis.target_channel);
            frameAnalysis.test_sequence = 1;
            frameAnalysis.start_time_ms = millis();
            Serial.println("[Frame Analysis] Step 1: Probe Request sent");
            break;

        case 1:
            if (elapsed >= 100) {
                frameBuilder.buildAuthenticationFrame(
                    TEST_AP_BSSID,
                    TEST_CLIENT_MAC,
                    0, 1, 0
                );
                frameBuilder.transmitFrame(frameAnalysis.target_channel);
                frameAnalysis.test_sequence = 2;
                frameAnalysis.start_time_ms = millis();
                Serial.println("[Frame Analysis] Step 2: Authentication sent");
            }
            break;

        case 2:
            if (elapsed >= 100) {
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
                frameBuilder.buildDeauthenticationFrame(
                    TEST_CLIENT_MAC,
                    TEST_AP_BSSID,
                    0x0007, 0
                );
                frameBuilder.transmitFrame(frameAnalysis.target_channel);
                frameAnalysis.test_sequence = 4;
                Serial.println("[Frame Analysis] Step 4: Deauthentication sent");
            }
            break;

        case 4:
            if (elapsed >= 200) {
                frameAnalysis.mode = FrameAnalysisMode::IDLE;
                Serial.println("[Frame Analysis] Auth Sequence Complete");
            }
            break;
    }

    frameAnalysis.frame_count++;
}

void frameAnalysis_DeauthBurst() {
    uint32_t elapsed = millis() - frameAnalysis.start_time_ms;

    if (millis() - frameAnalysis.last_frame_ms >= 50) {
        frameBuilder.buildDeauthenticationFrame(
            (const uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF",
            TEST_AP_BSSID,
            0x0007, 0
        );
        frameBuilder.transmitFrame(frameAnalysis.target_channel);
        frameAnalysis.frame_count++;
        frameAnalysis.last_frame_ms = millis();
    }

    if (elapsed >= 5000) {
        frameAnalysis.mode = FrameAnalysisMode::IDLE;
        Serial.printf("[Frame Analysis] Deauth burst complete: %d frames sent\n", frameAnalysis.frame_count);
    }
}

void frameAnalysis_ProbeTest() {
    uint32_t elapsed = millis() - frameAnalysis.start_time_ms;

    if (millis() - frameAnalysis.last_frame_ms >= 200) {
        frameBuilder.buildProbeRequestFrame(TEST_CLIENT_MAC, "OUROBOROS_TEST");
        frameBuilder.transmitFrame(frameAnalysis.target_channel);
        
        frameAnalysis.frame_count++;
        frameAnalysis.last_frame_ms = millis();

        if (frameAnalysis.frame_count % 3 == 0) {
            frameAnalysis.target_channel = (frameAnalysis.target_channel == 1) ? 6 :
                                           (frameAnalysis.target_channel == 6) ? 11 : 1;
        }
    }

    if (elapsed >= 10000) {
        frameAnalysis.mode = FrameAnalysisMode::IDLE;
        Serial.printf("[Frame Analysis] Probe test complete: %d frames\n", frameAnalysis.frame_count);
    }
}

void frameAnalysis_GATTIntegration() {
    GATTCharacteristic batteryLevel;
    batteryLevel.uuid = 0x2A19;
    batteryLevel.value_length = 1;
    batteryLevel.value[0] = (uint8_t)(50 + (millis() / 100) % 50);

    frameBuilder.updateGATTCharacteristic(batteryLevel);

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

    if (millis() - frameAnalysis.last_frame_ms >= 1000) {
        frameBuilder.transmitFrame(frameAnalysis.target_channel);
        frameAnalysis.frame_count++;
        frameAnalysis.last_frame_ms = millis();
    }

    if (millis() - frameAnalysis.start_time_ms >= 15000) {
        frameAnalysis.mode = FrameAnalysisMode::IDLE;
        Serial.printf("[Frame Analysis] GATT integration test complete\n");
    }
}

void displayFrameDiagnostics() {
    display.clear();
    display.drawStatusBar("Frame Analysis");

    int16_t y = STATUSBAR_H + 4;

    display.drawText("State:", 2, y, 1, CLR_TEXT);
    display.drawTextRight(frameBuilder.getStateString(), y, CLR_ACCENT);
    y += MENU_ITEM_H;

    char buf[24];
    snprintf(buf, sizeof(buf), "Frames: %lu", frameBuilder.getFrameCount());
    display.drawText(buf, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    snprintf(buf, sizeof(buf), "TX: %lu", frameBuilder.getTransmitCount());
    display.drawText(buf, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    snprintf(buf, sizeof(buf), "Errors: %lu", frameBuilder.getErrorCount());
    uint16_t errorColor = (frameBuilder.getErrorCount() > 0) ? CLR_DANGER : CLR_OK;
    display.drawText(buf, 2, y, 1, errorColor);
    y += MENU_ITEM_H;

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

    snprintf(buf, sizeof(buf), "Test: %d frames", frameAnalysis.frame_count);
    display.drawText(buf, 2, y, 1, CLR_DIM);
    y += MENU_ITEM_H;

    snprintf(buf, sizeof(buf), "Ch: %d", frameAnalysis.target_channel);
    display.drawText(buf, 2, y, 1, CLR_TEXT);

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
    evilTwin.begin();

    SubGHzConfig cfg = nvs.load();

    if (!subghz.begin()) {
        Serial.println("[WARN] CC1101 not found");
    } else {
        subghz.setFrequency(cfg.frequency);
        subghz.setModulation(cfg.modulation);
    }

    menu.begin();

    Serial.println("[OUROBOROS] Ready");
    Serial.println("[OUROBOROS] Frame Construction Module loaded");
    Serial.println("[OUROBOROS] Evil Twin Module loaded");
}

// ─────────────────────────────────────────
//  Loop
// ─────────────────────────────────────────
void loop() {
    buttons.tick();
    Screen s = menu.currentScreen();

    // ── Boot screen ──────────────────────────────
    if (s == Screen::BOOT) {	
        menu.tick();
        return;
    }

    // ── Evil Twin Screens (NEW) ──────────────────
    if (s == Screen::EVIL_TWIN_SCAN) {
        displayEvilTwinScanResults();
        evilTwin.tick();

        BtnEvent e = buttons.consume();

        if (e == BtnEvent::SELECT) {
            if (evilTwinState.phase == EvilTwinWorkflow::SCANNING_TARGETS) {
                evilTwin.startScan();
                evilTwinState.phase = EvilTwinWorkflow::TARGET_SELECTED;
            }
        }

        if (e == BtnEvent::DOWN && evilTwinState.selected_target < evilTwin.getScanCount() - 1) {
            evilTwinState.selected_target++;
        }

        if (e == BtnEvent::UP && evilTwinState.selected_target > 0) {
            evilTwinState.selected_target--;
        }

        if (e == BtnEvent::SELECT_LONG && evilTwinState.selected_target >= 0) {
            evilTwin.selectTarget(evilTwinState.selected_target);
            evilTwin.startEvilTwin(EvilTwinMode::OPEN_NETWORK);
            evilTwinState.phase = EvilTwinWorkflow::RUNNING_ATTACK;
            moduleRunning = true;
            menu.forceScreen(Screen::EVIL_TWIN_RUNNING);
        }

        if (e == BtnEvent::BACK) {
            stopAll();
            menu.forceScreen(Screen::MAIN_MENU);
        }

        return;
    }

    if (s == Screen::EVIL_TWIN_RUNNING) {
        displayEvilTwinRunning();
        evilTwin.tick();

        BtnEvent e = buttons.consume();

        if (e == BtnEvent::SELECT_LONG) {
            evilTwin.stopEvilTwin();
            evilTwinState.phase = EvilTwinWorkflow::IDLE;
            moduleRunning = false;
            menu.forceScreen(Screen::EVIL_TWIN_SCAN);
        }

        if (e == BtnEvent::BACK) {
            stopAll();
            menu.forceScreen(Screen::MAIN_MENU);
        }

        return;
    }

    if (s == Screen::EVIL_TWIN_CREDENTIALS) {
        displayEvilTwinCredentials();
        evilTwin.tick();

        BtnEvent e = buttons.consume();

        if (e == BtnEvent::BACK) {
            menu.forceScreen(Screen::MAIN_MENU);
        }

        return;
    }

    // ── Frame Analysis Screen ────────────────────
    if (s == Screen::FRAME_ANALYSIS || s == Screen::FRAME_ANALYSIS_BEACON ||
        s == Screen::FRAME_ANALYSIS_AUTH || s == Screen::FRAME_ANALYSIS_DEAUTH ||
        s == Screen::FRAME_ANALYSIS_PROBE || s == Screen::FRAME_ANALYSIS_GATT) {
        
        displayFrameDiagnostics();
        frameBuilder.tick();

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

        BtnEvent e = buttons.consume();
        
        if (e == BtnEvent::SELECT_LONG) {
            if (frameAnalysis.mode == FrameAnalysisMode::IDLE) {
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
            case Screen::EVIL_TWIN_SCAN:
                evilTwinState.phase = EvilTwinWorkflow::SCANNING_TARGETS;
                evilTwinState.selected_target = 0;
                break;
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

    // ── Non-blocking ticks ───────────────────────
    frameBuilder.tick();
    evilTwin.tick();

    // ── Ouroboros spinner on waiting screens ─────
    if (s == Screen::SUBGHZ_CAPTURE ||
        s == Screen::WIFI_PROBE_SNIFF) {
        ouro.tick();
        ouro.drawAt(SCREEN_W - 14, STATUSBAR_H + 14, 8, ouro.frame());
    }
}
