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
  const uint8_t level = this->parent_->night_light_level();
  if (level == this->last_synced_level_)
    return;
  this->last_synced_level_ = level;

  if (this->light_state_ == nullptr)
    return;

  auto call = this->light_state_->make_call();
  call.set_state(level > 0);
  call.set_brightness(level / 100.0f);
  call.set_transition_length(0);
  call.perform();
}

void LVClassic300SLight::dump_config() { ESP_LOGCONFIG(TAG, "Levoit Classic 300S Night Light"); }

}  // namespace lv_classic300s_humidifier
}  // namespace esphome
