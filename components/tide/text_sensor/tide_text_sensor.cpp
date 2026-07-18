#include "tide_text_sensor.h"

#include <ctime>

#include "esphome/core/log.h"

namespace esphome {
namespace tide {

static const char *const TAG = "tide.text_sensor";

void TideTextSensor::update() {
  if (this->parent_ == nullptr)
    return;

  time_t epoch = 0;
  switch (this->type_) {
    case TIDE_TEXT_HIGH_TIME:
      epoch = this->parent_->high_epoch();
      break;
    case TIDE_TEXT_LOW_TIME:
      epoch = this->parent_->low_epoch();
      break;
    case TIDE_TEXT_NEXT_HIGH_TIME:
      epoch = this->parent_->next_high_epoch();
      break;
    case TIDE_TEXT_NEXT_LOW_TIME:
      epoch = this->parent_->next_low_epoch();
      break;
  }
  if (epoch == 0) {
    this->publish_state("--:--");
    return;
  }

  // Format in local time (DST handled by localtime_r), matching the NOAA module.
  struct tm tm_local;
  localtime_r(&epoch, &tm_local);
  char buf[32];
  strftime(buf, sizeof(buf), this->format_.c_str(), &tm_local);

  // Strip a single leading zero to match the NOAA module's "10:58 AM" style.
  const char *out = (buf[0] == '0') ? buf + 1 : buf;
  this->publish_state(std::string(out));
}

void TideTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Tide Text Sensor", this); }

}  // namespace tide
}  // namespace esphome
