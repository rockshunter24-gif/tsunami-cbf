#pragma once

#include <Geode/Geode.hpp>

namespace tsunami {

class TsunamiPopup : public geode::Popup {
public:
    static TsunamiPopup* create();

protected:
    bool init();

    void buildBufferTab();
    void buildCreditsTab();
    void selectTab(int index);
    void refreshValue();
    void setTps(int tps, bool moveSlider);

    void onTab(cocos2d::CCObject* sender);
    void onSlider(cocos2d::CCObject* sender);
    void onStep(cocos2d::CCObject* sender);
    void onToggle(cocos2d::CCObject* sender);

    cocos2d::CCNode* m_bufferTab = nullptr;
    cocos2d::CCNode* m_creditsTab = nullptr;
    CCMenuItemSpriteExtra* m_tabButtons[2] = { nullptr, nullptr };
    Slider* m_slider = nullptr;
    cocos2d::CCLabelBMFont* m_tpsLabel = nullptr;
    cocos2d::CCLabelBMFont* m_detailLabel = nullptr;
};

} // namespace tsunami
