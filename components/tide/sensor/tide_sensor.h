#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

#include "../tide.h"

namespace esphome {
namespace tide {

enum TideSensorType {
  TIDE_SENSOR_CURRENT_HEIGHT,
  TIDE_SENSOR_PERCENTAGE,
  TIDE_SENSOR_HIGH_LEVEL,
  TIDE_SENSOR_LOW_LEVEL,
  TIDE_SENSOR_NEXT_HIGH_LEVEL,
  TIDE_SENSOR_NEXT_LOW_LEVEL,
  TIDE_SENSOR_MEAN_HIGH_WATER,
  TIDE_SENSOR_MEAN_LOW_WATER,
  TIDE_SENSOR_HIGH_EPOCH,
  TIDE_SENSOR_LOW_EPOCH,
  TIDE_SENSOR_NEXT_HIGH_EPOCH,
  TIDE_SENSOR_NEXT_LOW_EPOCH,
};

class TideSensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_parent(TideComponent *parent) { parent_ = parent; }
  void set_type(TideSensorType type) { type_ = type; }
  void update() override;
  void dump_config() override;

 protected:
  TideComponent *parent_{nullptr};
  TideSensorType type_{TIDE_SENSOR_CURRENT_HEIGHT};
};

}  // namespace tide
}  // namespace esphome
