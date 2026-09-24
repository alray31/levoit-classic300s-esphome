#include "lv_classic300s_light.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace lv_classic300s_humidifier {

static const char *const TAG = "lv_classic300s_humidifier.light";

void LVClassic300SLight::setup() {
  this->parent_->add_on_status_callback([this]() { this->sync_from_parent_(); });
  this->sync_from_parent_();
}

light::LightTraits LVClassic300SLight::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
  return traits;
}

void LVClassic300SLight::write_state(light::LightState *state) {
  float brightness;
  state->current_values_as_brightness(&brightness);

  uint8_t level = static_cast<uint8_t>(roundf(brightness * 100.0f));
  if (level > 100)
    level = 100;

  // remember what we just sent so the next status frame echoing this same
  // value back doesn't trigger a redundant make_call() in sync_from_parent_
  this->last_synced_level_ = level;
  this->parent_->set_night_light(level);
}

void LVClassic300SLight::sync_from_parent_() {
  if (this->light_state_ == nullptr)
    return;

  // While a notification effect (Breathe / Error Flash) is running, IT is
  // driving the physical light every ~150-400ms over set_night_light() --
  // it's the source of truth, not whatever the MCU's latest status frame
  // happens to report. Reacting here while an effect is active is actively
  // harmful: certain commands (the mode-change sync preamble in particular,
  // and apparently toggling Display too) make the MCU echo back a
  // night_light level of 0 in its very next status reply, as a side effect
  // unrelated to what we're actually driving the LED to. A plain
  // (non-effect) LightCall with brightness 0 is treated by ESPHome as an
  // explicit "turn off" request, which stops whatever effect is currently
  // running outright (see explicit_turn_off_request in
  // esphome/components/light/light_call.cpp) -- so without this guard, a
  // mode change or a Display toggle while Breathe/Error Flash is active
  // would silently kill the effect. Skip the sync entirely while an effect
  // owns the light; last_synced_level_ is deliberately left untouched so a
  // real resync still happens off the first status frame after the effect
  // ends.
  if (this->light_state_->get_current_effect_index() != 0)
    return;

  const uint8_t level = this->parent_->night_light_level();
  if (level == this->last_synced_level_)
    return;
  this->last_synced_level_ = level;

  auto call = this->light_state_->make_call();
  call.set_state(level > 0);
  call.set_brightness(level / 100.0f);
  call.set_transition_length(0);
  call.perform();
}

void LVClassic300SLight::dump_config() { ESP_LOGCONFIG(TAG, "Levoit Classic 300S Night Light"); }

}  // namespace lv_classic300s_humidifier
}  // namespace esphome
