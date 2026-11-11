#pragma once

#include <algorithm>
#include <cmath>

namespace tsunami {

// GD 2.2 runs physics at 240 steps ("ticks") per second.
constexpr int kPhysicsTps = 240;

struct Config {
    bool enabled       = true;
    int  bufferTps     = 15;    // 240 = 1 tick window (tightest) ... 1 = 240 tick window (1 full second)
    bool predictive    = true;  // simulate ahead the moment a click lands
    bool deathRewind   = true;  // if you still die inside the window, rewind and retry the click later
    int  lookahead     = 30;    // extra ticks simulated past the window to confirm a click is safe
    int  budget        = 9000;  // max simulated ticks per click (protects against stutter)
    bool showIndicator = true;
    bool safeMode      = false; // don't save progress on rated levels

    // Size of the buffer window in physics ticks.
    int windowTicks() const {
        int tps = std::clamp(bufferTps, 1, kPhysicsTps);
        return std::max(1, static_cast<int>(std::lround(static_cast<double>(kPhysicsTps) / tps)));
    }
    double windowMs() const { return windowTicks() * 1000.0 / kPhysicsTps; }

    void load();
    void save() const;
};

Config& cfg();

} // namespace tsunami
