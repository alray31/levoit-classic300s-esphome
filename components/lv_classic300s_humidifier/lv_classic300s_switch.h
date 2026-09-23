#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "lv_classic300s_humidifier.h"

namespace esphome {
namespace lv_classic300s_humidifier {

enum class SwitchKind {
  POWER,
  DISPLAY,
  STOP_AT_TARGET,
};

class LVClassic300SSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(LVClassic300SHumidifier *parent) { this->parent_ = parent; }
  void set_kind(SwitchKind kind) { this->kind_ = kind; }

  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
  void update_from_parent_();

  LVClassic300SHumidifier *parent_{nullptr};
  SwitchKind kind_{SwitchKind::POWER};
};

}  // namespace lv_classic300s_humidifier
}  // namespace esphome
