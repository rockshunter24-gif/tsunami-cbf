#include "Engine.hpp"
#include "Config.hpp"

#include <algorithm>
#include <climits>
#include <cmath>

using namespace geode::prelude;

namespace tsunami {

namespace {
constexpr int kJump = 1;
constexpr int kRewindMargin = 12; // ticks past "now" a rewind candidate must survive

struct Flag {
    bool& f; bool old;
    explicit Flag(bool& flag) : f(flag), old(flag) { f = true; }
    ~Flag() { f = old; }
};

int laneOf(bool p1) { return p1 ? 0 : 1; }
}

Engine& Engine::get() {
    static Engine e;
    return e;
}

// ---------------------------------------------------------------- reset

void Engine::reset() {
    m_owner = nullptr;
    m_tick = 0;
    m_stepInFrame = 0;
    m_curFrame.clear();
    m_framePattern.clear();
    m_win = Window{};
    m_sched.clear();
    m_seenTimestamps.clear();
    m_skipLogPress[0] = m_skipLogPress[1] = false;
    m_skipLogRelease[0] = m_skipLogRelease[1] = false;
    m_lastPressTick[0] = m_lastPressTick[1] = -1;
    m_deathDeferred = false;
    m_deathPlayer = nullptr;
    m_deathObject = nullptr;
}

// ---------------------------------------------------------------- state capture

void Engine::capturePlayer(PlayerObject* p, PlayerExtra& e) {
    if (!p) return;
    e.holding.clear();
    for (auto const& [k, v] : p->m_holdingButtons) e.holding.emplace_back(k, v);
    e.holdingLeft = p->m_holdingLeft;
    e.holdingRight = p->m_holdingRight;
    e.jumpBuffered = p->m_jumpBuffered;
    e.stateJumpBuffered = p->m_stateJumpBuffered;
    e.ringJumpRelated = p->m_ringJumpRelated;
    e.stateRingJump = p->m_stateRingJump;
    e.stateRingJump2 = p->m_stateRingJump2;
    e.touchedRing = p->m_touchedRing;
    e.touchedCustomRing = p->m_touchedCustomRing;
    e.isDead = p->m_isDead;
    e.touchingRings.clear();
    if (p->m_touchingRings) {
        for (auto obj : CCArrayExt<CCObject*>(p->m_touchingRings)) e.touchingRings.emplace_back(obj);
    }
    e.touchedRings.clear();
    for (int id : p->m_touchedRings) e.touchedRings.push_back(id);
}

void Engine::applyPlayer(PlayerObject* p, PlayerExtra const& e) {
    if (!p) return;
    p->m_holdingButtons.clear();
    for (auto const& [k, v] : e.holding) p->m_holdingButtons[k] = v;
    p->m_holdingLeft = e.holdingLeft;
    p->m_holdingRight = e.holdingRight;
    p->m_jumpBuffered = e.jumpBuffered;
    p->m_stateJumpBuffered = e.stateJumpBuffered;
    p->m_ringJumpRelated = e.ringJumpRelated;
    p->m_stateRingJump = e.stateRingJump;
    p->m_stateRingJump2 = e.stateRingJump2;
    p->m_touchedRing = e.touchedRing;
    p->m_touchedCustomRing = e.touchedCustomRing;
    p->m_isDead = e.isDead;
    if (p->m_touchingRings) {
        p->m_touchingRings->removeAllObjects();
        for (auto const& obj : e.touchingRings) p->m_touchingRings->addObject(obj.data());
    }
    p->m_touchedRings.clear();
    for (int id : e.touchedRings) p->m_touchedRings.insert(id);
}

void Engine::captureLayer(PlayLayer* pl, LayerExtra& e) {
    e.timestamp = pl->m_timestamp;
    e.clicks = pl->m_clicks;
    e.playerDied = pl->m_playerDied;
    e.isBetweenSteps = pl->m_isBetweenSteps;
    e.queue.assign(pl->m_queuedButtons.begin(), pl->m_queuedButtons.end());
}

void Engine::applyLayer(PlayLayer* pl, LayerExtra const& e) {
    pl->m_timestamp = e.timestamp;
    pl->m_clicks = e.clicks;
    pl->m_playerDied = e.playerDied;
    pl->m_isBetweenSteps = e.isBetweenSteps;
    pl->m_queuedButtons.clear();
    for (auto const& c : e.queue) pl->m_queuedButtons.push_back(c);
}

void Engine::capture(PlayLayer* pl, Snapshot& s) {
    s.cp = pl->createCheckpoint();
    captureLayer(pl, s.layer);
    capturePlayer(pl->m_player1, s.p1);
    capturePlayer(pl->m_player2, s.p2);
}

void Engine::restore(PlayLayer* pl, Snapshot& s) {
    if (!s.valid()) return;
    {
        Flag f(m_simulating); // keep any sounds / deaths triggered by the load silent
        pl->loadFromCheckpoint(s.cp);
    }
    applyLayer(pl, s.layer);
    applyPlayer(pl->m_player1, s.p1);
    applyPlayer(pl->m_player2, s.p2);
}

// ---------------------------------------------------------------- simulation

StepTiming Engine::timingAt(Window const& w, int k) const {
    if (k >= 0 && k < static_cast<int>(w.timing.size())) return w.timing[k];
    if (!m_framePattern.empty()) {
        int n = static_cast<int>(m_framePattern.size());
        return m_framePattern[(w.phase + k) % n];
    }
    if (!w.timing.empty()) {
        auto t = w.timing.back();
        t.half = false;
        t.last = true;
        return t;
    }
    return StepTiming{};
}

void Engine::inject(PlayLayer* pl, Ev const& e) {
    Flag f(m_injecting);
    pl->handleButton(e.down, e.button, e.p1);
}

bool Engine::simulate(PlayLayer* pl, Window& w, std::vector<Ev> const& plan, int ticks, int& budget) {
    restore(pl, w.snap);
    pl->m_queuedButtons.clear();

    Flag f(m_simulating);
    m_simDied = false;
    m_simComplete = false;

    size_t ei = 0;
    for (int k = 0; k < ticks; ++k) {
        if (budget <= 0) return false; // ran out of time: treat as unsafe
        while (ei < plan.size() && plan[ei].tick <= k) inject(pl, plan[ei++]);

        auto t = timingAt(w, k);
#if GEODE_COMP_GD_VERSION >= 22081
        pl->processCommands(t.dt, t.half, t.last);
#else
        pl->processCommands(t.dt);
#endif
        --budget;

        if (m_simDied) return false;
        if (m_simComplete) return true;
    }
    return true;
}

// Turns real inputs into a plan where the window's click happens `delay` ticks late.
// Later inputs keep their real timing unless that would put them before the input
// in front of them, in which case they keep their original spacing instead.
std::vector<Ev> Engine::buildPlan(Window const& w, std::vector<Ev> const& raw, int delay) const {
    std::vector<Ev> out;
    out.reserve(raw.size());

    bool first = true;
    int prevOut = 0, prevRaw = 0;
    for (auto const& e : raw) {
        bool lane = !e.fixed && e.button == kJump && laneOf(e.p1) == w.lane;
        if (!lane) {
            out.push_back(e);
            continue;
        }
        Ev n = e;
        if (first) {
            n.tick = delay;
            first = false;
        }
        else {
            n.tick = e.tick > prevOut ? e.tick : prevOut + std::max(1, e.tick - prevRaw);
        }
        prevOut = n.tick;
        prevRaw = e.tick;
        out.push_back(n);
    }
    std::stable_sort(out.begin(), out.end(), [](Ev const& a, Ev const& b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        return a.fixed && !b.fixed;
    });
    return out;
}

// Finds the smallest delay (1..window) that survives `ticks` simulated steps.
int Engine::searchDelay(PlayLayer* pl, Window& w, std::vector<Ev> const& raw, int ticks, int skip, int& budget) {
    int W = cfg().windowTicks();
    int maxCandidates = std::max(1, budget / std::max(1, ticks));
    int stride = std::max(1, static_cast<int>(std::ceil(static_cast<double>(W) / maxCandidates)));

    int found = -1;
    for (int d = 1; d <= W && budget > 0; d += stride) {
        if (d == skip) continue;
        if (simulate(pl, w, buildPlan(w, raw, d), ticks, budget)) { found = d; break; }
    }
    // coarse search hit: refine to the earliest safe tick
    if (found > 1 && stride > 1) {
        for (int d = std::max(1, found - stride + 1); d < found && budget > 0; ++d) {
            if (d == skip) continue;
            if (simulate(pl, w, buildPlan(w, raw, d), ticks, budget)) { found = d; break; }
        }
    }
    return found;
}

// ---------------------------------------------------------------- scheduling

bool Engine::laneBusy(int lane) const {
    for (auto const& e : m_sched) {
        if (e.button == kJump && laneOf(e.p1) == lane && e.tick > m_tick) return true;
    }
    return false;
}

void Engine::schedule(Ev const& e) {
    auto it = std::upper_bound(m_sched.begin(), m_sched.end(), e, [](Ev const& a, Ev const& b) {
        return a.tick < b.tick;
    });
    m_sched.insert(it, e);
}

void Engine::scheduleAfter(int lane, bool down, int button, bool p1, int rawTick) {
    int prevOut = m_tick, prevRaw = rawTick;
    for (auto const& e : m_sched) {
        if (e.button == kJump && laneOf(e.p1) == lane && e.tick >= prevOut) {
            prevOut = e.tick;
            prevRaw = e.origTick;
        }
    }
    Ev n;
    n.down = down;
    n.button = button;
    n.p1 = p1;
    n.origTick = rawTick;
    n.tick = rawTick > prevOut ? rawTick : prevOut + std::max(1, rawTick - prevRaw);
    if (n.tick <= m_tick) n.tick = m_tick + 1;
    schedule(n);
}

void Engine::fireScheduled(PlayLayer* pl) {
    while (!m_sched.empty() && m_sched.front().tick <= m_tick) {
        Ev e = m_sched.front();
        m_sched.erase(m_sched.begin());
        inject(pl, e);
    }
}

void Engine::logRaw(bool down, int button, bool p1, bool fixed) {
    if (!m_win.active) return;
    Ev e;
    e.tick = m_tick - m_win.t0;
    e.down = down;
    e.button = button;
    e.p1 = p1;
    e.origTick = m_tick;
    e.fixed = fixed;
    m_win.raw.push_back(e);
}

void Engine::noteHold(int lane, int holdTicks) {
    (void)lane;
    holdTicks = std::clamp(holdTicks, 1, 120);
    m_avgHold = m_avgHold * 0.7 + holdTicks * 0.3;
}

// ---------------------------------------------------------------- steps

void Engine::disabledStep(PlayLayer* pl) {
    // mod switched off mid-attempt: let anything we were holding back through now
    if (!m_sched.empty()) {
        m_tick = INT_MAX / 2;
        fireScheduled(pl);
    }
    reset();
}

void Engine::beforeStep(PlayLayer* pl, StepTiming const& t) {
    if (m_owner != pl) {
        reset();
        m_owner = pl;
    }
    ++m_tick;

    if (m_win.active) {
        if (m_tick > m_win.expire && !laneBusy(m_win.lane)) m_win = Window{};
        else m_win.timing.push_back(t);
    }

    fireScheduled(pl);
    evaluateQueue(pl, t);
}

void Engine::afterStep(PlayLayer* pl, StepTiming const& t) {
    m_curFrame.push_back(t);
    ++m_stepInFrame;
    if (t.last) {
        m_framePattern = m_curFrame;
        m_curFrame.clear();
        m_stepInFrame = 0;
    }
    if (m_deathDeferred) resolveDeath(pl);
}

// ---------------------------------------------------------------- clicks

void Engine::evaluateQueue(PlayLayer* pl, StepTiming const& t) {
    auto& q = pl->m_queuedButtons;

    // forget timestamps that are no longer queued
    std::erase_if(m_seenTimestamps, [&](double ts) {
        for (auto const& c : q) if (c.m_timestamp == ts) return false;
        return true;
    });

    for (size_t i = 0; i < q.size();) {
        PlayerButtonCommand c = q[i];
        bool isJump = static_cast<int>(c.m_button) == kJump;
        bool seen = std::find(m_seenTimestamps.begin(), m_seenTimestamps.end(), c.m_timestamp) != m_seenTimestamps.end();
        if (!c.m_isPush || !isJump || seen) { ++i; continue; }
        m_seenTimestamps.push_back(c.m_timestamp);

        bool p1 = !c.m_isPlayer2;
        int lane = laneOf(p1);

        // find this press's release if it's already queued too (a quick tap)
        int rel = -1;
        for (size_t j = i + 1; j < q.size(); ++j) {
            if (!q[j].m_isPush && q[j].m_button == c.m_button && q[j].m_isPlayer2 == c.m_isPlayer2) {
                rel = static_cast<int>(j);
                break;
            }
        }
        int knownHold = -1;
        if (rel >= 0) {
            double secs = q[rel].m_timestamp - c.m_timestamp;
            knownHold = std::clamp(static_cast<int>(std::lround(secs * kPhysicsTps)), 0, kPhysicsTps);
        }

        m_lastPressTick[lane] = m_tick;

        // an earlier click on this lane is still being held back: keep the order
        if (laneBusy(lane)) {
            if (rel >= 0) q.erase(q.begin() + rel);
            q.erase(q.begin() + i);
            scheduleAfter(lane, true, kJump, p1, m_tick);
            logRaw(true, kJump, p1);
            if (rel >= 0) {
                scheduleAfter(lane, false, kJump, p1, m_tick + std::max(1, knownHold));
                noteHold(lane, knownHold);
            }
            continue;
        }

        int delay = processPress(pl, t, p1, knownHold);
        if (delay > 0) {
            // hold the click back: pull it (and its release) out of GD's queue
            if (rel >= 0) q.erase(q.begin() + rel);
            q.erase(q.begin() + i);

            Ev press;
            press.tick = m_tick + delay;
            press.down = true;
            press.p1 = p1;
            press.origTick = m_tick;
            schedule(press);
            if (rel >= 0) {
                Ev r = press;
                r.down = false;
                r.origTick = m_tick + std::max(1, knownHold);
                r.tick = press.tick + std::max(1, knownHold);
                schedule(r);
                noteHold(lane, knownHold);
            }
            continue;
        }

        // click goes through untouched; it'll reach handleButton later this step
        m_skipLogPress[lane] = true;
        if (rel >= 0) m_skipLogRelease[lane] = true;
        ++i;
    }
}

int Engine::processPress(PlayLayer* pl, StepTiming const& t, bool p1, int knownHold) {
    auto const& c = cfg();
    if (!c.predictive && !c.deathRewind) return 0;

    int W = c.windowTicks();
    int H = W + c.lookahead;

    m_win = Window{};
    m_win.active = true;
    m_win.t0 = m_tick;
    m_win.lane = laneOf(p1);
    m_win.p1 = p1;
    m_win.phase = m_stepInFrame;
    m_win.expire = m_tick + H + 2;
    m_win.timing.push_back(t);
    capture(pl, m_win.snap);
    if (!m_win.snap.valid()) {
        m_win = Window{};
        return 0;
    }

    // other inputs already waiting in GD's queue are replayed untouched at tick 0
    bool skippedPress = false, skippedRelease = knownHold < 0;
    for (auto const& q : m_win.snap.layer.queue) {
        bool same = static_cast<int>(q.m_button) == kJump && q.m_isPlayer2 == !p1;
        if (same && q.m_isPush && !skippedPress) { skippedPress = true; continue; }
        if (same && !q.m_isPush && skippedPress && !skippedRelease) { skippedRelease = true; continue; }
        Ev e;
        e.tick = 0;
        e.down = q.m_isPush;
        e.button = static_cast<int>(q.m_button);
        e.p1 = !q.m_isPlayer2;
        e.origTick = m_tick;
        e.fixed = true;
        m_win.raw.push_back(e);
    }

    Ev press;
    press.tick = 0;
    press.down = true;
    press.p1 = p1;
    press.origTick = m_tick;
    m_win.raw.push_back(press);
    if (knownHold >= 0) {
        Ev r = press;
        r.down = false;
        r.tick = std::max(1, knownHold);
        r.origTick = m_tick + r.tick;
        m_win.raw.push_back(r);
    }

    if (!c.predictive) return 0;

    // the release hasn't happened yet: assume a tap as long as your usual ones
    std::vector<Ev> raw = m_win.raw;
    if (knownHold < 0) {
        Ev r = press;
        r.down = false;
        r.tick = std::max(1, static_cast<int>(std::lround(m_avgHold)));
        r.origTick = m_tick + r.tick;
        raw.push_back(r);
    }

    int budget = c.budget;
    int delay = 0;
    if (!simulate(pl, m_win, buildPlan(m_win, raw, 0), H, budget)) {
        int found = searchDelay(pl, m_win, raw, H, -1, budget);
        if (found > 0) delay = found;
    }

    restore(pl, m_win.snap);
    m_win.delay = delay;
    if (delay > 0) showIndicator(pl, fmt::format("+{} tick{}", delay, delay == 1 ? "" : "s"));
    return delay;
}

bool Engine::onButton(PlayLayer* pl, bool down, int button, bool p1) {
    (void)pl;
    int lane = laneOf(p1);

    if (button != kJump) {
        logRaw(down, button, p1);
        return false;
    }

    if (down) {
        if (m_skipLogPress[lane]) m_skipLogPress[lane] = false;
        else logRaw(true, button, p1);
    }
    else {
        if (m_lastPressTick[lane] >= 0) noteHold(lane, m_tick - m_lastPressTick[lane]);
        if (m_skipLogRelease[lane]) m_skipLogRelease[lane] = false;
        else logRaw(false, button, p1);
    }

    if (laneBusy(lane)) {
        scheduleAfter(lane, down, button, p1, m_tick);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- death rewind

bool Engine::onDestroyPlayer(PlayLayer* pl, PlayerObject* p, GameObject* obj) {
    if (obj && obj == pl->m_anticheatSpike) return false;
    if (m_simulating) {
        m_simDied = true;
        return true;
    }
    if (m_allowDeath) return false;

    auto const& c = cfg();
    if (!c.enabled || !c.deathRewind) return false;
    if (m_deathDeferred) return true;
    if (!m_win.active || m_win.rewindUsed || !m_win.snap.valid()) return false;
    if (m_tick - m_win.t0 > c.windowTicks() + c.lookahead) return false;

    // never rewind from inside GD's collision loop: finish the step first
    m_deathDeferred = true;
    m_deathPlayer = p;
    m_deathObject = obj;
    return true;
}

void Engine::resolveDeath(PlayLayer* pl) {
    m_deathDeferred = false;
    m_win.rewindUsed = true;

    int elapsed = m_tick - m_win.t0 + 1;
    int ticks = elapsed + kRewindMargin;
    int budget = cfg().budget;

    LayerExtra nowLayer;
    captureLayer(pl, nowLayer);
    Snapshot now;
    capture(pl, now);

    int found = -1;
    if (m_win.delay != 0 && simulate(pl, m_win, buildPlan(m_win, m_win.raw, 0), ticks, budget)) found = 0;
    if (found < 0) found = searchDelay(pl, m_win, m_win.raw, ticks, m_win.delay, budget);

    if (found < 0) {
        // nothing survives: put the world back exactly as it was and die for real
        restore(pl, now);
        Flag f(m_allowDeath);
        pl->destroyPlayer(m_deathPlayer, m_deathObject);
        return;
    }

    // replay the real ticks since the click with the corrected timing
    auto plan = buildPlan(m_win, m_win.raw, found);
    std::vector<Ev> past, future;
    for (auto const& e : plan) (e.tick < elapsed ? past : future).push_back(e);

    int replayBudget = elapsed + 1;
    simulate(pl, m_win, past, elapsed, replayBudget);

    nowLayer.playerDied = false;
    applyLayer(pl, nowLayer);

    std::erase_if(m_sched, [&](Ev const& e) {
        return e.button == kJump && laneOf(e.p1) == m_win.lane;
    });
    for (auto e : future) {
        e.tick += m_win.t0;
        schedule(e);
    }

    m_win.delay = found;
    showIndicator(pl, fmt::format("REWIND +{}", found));
}

// ---------------------------------------------------------------- indicator

void Engine::showIndicator(PlayLayer* pl, std::string const& text) {
    if (!cfg().showIndicator || !pl) return;
    auto id = "tsunami-indicator"_spr;
    auto label = typeinfo_cast<CCLabelBMFont*>(pl->getChildByID(id));
    if (!label) {
        label = CCLabelBMFont::create("", "bigFont.fnt");
        label->setID(id);
        label->setAnchorPoint({ 1.f, 1.f });
        label->setScale(0.35f);
        label->setColor({ 130, 215, 255 });
        auto win = CCDirector::get()->getWinSize();
        label->setPosition({ win.width - 6.f, win.height - 22.f });
        pl->addChild(label, 9999);
    }
    label->setString(("TSUNAMI " + text).c_str());
    label->stopAllActions();
    label->setOpacity(255);
    label->runAction(CCSequence::create(CCDelayTime::create(0.6f), CCFadeOut::create(0.4f), nullptr));
}

} // namespace tsunami
