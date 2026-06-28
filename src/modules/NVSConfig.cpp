#include "NVSConfig.h"

void NVSConfig::begin() {
    _prefs.begin(NS, false);
    Serial.println("[NVS] Initialized");
}

void NVSConfig::save(const SubGHzConfig& cfg) {
    _prefs.begin(NS, false);
    _prefs.putFloat("freq",  cfg.frequency);
    _prefs.putUChar("mod",   cfg.modulation);
    _prefs.putUChar("bw",    cfg.bandwidth);
    _prefs.putUChar("pwr",   cfg.txPower);
    _prefs.putUChar("pkt",   cfg.pktLength);
    _prefs.end();
    Serial.printf("[NVS] Saved: %.2f MHz mod=%d bw=%d pwr=%d pkt=%d\n",
        cfg.frequency, cfg.modulation, cfg.bandwidth, cfg.txPower, cfg.pktLength);
}

SubGHzConfig NVSConfig::load() {
    SubGHzConfig cfg;
    _prefs.begin(NS, true);   // read-only
    cfg.frequency  = _prefs.getFloat("freq",  433.92f);
    cfg.modulation = _prefs.getUChar("mod",   2);
    cfg.bandwidth  = _prefs.getUChar("bw",    0);
    cfg.txPower    = _prefs.getUChar("pwr",   4);
    cfg.pktLength  = _prefs.getUChar("pkt",   61);
    _prefs.end();
    Serial.printf("[NVS] Loaded: %.2f MHz mod=%d bw=%d pwr=%d pkt=%d\n",
        cfg.frequency, cfg.modulation, cfg.bandwidth, cfg.txPower, cfg.pktLength);
    return cfg;
}

void NVSConfig::reset() {
    _prefs.begin(NS, false);
    _prefs.clear();
    _prefs.end();
    Serial.println("[NVS] Reset to defaults");
}
