#include "lv_classic300s_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lv_classic300s_humidifier {

static const char *const TAG = "lv_classic300s_humidifier.number";

void LVClassic300SNumber::setup() {
  this->parent_->add_on_status_callback([this]() { this->update_from_parent_(); });
  this->update_from_parent_();
}

void LVClassic300SNumber::update_from_parent_() {
  switch (this->kind_) {
    case NumberKind::MANUAL_LEVEL:
      this->publish_state(this->parent_->manual_level());
      break;
    case NumberKind::AUTO_TARGET_HUMIDITY:
      this->publish_state(this->parent_->auto_target());
      break;
    case NumberKind::SLEEP_TARGET_HUMIDITY:
      this->publish_state(this->parent_->sleep_target());
      break;
    case NumberKind::NIGHT_LIGHT:
      this->publish_state(this->parent_->night_light_level());
      break;
  }
}

void LVClassic300SNumber::control(float value) {
  const uint8_t v = static_cast<uint8_t>(value < 0 ? 0 : value);

  switch (this->kind_) {
    case NumberKind::MANUAL_LEVEL:
      // sending the manual-level command also switches the appliance into
      // MANUAL mode, matching stock behavior (sync preamble + level command)
      this->parent_->send_mode_sync_preamble();
      this->parent_->set_manual_level(v);
      break;
    case NumberKind::AUTO_TARGET_HUMIDITY:
      this->parent_->send_mode_sync_preamble();
      this->parent_->set_auto_target(v);
      break;
    case NumberKind::SLEEP_TARGET_HUMIDITY:
      this->parent_->send_mode_sync_preamble();
      this->parent_->set_sleep_target(v);
      break;
    case NumberKind::NIGHT_LIGHT:
      this->parent_->set_night_light(v);
      break;
  }

  this->publish_state(value);
}

void LVClassic300SNumber::dump_config() { LOG_NUMBER("", "Levoit Classic 300S Number", this); }

}  // namespace lv_classic300s_humidifier
}  // namespace esphome
