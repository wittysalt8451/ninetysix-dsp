#include "UIManager.h"

using namespace daisy;
using namespace patch_sm;
using namespace sudwalfulkaan;

void UIManager::Init(const PotConfig pot_configs[4]) {
    button_.Init(DaisyPatchSM::B7, 0, Switch::TYPE_MOMENTARY, Switch::POLARITY_INVERTED, Switch::PULL_UP);
    toggle_.Init(DaisyPatchSM::B8, 0, Switch::TYPE_TOGGLE, Switch::POLARITY_NORMAL, Switch::PULL_UP);
    last_toggle_state_ = toggle_.Pressed();

    for(int i = 0; i < 4; ++i) {
        pot_configs_[i] = pot_configs[i];
        pots_[i].Init(pot_configs[i].initial_value, pot_configs[i].slew_rate);
    }
}

void UIManager::Update() {
    button_.Debounce();
    toggle_.Debounce();

    if(button_.RisingEdge() && button_pressed_callback_) {
        button_pressed_callback_();
    }

    toggle_state_ = toggle_.Pressed();
    if(toggle_state_ != last_toggle_state_ && toggle_changed_callback_) {
        toggle_changed_callback_(toggle_state_);
        last_toggle_state_ = toggle_state_;
    }
}

void UIManager::SetPotValue(int index, float value) {
    if(index < 0 || index >= 4) return;
    float smoothed_raw = pots_[index].Process(value);
    if(pots_[index].HasSignificantChange()) {
        float mapped = ApplyMapping(index, smoothed_raw);
        if(pot_changed_callback_) {
            pot_changed_callback_(index, mapped);
        }
    }
}

float UIManager::GetPotValue(int index) const {
    if(index < 0 || index >= 4) return 0.f;
    float raw_val = pots_[index].Value();
    return ApplyMapping(index, raw_val);
}

float UIManager::GetRawPotValue(int index) const {
    if(index < 0 || index >= 4) return 0.f;
    return pots_[index].Value();
}

float UIManager::ApplyMapping(int index, float value) const {
    const PotConfig& cfg = pot_configs_[index];
    switch(cfg.mapping) {
        case MappingType::None:
            return value;
        case MappingType::Linear:
            return value * (cfg.max_value - cfg.min_value) + cfg.min_value;
        case MappingType::Log: {
            if(value <= 0.f) return cfg.min_value;
            float logMin = std::log(cfg.min_value);
            float logMax = std::log(cfg.max_value);
            float logVal = logMin + value * (logMax - logMin);
            return std::exp(logVal);
        }
    }
    return value;
}
