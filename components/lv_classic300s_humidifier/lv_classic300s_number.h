#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "lv_classic300s_humidifier.h"

namespace esphome {
namespace lv_classic300s_humidifier {

enum class NumberKind {
  MANUAL_LEVEL,
  AUTO_TARGET_HUMIDITY,
  SLEEP_TARGET_HUMIDITY,
  NIGHT_LIGHT,
};

class LVClassic300SNumber : public number::Number, public Component {
 public:
  void set_parent(LVClassic300SHumidifier *parent) { this->parent_ = parent; }
  void set_kind(NumberKind kind) { this->kind_ = kind; }

  void setup() override;
  void dump_config() override;

 protected:
  void control(float value) override;
  void update_from_parent_();

  LVClassic300SHumidifier *parent_{nullptr};
  NumberKind kind_{NumberKind::MANUAL_LEVEL};
};

}  // namespace lv_classic300s_humidifier
}  // namespace esphome
