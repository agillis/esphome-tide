#include "host_time.h"

#include <ctime>

#include "esphome/core/log.h"

namespace esphome {
namespace host_time {

static const char *const TAG = "host_time";

void HostTime::setup() { this->update(); }

void HostTime::update() {
  auto now = ::time(nullptr);
  this->synchronize_epoch_(static_cast<uint32_t>(now));
  ESP_LOGD(TAG, "Seeded clock from host system time (epoch %u)", static_cast<unsigned>(now));
}

}  // namespace host_time
}  // namespace esphome
