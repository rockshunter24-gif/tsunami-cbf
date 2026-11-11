#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/GJGameLevel.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "Config.hpp"
#include "Engine.hpp"
#include "TsunamiPopup.hpp"

using namespace geode::prelude;
using namespace tsunami;

namespace {

// Only touch the real, on-screen PlayLayer (not the editor, not fake layers other mods make).
PlayLayer* realPlayLayer(GJBaseGameLayer* self) {
    auto pl = PlayLayer::get();
    if (!pl || static_cast<GJBaseGameLayer*>(pl) != self) return nullptr;
    if (!pl->getParent()) return nullptr;
    return pl;
}

CCMenuItemSpriteExtra* makeOpenButton(CCObject* target, SEL_MenuHandler sel, float scale) {
    auto top = CCSprite::createWithSpriteFrameName("tsunami-wave.png"_spr);
    CCNode* spr;
    if (top) {
        top->setScale(0.9f);
        spr = CircleButtonSprite::create(top, CircleBaseColor::Cyan, CircleBaseSize::Small);
    }
    else {
        auto label = CCLabelBMFont::create("T", "bigFont.fnt");
        spr = CircleButtonSprite::create(label, CircleBaseColor::Cyan, CircleBaseSize::Small);
    }
    spr->setScale(scale);
    auto btn = CCMenuItemSpriteExtra::create(spr, target, sel);
    btn->setID("tsunami-cbf-button"_spr);
    return btn;
}

} // namespace

// ------------------------------------------------------------------ physics steps / inputs

class $modify(TsunamiBaseLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("GJBaseGameLayer::handleButton", Priority::Early);
    }

    // one call = one 240 TPS physics step
#if GEODE_COMP_GD_VERSION >= 22081
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        StepTiming t{ dt, isHalfTick, isLastTick };
#else
    void processCommands(float dt) {
        StepTiming t{ dt, false, true };
#endif
        auto& e = Engine::get();
        auto pl = realPlayLayer(this);

        auto callOriginal = [&] {
#if GEODE_COMP_GD_VERSION >= 22081
            GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
#else
            GJBaseGameLayer::processCommands(dt);
#endif
        };

        if (!pl || e.simulating()) return callOriginal();
        if (!cfg().enabled) {
            e.disabledStep(pl);
            return callOriginal();
        }

        e.beforeStep(pl, t);
        callOriginal();
        e.afterStep(pl, t);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        auto& e = Engine::get();
        auto pl = realPlayLayer(this);
        if (pl && cfg().enabled && !e.simulating() && !e.injecting()) {
            if (e.onButton(pl, down, button, isPlayer1)) return; // held back, released later
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

// ------------------------------------------------------------------ deaths, resets, safe mode

class $modify(TsunamiPlayLayer, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (Engine::get().onDestroyPlayer(this, player, object)) return;
        PlayLayer::destroyPlayer(player, object);
    }

    void resetLevel() {
        Engine::get().reset();
        PlayLayer::resetLevel();
    }

    void onQuit() {
        Engine::get().reset();
        PlayLayer::onQuit();
    }

    void levelComplete() {
        auto& e = Engine::get();
        if (e.simulating()) {
            e.markSimComplete();
            return;
        }
        bool testMode = m_isTestMode;
        if (cfg().enabled && cfg().safeMode) m_isTestMode = true;
        PlayLayer::levelComplete();
        m_isTestMode = testMode;
    }

    void showNewBest(bool newReward, int orbs, int diamonds, bool demonKey, bool noRetry, bool noTitle) {
        if (cfg().enabled && cfg().safeMode) return;
        PlayLayer::showNewBest(newReward, orbs, diamonds, demonKey, noRetry, noTitle);
    }
};

class $modify(TsunamiLevel, GJGameLevel) {
    void savePercentage(int percent, bool isPracticeMode, int clicks, int attempts, bool isChkValid) {
        if (cfg().enabled && cfg().safeMode && m_stars.value() > 0) return;
        GJGameLevel::savePercentage(percent, isPracticeMode, clicks, attempts, isChkValid);
    }
};

// No sound effects from simulated futures.
class $modify(TsunamiAudio, FMODAudioEngine) {
    int playEffectAdvanced(
        gd::string path, float speed, float unknown, float volume, float pitch, bool fastFourierTransform,
        bool reverb, int startMillis, int endMillis, int fadeIn, int fadeOut, bool loopEnabled, int effectID,
        bool override, bool noPreload, int channelID, int uniqueID, float minInterval, int sfxGroup
    ) {
        if (Engine::get().simulating()) return 0;
        return FMODAudioEngine::playEffectAdvanced(
            path, speed, unknown, volume, pitch, fastFourierTransform, reverb, startMillis, endMillis,
            fadeIn, fadeOut, loopEnabled, effectID, override, noPreload, channelID, uniqueID, minInterval, sfxGroup
        );
    }
};

// ------------------------------------------------------------------ GUI buttons

class $modify(TsunamiPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto win = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();
        menu->setID("tsunami-cbf-menu"_spr);
        menu->setPosition({ win.width - 30.f, 30.f });
        menu->addChild(makeOpenButton(this, menu_selector(TsunamiPause::onTsunami), 0.85f));
        this->addChild(menu, 50);
    }

    void onTsunami(CCObject*) {
        if (auto p = TsunamiPopup::create()) p->show();
    }
};

class $modify(TsunamiMenu, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        auto win = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();
        menu->setID("tsunami-cbf-menu"_spr);
        menu->setPosition({ 28.f, win.height - 28.f });
        menu->addChild(makeOpenButton(this, menu_selector(TsunamiMenu::onTsunami), 0.8f));
        this->addChild(menu, 50);
        return true;
    }

    void onTsunami(CCObject*) {
        if (auto p = TsunamiPopup::create()) p->show();
    }
};

$on_mod(Loaded) {
    cfg().load();
}
