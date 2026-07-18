#pragma once

#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace host_time {

// Test-only time source for the ESPHome `host` platform: seeds the clock from
// the PC's system time so the tide component has valid UTC to predict from.
class HostTime : public time::RealTimeClock {
 public:
  void setup() override;
  void update() override;
};

}  // namespace host_time
}  // namespace esphome
