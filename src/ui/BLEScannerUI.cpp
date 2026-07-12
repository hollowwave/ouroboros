#include "BLEScannerUI.h"
#include "config.h"

// ─────────────────────────────────────────
//  BLE Scan Screen Implementation
// ─────────────────────────────────────────
BLEScanScreen::BLEScanScreen(Display& display, Buttons& buttons, BLESnifferJammer& jammer)
    : _display(display), _buttons(buttons), _jammer(jammer) {}

void BLEScanScreen::enter() {
    _selected_idx = 0;
    _needsRedraw = true;
    _last_update_ms = millis();
    _jammer.startSniffing();
    Serial.println("[BLEScanScreen] Entered - BLE scanning started");
}

void BLEScanScreen::tick() {
    // Redraw if needed or periodically
    if (_needsRedraw || (millis() - _last_update_ms >= 500)) {
        _redraw();
        _needsRedraw = false;
        _last_update_ms = millis();
    }

    _jammer.tick();

    BtnEvent e = _buttons.consume();

    int device_count = _jammer.getDiscoveredCount();

    if (e == BtnEvent::DOWN && _selected_idx < device_count - 1) {
        _selected_idx++;
        _needsRedraw = true;
    }

    if (e == BtnEvent::UP && _selected_idx > 0) {
        _selected_idx--;
        _needsRedraw = true;
    }

    if (e == BtnEvent::SELECT) {
        // Refresh scan manually
        Serial.println("[BLEScanScreen] Manual scan refresh");
        _needsRedraw = true;
    }

    if (e == BtnEvent::SELECT_LONG && device_count > 0) {
        // Select this device for jamming
        _done = true;
        Serial.printf("[BLEScanScreen] Device #%d selected for jamming\n", _selected_idx);
    }

    if (e == BtnEvent::BACK) {
        _jammer.stopSniffing();
        _done = true;
    }
}

void BLEScanScreen::_redraw() {
    _display.clear();
    _display.drawStatusBar("BLE Scanner");

    int device_count = _jammer.getDiscoveredCount();
    int16_t y = STATUSBAR_H + 4;

    if (device_count == 0) {
        _display.drawCenteredText("Scanning...", SCREEN_H / 2, CLR_DIM);
        char pkt_str[32];
        snprintf(pkt_str, sizeof(pkt_str), "Packets: %d", _jammer.getCapturedPacketCount());
        _display.drawCenteredText(pkt_str, SCREEN_H / 2 + 12, CLR_DIM);
        _display.drawHintBar("SELECT: Refresh | BACK: Exit");
        return;
    }

    const BLEPacketInfo* devices = _jammer.getDiscoveredDevices();

    // Show device list (max 5 per screen)
    for (int i = 0; i < (device_count > 5 ? 5 : device_count); i++) {
        const BLEPacketInfo& dev = devices[i];
        
        char label[40];
        const char* name = (dev.name[0] != '\0') ? dev.name : "[No Name]";
        snprintf(label, sizeof(label), "%.16s [%d]", name, dev.rssi);

        uint16_t color = (i == _selected_idx) ? CLR_ACCENT : CLR_TEXT;
        _display.drawText(label, 2, y, 1, color);

        // Draw MAC in smaller font on next line
        char mac_str[24];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                dev.addr[0], dev.addr[1], dev.addr[2],
                dev.addr[3], dev.addr[4], dev.addr[5]);
        _display.drawText(mac_str, 4, y + 8, 1, CLR_DIM);

        y += MENU_ITEM_H + 2;
    }

    // Footer with count
    char footer[40];
    snprintf(footer, sizeof(footer), "Devices: %d | Packets: %d",
            device_count, _jammer.getCapturedPacketCount());
    _display.drawText(footer, 2, SCREEN_H - 12, 1, CLR_DIM);

    _display.drawHintBar("UP/DOWN: Select | SELECT_LONG: Jam | BACK: Exit");
}

