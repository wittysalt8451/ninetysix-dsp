#pragma once
#include "daisy_patch_sm.h"
#include "ParamSmoother.h"
#include <cmath>
#include <functional>

namespace ninetysix {

  enum class MappingType { None, Linear, Log };

  struct PotConfig {
      float initial_value = 0.f;
      float slew_rate = 0.05f;      // smoothing speed (0..1)
      float min_value = 0.f;
      float max_value = 1.f;
      MappingType mapping = MappingType::None;
  };

  class UIManager {
    public:
      void Init(const PotConfig pot_configs[4]);
      void Update();
  
      float GetPotValue(int index) const;     // mapped & smoothed [min..max] or [0..1]
      float GetRawPotValue(int index) const;  // smoothed raw ADC [0..1]

      /** @brief Current panel toggle (B8) state after debounce; updated in Update(). */
      bool GetTogglePressed() const { return toggle_state_; }

      /**
       * @brief Current panel button (B7) state after debounce; updated in Update().
       *
       * The callback only fires on the rising edge, so a module that needs to
       * time a long press -- hold to clear, hold to enter a mode -- has to read
       * the level itself and count in its own control tick.
       */
      bool GetButtonPressed() const { return button_state_; }
  
      void SetPotValue(int index, float value);  // feed raw ADC [0..1]
  
      void SetButtonPressedCallback(void (*cb)()) { button_pressed_callback_ = cb; }
      void SetToggleChangedCallback(void (*cb)(bool)) { toggle_changed_callback_ = cb; }
      void SetPotChangedCallback(void (*cb)(int, float)) { pot_changed_callback_ = cb; }
  
    private:
      daisy::Switch button_;
      daisy::Switch toggle_;
      bool button_state_ = false;
      bool toggle_state_ = false;
      bool last_toggle_state_ = false;
  
      ninetysix::ParamSmoother pots_[4];
      PotConfig pot_configs_[4];
  
      void (*button_pressed_callback_)() = nullptr;
      void (*toggle_changed_callback_)(bool) = nullptr;
      void (*pot_changed_callback_)(int, float) = nullptr;
  
      float ApplyMapping(int index, float value) const;
  };
}
