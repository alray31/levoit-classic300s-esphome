#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "lv_classic300s_humidifier.h"

namespace esphome {
namespace lv_classic300s_humidifier {

class LVClassic300SModeSelect : public select::Select, public Component {
 public:
  void set_parent(LVClassic300SHumidifier *parent) { this->parent_ = parent; }

  void setup() override;
  void dump_config() override;

 protected:
  void control(const std::string &value) override;
  void update_from_parent_();

  LVClassic300SHumidifier *parent_{nullptr};
};

}  // namespace lv_classic300s_humidifier
}  // namespace esphome
