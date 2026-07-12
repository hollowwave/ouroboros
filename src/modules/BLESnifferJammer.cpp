// ─────────────────────────────────────────
//  Static Callback (required for ESP32 BLE)
// ─────────────────────────────────────────
void BLESnifferJammer::_blePacketCallback(esp_gap_ble_cb_event_t event, 
                                          esp_ble_gap_cb_param_t* param) {
    if (!_instance) return;
    
    switch (event) {
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            // Access scan result directly from param
            if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                _instance->_handleBLEPacket(
                    param->scan_rst.bda,
                    param->scan_rst.rssi,
                    param->scan_rst.ble_adv,
                    param->scan_rst.adv_data_len
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
