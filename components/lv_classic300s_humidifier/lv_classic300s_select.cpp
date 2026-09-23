#include "lv_classic300s_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lv_classic300s_humidifier {

static const char *const TAG = "lv_classic300s_humidifier.select";

void LVClassic300SModeSelect::setup() {
  this->parent_->add_on_status_callback([this]() { this->update_from_parent_(); });
  this->update_from_parent_();
}

void LVClassic300SModeSelect::update_from_parent_() {
  switch (this->parent_->work_mode()) {
    case MODE_AUTO:
      this->publish_state("auto");
      break;
    case MODE_MANUAL:
      this->publish_state("manual");
      break;
    case MODE_SLEEP:
      this->publish_state("sleep");
      break;
    default:
      break;  // unknown until the first status frame arrives
  }
}

void LVClassic300SModeSelect::control(const std::string &value) {
  WorkMode mode;
  uint8_t target_or_level;

  if (value == "auto") {
    mode = MODE_AUTO;
    target_or_level = this->parent_->auto_target();
  } else if (value == "manual") {
    mode = MODE_MANUAL;
    target_or_level = this->parent_->manual_level();
  } else if (value == "sleep") {
    mode = MODE_SLEEP;
    target_or_level = this->parent_->sleep_target();
  } else {
    return;
  }

  this->parent_->set_mode(mode, target_or_level);
  this->publish_state(value);
}

void LVClassic300SModeSelect::dump_config() { LOG_SELECT("", "Levoit Classic 300S Mode Select", this); }

}  // namespace lv_classic300s_humidifier
}  // namespace esphome
