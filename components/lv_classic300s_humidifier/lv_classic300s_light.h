#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "lv_classic300s_humidifier.h"

namespace esphome {
namespace lv_classic300s_humidifier {

// Night Light exposed as a real `light` entity (on/off + 0-100% brightness)
// instead of a plain number, so it gets a proper light card in HA and can
// host effects (see common_entities.yaml for the notification effects built
// on top of this).
//
// Unlike a plain output-backed `monochromatic` light, this keeps the HA-side
// state in sync with what the appliance MCU actually reports (status_callback_),
// e.g. after an ESPHome reboot where the MCU may still remember a level the
// light entity's restore_mode doesn't know about.
class LVClassic300SLight : public Component, public light::LightOutput {
 public:
  void set_parent(LVClassic300SHumidifier *parent) { this->parent_ = parent; }

  void setup() override;
  void dump_config() override;

  light::LightTraits get_traits() override;
  void setup_state(light::LightState *state) override { this->light_state_ = state; }
  void write_state(light::LightState *state) override;

 protected:
  void sync_from_parent_();

  LVClassic300SHumidifier *parent_{nullptr};
  light::LightState *light_state_{nullptr};
  // sentinel > 100 so the first status update after boot always syncs once,
  // even if the device happens to report 0%.
  uint16_t last_synced_level_{0xFFFF};
};

}  // namespace lv_classic300s_humidifier
}  // namespace esphome
