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

// ─────────────────────────────────────────
//  Global instances
// ─────────────────────────────────────────
Display              display;
Buttons              buttons;
Ouroboros            ouro(display);
Menu                 menu(display, buttons);
WiFiAttack           wifi(display);
WiFiMapper           mapper(display, buttons);
BLEModule            ble(display);
SubGHz               subghz(display);
NVSConfig            nvs;
ConfigScreen         config(display, buttons, subghz);
DeauthPicker         deauthPicker(display, buttons, wifi);
RollingCodeDetector  rollingCode(display, buttons);

bool moduleRunning = false;

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
    moduleRunning = false;
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

    // ── Ouroboros spinner on waiting screens ─────
    if (s == Screen::SUBGHZ_CAPTURE ||
        s == Screen::WIFI_PROBE_SNIFF) {
        ouro.tick();
        ouro.drawAt(SCREEN_W - 14, STATUSBAR_H + 14, 8, ouro.frame());
    }
}
