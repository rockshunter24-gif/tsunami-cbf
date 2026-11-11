#pragma once

#include <Geode/Geode.hpp>

#include <string>
#include <vector>

namespace tsunami {

// One input event. `tick` is either absolute (scheduled events) or relative to a
// window's start tick (window logs / simulation plans) depending on context.
struct Ev {
    int  tick     = 0;
    bool down     = false;
    int  button   = 1;     // 1 = jump, 2 = left, 3 = right
    bool p1       = true;
    int  origTick = 0;     // when the player actually made this input
    bool fixed    = false; // never shifted (other inputs that were already queued)
};

struct StepTiming {
    float dt   = 0.f;
    bool  half = false;
    bool  last = true;
};

// Per-player state that GD's checkpoints don't store but that matters for clicks.
struct PlayerExtra {
    std::vector<std::pair<int, bool>> holding;
    bool holdingLeft = false;
    bool holdingRight = false;
    bool jumpBuffered = false;
    unsigned char stateJumpBuffered = 0;
    bool ringJumpRelated = false;
    bool stateRingJump = false;
    bool stateRingJump2 = false;
    bool touchedRing = false;
    bool touchedCustomRing = false;
    bool isDead = false;
    std::vector<geode::Ref<cocos2d::CCObject>> touchingRings;
    std::vector<int> touchedRings;
};

// Layer state that GD's checkpoints don't store.
struct LayerExtra {
    double timestamp = 0.0;
    int clicks = 0;
    bool playerDied = false;
    bool isBetweenSteps = false;
    std::vector<PlayerButtonCommand> queue;
};

struct Snapshot {
    geode::Ref<CheckpointObject> cp = nullptr;
    LayerExtra layer;
    PlayerExtra p1;
    PlayerExtra p2;
    bool valid() const { return cp != nullptr; }
};

// Everything we know about the most recent buffered click.
struct Window {
    bool active = false;
    int t0 = 0;              // absolute tick of the click
    int lane = 0;            // 0 = player 1, 1 = player 2
    bool p1 = true;
    int delay = 0;           // ticks the click is currently being delayed by
    int phase = 0;           // index of the t0 step inside its frame
    int expire = 0;
    bool rewindUsed = false;
    Snapshot snap;
    std::vector<Ev> raw;              // real inputs since t0 (relative ticks)
    std::vector<StepTiming> timing;   // real step timings since t0
};

class Engine {
public:
    static Engine& get();

    // ---- hook entry points ----
    void beforeStep(PlayLayer* pl, StepTiming const& t);
    void afterStep(PlayLayer* pl, StepTiming const& t);
    void disabledStep(PlayLayer* pl);
    bool onButton(PlayLayer* pl, bool down, int button, bool p1);              // true = swallowed
    bool onDestroyPlayer(PlayLayer* pl, PlayerObject* p, GameObject* obj);    // true = cancelled
    void reset();

    bool simulating() const { return m_simulating; }
    bool injecting() const { return m_injecting; }
    void markSimComplete() { m_simComplete = true; }

private:
    // state capture
    void capture(PlayLayer* pl, Snapshot& s);
    void restore(PlayLayer* pl, Snapshot& s);
    static void captureLayer(PlayLayer* pl, LayerExtra& e);
    static void applyLayer(PlayLayer* pl, LayerExtra const& e);
    static void capturePlayer(PlayerObject* p, PlayerExtra& e);
    static void applyPlayer(PlayerObject* p, PlayerExtra const& e);

    // simulation
    StepTiming timingAt(Window const& w, int k) const;
    bool simulate(PlayLayer* pl, Window& w, std::vector<Ev> const& plan, int ticks, int& budget);
    void inject(PlayLayer* pl, Ev const& e);
    std::vector<Ev> buildPlan(Window const& w, std::vector<Ev> const& raw, int delay) const;
    int searchDelay(PlayLayer* pl, Window& w, std::vector<Ev> const& raw, int ticks, int skip, int& budget);

    // input handling
    void evaluateQueue(PlayLayer* pl, StepTiming const& t);
    int  processPress(PlayLayer* pl, StepTiming const& t, bool p1, int knownHold);
    void resolveDeath(PlayLayer* pl);
    void fireScheduled(PlayLayer* pl);
    bool laneBusy(int lane) const;
    void schedule(Ev const& e);
    void scheduleAfter(int lane, bool down, int button, bool p1, int rawTick);
    void logRaw(bool down, int button, bool p1, bool fixed = false);
    void noteHold(int lane, int holdTicks);
    void showIndicator(PlayLayer* pl, std::string const& text);

    PlayLayer* m_owner = nullptr;
    int m_tick = 0;
    int m_stepInFrame = 0;
    std::vector<StepTiming> m_curFrame;
    std::vector<StepTiming> m_framePattern;

    Window m_win;
    std::vector<Ev> m_sched;           // absolute ticks, sorted
    std::vector<double> m_seenTimestamps;
    bool m_skipLogPress[2] = { false, false };
    bool m_skipLogRelease[2] = { false, false };
    int m_lastPressTick[2] = { -1, -1 };
    double m_avgHold = 10.0;

    bool m_deathDeferred = false;
    PlayerObject* m_deathPlayer = nullptr;
    GameObject* m_deathObject = nullptr;
    bool m_allowDeath = false;

    bool m_simulating = false;
    bool m_injecting = false;
    bool m_simDied = false;
    bool m_simComplete = false;
};

} // namespace tsunami
