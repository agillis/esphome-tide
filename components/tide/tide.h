#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/time/real_time_clock.h"

#include "tide_astronomy.h"
#include "tide_station.h"

namespace esphome {
namespace tide {

// One constituent reduced to fast-prediction form at the snapshot reference
// time t0: height contribution = A * cos(w * (t - t0) + phi).
struct ConstituentParam {
  double A;    // f * H, meters
  double w;    // constituent speed, radians/hour
  double phi;  // (V0 + u - kappa), radians
};

// Cached prediction results, valid for a short window around t0 (node factors
// drift negligibly over a day, so we reuse them for extrema search).
struct TideSnapshot {
  bool valid{false};
  time_t t0{0};
  float current_height{NAN};  // meters above chart datum
  float percentage{NAN};      // 0=just past high, 50=low, 100=high
  bool has_high{false};
  bool has_low{false};
  time_t high_epoch{0};
  time_t low_epoch{0};
  float high_level{NAN};  // meters above chart datum
  float low_level{NAN};
  // The strictly upcoming high and low (both in the future), independent of the
  // bracketing pair above.
  bool has_next_high{false};
  bool has_next_low{false};
  time_t next_high_epoch{0};
  time_t next_low_epoch{0};
  float next_high_level{NAN};
  float next_low_level{NAN};
};

// Offline harmonic tide predictor for a single station. Mirrors the outputs of
// the online NOAA module (high/low level + time, tide percentage, MHW/MLW) but
// computes everything on-device from the current time — no network required.
class TideComponent : public PollingComponent {
 public:
  void set_time(time::RealTimeClock *time) { time_ = time; }
  void set_station_data(const std::string &data) { raw_station_ = data; }
  void set_units_feet(bool feet) { units_feet_ = feet; }
  void set_prediction_window_hours(float hours) { window_h_ = hours; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  bool has_station() const { return station_.valid; }

  // --- Outputs (levels in the configured display unit; times as epochs) ---
  float current_height();     // instantaneous height now
  float tide_percentage();    // position within the current high<->low cycle
  float high_level();         // level of the high tide bounding "now"
  float low_level();          // level of the low tide bounding "now"
  time_t high_epoch();        // time of that high tide
  time_t low_epoch();         // time of that low tide
  float next_high_level();    // level of the next (upcoming) high tide
  float next_low_level();     // level of the next (upcoming) low tide
  time_t next_high_epoch();   // time of the next (upcoming) high tide
  time_t next_low_epoch();    // time of the next (upcoming) low tide
  float mean_high_water() const { return conv_(station_.mhw); }
  float mean_low_water() const { return conv_(station_.mlw); }

 protected:
  float conv_(float meters) const;
  void ensure_fresh_();
  void recompute_(time_t now);
  double eval_height_(double dt_hours) const;
  double eval_deriv_(double dt_hours) const;
  double eval_d2_(double dt_hours) const;
  double bisect_deriv_(double a, double b, double fa) const;
  void find_extremes_(time_t now);

  time::RealTimeClock *time_{nullptr};
  std::string raw_station_;
  bool units_feet_{true};
  float window_h_{30.0f};

  Station station_;
  std::vector<ConstituentParam> params_;
  TideSnapshot snap_;
};

}  // namespace tide
}  // namespace esphome
