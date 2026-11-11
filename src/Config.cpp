#include "Config.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace tsunami {

Config& cfg() {
    static Config c;
    return c;
}

void Config::load() {
    auto m = Mod::get();
    enabled       = m->getSavedValue<bool>("enabled", true);
    bufferTps     = std::clamp(static_cast<int>(m->getSavedValue<int64_t>("buffer-tps", 15)), 1, kPhysicsTps);
    predictive    = m->getSavedValue<bool>("predictive", true);
    deathRewind   = m->getSavedValue<bool>("death-rewind", true);
    lookahead     = std::clamp(static_cast<int>(m->getSavedValue<int64_t>("lookahead", 30)), 4, 240);
    budget        = std::clamp(static_cast<int>(m->getSavedValue<int64_t>("budget", 9000)), 500, 60000);
    showIndicator = m->getSavedValue<bool>("show-indicator", true);
    safeMode      = m->getSavedValue<bool>("safe-mode", false);
}

void Config::save() const {
    auto m = Mod::get();
    m->setSavedValue<bool>("enabled", enabled);
    m->setSavedValue<int64_t>("buffer-tps", bufferTps);
    m->setSavedValue<bool>("predictive", predictive);
    m->setSavedValue<bool>("death-rewind", deathRewind);
    m->setSavedValue<int64_t>("lookahead", lookahead);
    m->setSavedValue<int64_t>("budget", budget);
    m->setSavedValue<bool>("show-indicator", showIndicator);
    m->setSavedValue<bool>("safe-mode", safeMode);
}

} // namespace tsunami
