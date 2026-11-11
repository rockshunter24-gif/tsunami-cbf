#include "TsunamiPopup.hpp"
#include "Config.hpp"

using namespace geode::prelude;

namespace tsunami {

namespace {
constexpr float kWidth = 380.f;
constexpr float kHeight = 270.f;

const ccColor3B kSky       = { 150, 220, 255 }; // popup body
const ccColor3B kDeep      = { 25, 80, 135 };   // inner panels
const ccColor3B kFoam      = { 235, 250, 255 }; // main text
const ccColor3B kAqua      = { 110, 225, 255 }; // highlights
const ccColor3B kTabIdle   = { 80, 150, 200 };

enum ToggleTag {
    TagEnabled = 1,
    TagPredictive,
    TagRewind,
    TagIndicator,
    TagSafeMode,
};

NineSlice* panel(float w, float h, ccColor3B color, GLubyte opacity) {
    auto bg = NineSlice::create("square02b_001.png", { 0, 0, 80, 80 });
    bg->setContentSize({ w, h });
    bg->setColor(color);
    bg->setOpacity(opacity);
    return bg;
}

CCLabelBMFont* label(char const* text, char const* font, float scale, ccColor3B color) {
    auto l = CCLabelBMFont::create(text, font);
    l->setScale(scale);
    l->setColor(color);
    return l;
}
}

TsunamiPopup* TsunamiPopup::create() {
    auto ret = new TsunamiPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool TsunamiPopup::init() {
    if (!Popup::init(kWidth, kHeight, "square02b_001.png", { 0, 0, 80, 80 })) return false;

    m_bgSprite->setColor(kSky);

    // title bar
    auto titleBar = panel(kWidth - 20.f, 34.f, kDeep, 230);
    titleBar->setPosition({ kWidth / 2.f, kHeight - 24.f });
    m_mainLayer->addChild(titleBar);

    auto title = label("Tsunami CBF", "bigFont.fnt", 0.75f, kAqua);
    title->setPosition({ kWidth / 2.f, kHeight - 24.f });
    m_mainLayer->addChild(title);

    // tabs
    auto tabMenu = CCMenu::create();
    tabMenu->setPosition({ 0, 0 });
    char const* names[2] = { "Buffer", "Credits" };
    for (int i = 0; i < 2; ++i) {
        auto spr = ButtonSprite::create(names[i], 90, true, "bigFont.fnt", "GJ_button_04.png", 26.f, 0.55f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(TsunamiPopup::onTab));
        btn->setTag(i);
        btn->setPosition({ kWidth / 2.f + (i == 0 ? -55.f : 55.f), kHeight - 60.f });
        tabMenu->addChild(btn);
        m_tabButtons[i] = btn;
    }
    m_mainLayer->addChild(tabMenu);

    m_bufferTab = CCNode::create();
    m_creditsTab = CCNode::create();
    m_mainLayer->addChild(m_bufferTab);
    m_mainLayer->addChild(m_creditsTab);

    buildBufferTab();
    buildCreditsTab();
    selectTab(0);
    return true;
}

void TsunamiPopup::buildBufferTab() {
    auto const& c = cfg();

    auto body = panel(kWidth - 30.f, 175.f, kDeep, 200);
    body->setPosition({ kWidth / 2.f, 103.f });
    m_bufferTab->addChild(body);

    auto heading = label("Input Buffer", "goldFont.fnt", 0.6f, kFoam);
    heading->setPosition({ 70.f, 175.f });
    m_bufferTab->addChild(heading);

    m_tpsLabel = label("", "bigFont.fnt", 0.45f, kAqua);
    m_tpsLabel->setAnchorPoint({ 1.f, 0.5f });
    m_tpsLabel->setPosition({ kWidth - 30.f, 175.f });
    m_bufferTab->addChild(m_tpsLabel);

    // slider: left = 1 TPS (biggest buffer), right = 240 TPS (tightest)
    m_slider = Slider::create(this, menu_selector(TsunamiPopup::onSlider), 0.8f);
    m_slider->setPosition({ kWidth / 2.f, 150.f });
    m_bufferTab->addChild(m_slider);

    auto stepMenu = CCMenu::create();
    stepMenu->setPosition({ 0, 0 });
    for (int i = 0; i < 2; ++i) {
        auto spr = ButtonSprite::create(i == 0 ? "-" : "+", 20, true, "bigFont.fnt", "GJ_button_04.png", 22.f, 0.6f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(TsunamiPopup::onStep));
        btn->setTag(i == 0 ? -1 : 1);
        btn->setPosition({ i == 0 ? 40.f : kWidth - 40.f, 150.f });
        stepMenu->addChild(btn);
    }
    m_bufferTab->addChild(stepMenu);

    auto lowEnd = label("1 TPS", "chatFont.fnt", 0.5f, kFoam);
    lowEnd->setPosition({ 100.f, 134.f });
    m_bufferTab->addChild(lowEnd);
    auto highEnd = label("240 TPS", "chatFont.fnt", 0.5f, kFoam);
    highEnd->setPosition({ kWidth - 100.f, 134.f });
    m_bufferTab->addChild(highEnd);

    m_detailLabel = label("", "chatFont.fnt", 0.65f, kFoam);
    m_detailLabel->setPosition({ kWidth / 2.f, 116.f });
    m_bufferTab->addChild(m_detailLabel);

    // toggles
    auto toggles = CCMenu::create();
    toggles->setPosition({ 0, 0 });
    struct Row { int tag; char const* text; bool value; float x; float y; };
    Row rows[] = {
        { TagEnabled,    "Enabled",         c.enabled,       38.f,  94.f },
        { TagPredictive, "Predict Clicks",  c.predictive,    38.f,  70.f },
        { TagRewind,     "Death Rewind",    c.deathRewind,   38.f,  46.f },
        { TagIndicator,  "Show Indicator",  c.showIndicator, 205.f, 94.f },
        { TagSafeMode,   "Safe Mode",       c.safeMode,      205.f, 70.f },
    };
    for (auto const& r : rows) {
        auto t = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(TsunamiPopup::onToggle), 0.55f);
        t->setTag(r.tag);
        t->toggle(r.value);
        t->setPosition({ r.x, r.y });
        toggles->addChild(t);

        auto l = label(r.text, "bigFont.fnt", 0.35f, kFoam);
        l->setAnchorPoint({ 0.f, 0.5f });
        l->limitLabelWidth(130.f, 0.35f, 0.2f);
        l->setPosition({ r.x + 15.f, r.y });
        m_bufferTab->addChild(l);
    }
    m_bufferTab->addChild(toggles);

    auto hint = label("Lower TPS = bigger buffer. Works in every gamemode.", "chatFont.fnt", 0.5f, kAqua);
    hint->limitLabelWidth(320.f, 0.5f, 0.3f);
    hint->setPosition({ kWidth / 2.f, 26.f });
    m_bufferTab->addChild(hint);

    setTps(c.bufferTps, true);
}

void TsunamiPopup::buildCreditsTab() {
    auto body = panel(kWidth - 30.f, 175.f, kDeep, 200);
    body->setPosition({ kWidth / 2.f, 103.f });
    m_creditsTab->addChild(body);

    if (auto wave = CCSprite::createWithSpriteFrameName("tsunami-wave.png"_spr)) {
        wave->setScale(1.4f);
        wave->setPosition({ kWidth / 2.f, 148.f });
        m_creditsTab->addChild(wave);
    }

    auto by = label("Made by", "goldFont.fnt", 0.7f, kFoam);
    by->setPosition({ kWidth / 2.f, 105.f });
    m_creditsTab->addChild(by);

    auto name = label("Malone Ocean", "bigFont.fnt", 0.85f, kAqua);
    name->setPosition({ kWidth / 2.f, 78.f });
    m_creditsTab->addChild(name);

    auto ver = label("Tsunami CBF v1.0.0", "chatFont.fnt", 0.6f, kFoam);
    ver->setPosition({ kWidth / 2.f, 40.f });
    m_creditsTab->addChild(ver);
}

void TsunamiPopup::selectTab(int index) {
    m_bufferTab->setVisible(index == 0);
    m_creditsTab->setVisible(index == 1);
    for (int i = 0; i < 2; ++i) {
        if (auto spr = typeinfo_cast<ButtonSprite*>(m_tabButtons[i]->getNormalImage())) {
            if (spr->m_BGSprite) spr->m_BGSprite->setColor(i == index ? kAqua : kTabIdle);
        }
    }
}

void TsunamiPopup::refreshValue() {
    auto const& c = cfg();
    m_tpsLabel->setString(fmt::format("{} TPS", c.bufferTps).c_str());
    int ticks = c.windowTicks();
    m_detailLabel->setString(fmt::format(
        "Buffer window: {} tick{}  ({:.1f} ms)", ticks, ticks == 1 ? "" : "s", c.windowMs()
    ).c_str());
}

void TsunamiPopup::setTps(int tps, bool moveSlider) {
    auto& c = cfg();
    c.bufferTps = std::clamp(tps, 1, kPhysicsTps);
    c.save();
    if (moveSlider && m_slider) {
        m_slider->setValue(static_cast<float>(c.bufferTps - 1) / static_cast<float>(kPhysicsTps - 1));
    }
    refreshValue();
}

void TsunamiPopup::onTab(CCObject* sender) {
    selectTab(sender->getTag());
}

void TsunamiPopup::onSlider(CCObject* sender) {
    auto thumb = static_cast<SliderThumb*>(sender);
    float v = std::clamp(thumb->getValue(), 0.f, 1.f);
    setTps(1 + static_cast<int>(std::lround(v * (kPhysicsTps - 1))), false);
}

void TsunamiPopup::onStep(CCObject* sender) {
    setTps(cfg().bufferTps + sender->getTag(), true);
}

void TsunamiPopup::onToggle(CCObject* sender) {
    auto toggler = static_cast<CCMenuItemToggler*>(sender);
    bool value = !toggler->isToggled(); // GD flips the state after the callback runs
    auto& c = cfg();
    switch (sender->getTag()) {
        case TagEnabled:    c.enabled = value; break;
        case TagPredictive: c.predictive = value; break;
        case TagRewind:     c.deathRewind = value; break;
        case TagIndicator:  c.showIndicator = value; break;
        case TagSafeMode:   c.safeMode = value; break;
        default: break;
    }
    c.save();
}

} // namespace tsunami
