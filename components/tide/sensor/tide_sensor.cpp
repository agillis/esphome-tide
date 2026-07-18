#include "tide_sensor.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome {
namespace tide {

static const char *const TAG = "tide.sensor";

void TideSensor::update() {
  if (this->parent_ == nullptr)
    return;
  float value = NAN;
  switch (this->type_) {
    case TIDE_SENSOR_CURRENT_HEIGHT:
      value = this->parent_->current_height();
      break;
    case TIDE_SENSOR_PERCENTAGE:
      value = this->parent_->tide_percentage();
      break;
    case TIDE_SENSOR_HIGH_LEVEL:
      value = this->parent_->high_level();
      break;
    case TIDE_SENSOR_LOW_LEVEL:
      value = this->parent_->low_level();
      break;
    case TIDE_SENSOR_NEXT_HIGH_LEVEL:
      value = this->parent_->next_high_level();
      break;
    case TIDE_SENSOR_NEXT_LOW_LEVEL:
      value = this->parent_->next_low_level();
      break;
    case TIDE_SENSOR_MEAN_HIGH_WATER:
      value = this->parent_->mean_high_water();
      break;
    case TIDE_SENSOR_MEAN_LOW_WATER:
      value = this->parent_->mean_low_water();
      break;
    case TIDE_SENSOR_HIGH_EPOCH:
      value = (float) this->parent_->high_epoch();
      break;
    case TIDE_SENSOR_LOW_EPOCH:
      value = (float) this->parent_->low_epoch();
      break;
    case TIDE_SENSOR_NEXT_HIGH_EPOCH:
      value = (float) this->parent_->next_high_epoch();
      break;
    case TIDE_SENSOR_NEXT_LOW_EPOCH:
      value = (float) this->parent_->next_low_epoch();
      break;
  }
  this->publish_state(value);
}

void TideSensor::dump_config() { LOG_SENSOR("", "Tide Sensor", this); }

}  // namespace tide
}  // namespace esphome
