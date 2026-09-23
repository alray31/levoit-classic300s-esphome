#include "lv_classic300s_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lv_classic300s_humidifier {

static const char *const TAG = "lv_classic300s_humidifier.switch";

void LVClassic300SSwitch::setup() {
  this->parent_->add_on_status_callback([this]() { this->update_from_parent_(); });
  this->update_from_parent_();
}

void LVClassic300SSwitch::update_from_parent_() {
  bool state;
  switch (this->kind_) {
    case SwitchKind::POWER:
      state = this->parent_->power_state();
      break;
    case SwitchKind::DISPLAY:
      state = this->parent_->display_state();
      break;
    case SwitchKind::STOP_AT_TARGET:
      // not reported by the MCU (see docs/A5_UART_protocol.md) -- this stays
      // whatever we last commanded, it is not corrected from status frames
      return;
    default:
      return;
  }
  this->publish_state(state);
}

void LVClassic300SSwitch::write_state(bool state) {
  switch (this->kind_) {
    case SwitchKind::POWER:
      this->parent_->set_power(state);
      break;
    case SwitchKind::DISPLAY:
      this->parent_->set_display(state);
      break;
    case SwitchKind::STOP_AT_TARGET:
      this->parent_->set_stop_at_target(state);
      break;
  }
  // optimistic: publish right away for a snappy UI. For POWER/DISPLAY the
  // next status frame will correct this if the MCU actually rejected it.
  this->publish_state(state);
}

void LVClassic300SSwitch::dump_config() { LOG_SWITCH("", "Levoit Classic 300S Switch", this); }

}  // namespace lv_classic300s_humidifier
}  // namespace esphome