void BLEScanScreen::_drawDeviceDetails(int8_t idx) {
    if (idx < 0 || idx >= _jammer.getDiscoveredCount()) return;

    const BLEPacketInfo* devices = _jammer.getDiscoveredDevices();
    const BLEPacketInfo& dev = devices[idx];

    _display.clear();
    _display.drawStatusBar("Device Details");

    int16_t y = STATUSBAR_H + 4;

    // Name
    char name_str[40];
    snprintf(name_str, sizeof(name_str), "Name: %.20s", 
            (dev.name[0] != '\0') ? dev.name : "[Unknown]");
    _display.drawText(name_str, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    // MAC Address
    char mac_str[40];
    snprintf(mac_str, sizeof(mac_str), "MAC: %02X:%02X:%02X:%02X:%02X:%02X",
            dev.addr[0], dev.addr[1], dev.addr[2],
            dev.addr[3], dev.addr[4], dev.addr[5]);
    _display.drawText(mac_str, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    // Signal Strength
    char rssi_str[40];
    snprintf(rssi_str, sizeof(rssi_str), "RSSI: %d dBm", dev.rssi);
    _display.drawText(rssi_str, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    // Packet Count
    char pkt_str[40];
    snprintf(pkt_str, sizeof(pkt_str), "Packets: %u", dev.packetCount);
    _display.drawText(pkt_str, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    // Connectable status
    char conn_str[40];
    snprintf(conn_str, sizeof(conn_str), "Connectable: %s",
            dev.connectable ? "Yes" : "No");
    _display.drawText(conn_str, 2, y, 1, CLR_DIM);
}

// ─────────────────────────────────────────
//  BLE Jammer Screen Implementation
// ─────────────────────────────────────────
BLEJammerScreen::BLEJammerScreen(Display& display, Buttons& buttons, BLESnifferJammer& jammer)
    : _display(display), _buttons(buttons), _jammer(jammer) {}

void BLEJammerScreen::enter() {
    _done = false;
    _jammer_active = false;
    _start_time_ms = 0;

    if (_jam_mode == BLEJammerMode::JAM_BROADCAST) {
        _jammer.startBroadcastJam();
    } else if (_jam_mode == BLEJammerMode::JAM_SELECTIVE) {
        _jammer.startSelectiveJam(_target_mac);
    }

    _jammer_active = true;
    _start_time_ms = millis();
    Serial.println("[BLEJammerScreen] Jammer activated");
}

void BLEJammerScreen::setTargetMAC(const uint8_t* mac) {
    memcpy(_target_mac, mac, 6);
    _jam_mode = BLEJammerMode::JAM_SELECTIVE;
}

void BLEJammerScreen::tick() {
    _redraw();
    _jammer.tick();

    BtnEvent e = _buttons.consume();

    if (e == BtnEvent::SELECT_LONG) {
        // Stop jamming
        _jammer.stopJam();
        _jammer_active = false;
        _done = true;
        Serial.println("[BLEJammerScreen] Jammer stopped by user");
    }

    if (e == BtnEvent::BACK) {
        _jammer.stopJam();
        _jammer_active = false;
        _done = true;
    }

    if (e == BtnEvent::UP) {
        // Increase jam intensity
        uint8_t current = 100;  // Would read from jammer
        if (current < 100) {
            _jammer.setJamIntensity(current + 10);
        }
    }

    if (e == BtnEvent::DOWN) {
        // Decrease jam intensity
        uint8_t current = 100;
        if (current > 10) {
            _jammer.setJamIntensity(current - 10);
        }
    }
}

void BLEJammerScreen::_redraw() {
    _display.clear();
    
    const char* mode_str = (_jam_mode == BLEJammerMode::JAM_BROADCAST) ? 
                           "BLE Jammer - Broadcast" : "BLE Jammer - Selective";
    _display.drawStatusBar(mode_str);

    _drawJammerStats();
}

void BLEJammerScreen::_drawJammerStats() {
    int16_t y = STATUSBAR_H + 4;

    uint32_t elapsed = millis() - _start_time_ms;
    uint32_t seconds = elapsed / 1000;
    uint32_t minutes = seconds / 60;

    // Elapsed time
    char time_str[32];
    snprintf(time_str, sizeof(time_str), "Elapsed: %u:%02u", minutes, seconds % 60);
    _display.drawText(time_str, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    // Status
    uint16_t status_color = _jammer_active ? CLR_ACCENT : CLR_DIM;
    _display.drawText("Status: ACTIVE", 2, y, 1, status_color);
    y += MENU_ITEM_H;

    // Packets sent
    char pkt_str[32];
    snprintf(pkt_str, sizeof(pkt_str), "Packets: %lu", _jammer.getPacketsSent());
    _display.drawText(pkt_str, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    // Interruption attempts
    char int_str[32];
    snprintf(int_str, sizeof(int_str), "Attempts: %lu", _jammer.getInterruptionAttempts());
    _display.drawText(int_str, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    // Target info
    if (_jam_mode == BLEJammerMode::JAM_SELECTIVE) {
        char target_str[40];
        snprintf(target_str, sizeof(target_str), "Target: %02X:%02X:%02X:%02X:%02X:%02X",
                _target_mac[0], _target_mac[1], _target_mac[2],
                _target_mac[3], _target_mac[4], _target_mac[5]);
        _display.drawText(target_str, 2, y, 1, CLR_DIM);
        y += MENU_ITEM_H;
    }

    _display.drawHintBar("UP/DOWN: Intensity | SELECT_LONG: Stop | BACK: Exit");
}

// ─────────────────────────────────────────
//  BLE Stats Screen Implementation
// ─────────────────────────────────────────
BLEStatsScreen::BLEStatsScreen(Display& display, Buttons& buttons, BLESnifferJammer& jammer)
    : _display(display), _buttons(buttons), _jammer(jammer) {}

void BLEStatsScreen::enter() {
    _done = false;
    _stat_page = 0;
    Serial.println("[BLEStatsScreen] Entered");
}

void BLEStatsScreen::tick() {
    _redraw();

    BtnEvent e = _buttons.consume();

    if (e == BtnEvent::RIGHT || e == BtnEvent::DOWN) {
        _stat_page = (_stat_page + 1) % 3;
    }

    if (e == BtnEvent::LEFT || e == BtnEvent::UP) {
        _stat_page = (_stat_page == 0) ? 2 : _stat_page - 1;
    }

    if (e == BtnEvent::BACK) {
        _done = true;
    }
}

void BLEStatsScreen::_redraw() {
    _display.clear();
    _display.drawStatusBar("BLE Statistics");

    switch (_stat_page) {
        case 0:
            _drawSummary();
            break;
        case 1:
            _drawJamDetails();
            break;
        case 2:
            _drawDeviceList();
            break;
    }

    char page_str[16];
    snprintf(page_str, sizeof(page_str), "Page %d/3", _stat_page + 1);
    _display.drawText(page_str, SCREEN_W - 30, SCREEN_H - 10, 1, CLR_DIM);
}

void BLEStatsScreen::_drawSummary() {
    int16_t y = STATUSBAR_H + 4;

    // Mode
    char mode_str[32];
    snprintf(mode_str, sizeof(mode_str), "Mode: %s", _jammer.getModeString());
    _display.drawText(mode_str, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    // Devices discovered
    char dev_str[32];
    snprintf(dev_str, sizeof(dev_str), "Devices: %d", _jammer.getDiscoveredCount());
    _display.drawText(dev_str, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    // Total packets
    char pkt_str[32];
    snprintf(pkt_str, sizeof(pkt_str), "Packets: %d", _jammer.getCapturedPacketCount());
    _display.drawText(pkt_str, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    // Jammer status
    uint16_t jam_color = (_jammer.getMode() != BLEJammerMode::IDLE) ? CLR_OK : CLR_DIM;
    _display.drawText("Jammer: ", 2, y, 1, CLR_TEXT);
    _display.drawTextRight(_jammer.getMode() != BLEJammerMode::IDLE ? "ACTIVE" : "IDLE", y, jam_color);

    _display.drawHintBar("LEFT/RIGHT: Pages | BACK: Exit");
}

void BLEStatsScreen::_drawJamDetails() {
    int16_t y = STATUSBAR_H + 4;

    char mode_str[32];
    snprintf(mode_str, sizeof(mode_str), "Jam Mode: %s", _jammer.getModeString());
    _display.drawText(mode_str, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    char pkt_str[32];
    snprintf(pkt_str, sizeof(pkt_str), "Packets Sent: %lu", _jammer.getPacketsSent());
    _display.drawText(pkt_str, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    char att_str[32];
    snprintf(att_str, sizeof(att_str), "Attempts: %lu", _jammer.getInterruptionAttempts());
    _display.drawText(att_str, 2, y, 1, CLR_TEXT);
    y += MENU_ITEM_H;

    _display.drawText("Intensity: 100%", 2, y, 1, CLR_DIM);

    _display.drawHintBar("LEFT/RIGHT: Pages | BACK: Exit");
}

void BLEStatsScreen::_drawDeviceList() {
    int16_t y = STATUSBAR_H + 4;

    int count = _jammer.getDiscoveredCount();
    if (count == 0) {
        _display.drawCenteredText("No devices discovered", SCREEN_H / 2, CLR_DIM);
        _display.drawHintBar("LEFT/RIGHT: Pages | BACK: Exit");
        return;
    }

    const BLEPacketInfo* devices = _jammer.getDiscoveredDevices();

    for (int i = 0; i < (count > 4 ? 4 : count); i++) {
        const BLEPacketInfo& dev = devices[i];
        
        char label[40];
        const char* name = (dev.name[0] != '\0') ? dev.name : "[No Name]";
        snprintf(label, sizeof(label), "%.14s (%d)", name, dev.rssi);
        _display.drawText(label, 2, y, 1, CLR_TEXT);
        y += MENU_ITEM_H;
    }

    _display.drawHintBar("LEFT/RIGHT: Pages | BACK: Exit");
}
