#pragma once

#include <string>

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include "../tide.h"

namespace esphome {
namespace tide {

enum TideTextSensorType {
  TIDE_TEXT_HIGH_TIME,
  TIDE_TEXT_LOW_TIME,
};

class TideTextSensor : public text_sensor::TextSensor, public PollingComponent {
 public:
  void set_parent(TideComponent *parent) { parent_ = parent; }
  void set_type(TideTextSensorType type) { type_ = type; }
  void set_format(const std::string &format) { format_ = format; }
  void update() override;
  void dump_config() override;

 protected:
  TideComponent *parent_{nullptr};
  TideTextSensorType type_{TIDE_TEXT_HIGH_TIME};
  std::string format_{"%I:%M %p"};
};

}  // namespace tide
}  // namespace esphome
