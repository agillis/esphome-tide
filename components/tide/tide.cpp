#include "tide.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome {
namespace tide {

static const char *const TAG = "tide";
static constexpr float FT_PER_M = 3.280839895f;
static constexpr double D2R = 3.14159265358979323846 / 180.0;

void TideComponent::setup() {
  if (!parse_station(this->raw_station_, this->station_)) {
    ESP_LOGE(TAG, "Failed to parse station data; tide prediction disabled");
    this->mark_failed();
    return;
  }
}

void TideComponent::update() {
  this->ensure_fresh_();
  if (!this->snap_.valid) {
    ESP_LOGD(TAG, "No valid prediction yet (waiting for time sync)");
    return;
  }
  ESP_LOGD(TAG, "Now %.2f %s | high %.2f low %.2f | %.0f%%", this->conv_(this->snap_.current_height),
           this->units_feet_ ? "ft" : "m", this->conv_(this->snap_.high_level), this->conv_(this->snap_.low_level),
           this->snap_.percentage);
}

void TideComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Offline Tide Predictor:");
  if (this->station_.valid) {
    ESP_LOGCONFIG(TAG, "  Constituents: %d", (int) this->station_.constituents.size());
    ESP_LOGCONFIG(TAG, "  Z0 (datum offset): %.3f m", this->station_.z0);
    ESP_LOGCONFIG(TAG, "  Chart datum: %s", this->station_.datum.c_str());
    ESP_LOGCONFIG(TAG, "  Units: %s", this->units_feet_ ? "ft" : "m");
    ESP_LOGCONFIG(TAG, "  Prediction window: %.0f h", this->window_h_);
  } else {
    ESP_LOGCONFIG(TAG, "  STATION DATA INVALID");
  }
}

float TideComponent::conv_(float meters) const {
  if (std::isnan(meters))
    return meters;
  return this->units_feet_ ? meters * FT_PER_M : meters;
}

void TideComponent::ensure_fresh_() {
  if (this->time_ == nullptr || !this->station_.valid)
    return;
  auto now_time = this->time_->now();
  if (!now_time.is_valid()) {
    this->snap_.valid = false;
    return;
  }
  time_t now = now_time.timestamp;
  if (this->snap_.valid) {
    time_t age = now > this->snap_.t0 ? now - this->snap_.t0 : this->snap_.t0 - now;
    if (age < 30)
      return;  // cache still fresh
  }
  this->recompute_(now);
}

void TideComponent::recompute_(time_t now) {
  this->snap_ = TideSnapshot{};
  this->snap_.t0 = now;
  if (!this->station_.valid)
    return;

  // Astronomy + node factors are evaluated once at t0 and held constant over the
  // (~30 h) search window — they vary on an 18.6-year timescale.
  Astro a = compute_astro((double) now);
  this->build_params_(a, this->params_);

  this->snap_.current_height = (float) this->eval_height_(0.0);
  this->find_extremes_(now);
  this->snap_.valid = true;
}

void TideComponent::build_params_(const Astro &a, std::vector<ConstituentParam> &out) const {
  out.clear();
  out.reserve(this->station_.constituents.size());
  for (const auto &ac : this->station_.constituents) {
    double f, u;
    node_correction(ac.def->node, a, f, u);
    double v0 = 0.0, speed = 0.0;
    for (int k = 0; k < 7; k++) {
      v0 += ac.def->coeff[k] * a.val[k];
      speed += ac.def->coeff[k] * a.spd[k];
    }
    ConstituentParam cp;
    cp.A = (double) ac.amplitude * f;
    cp.w = speed * D2R;
    cp.phi = (v0 + u - (double) ac.kappa) * D2R;
    out.push_back(cp);
  }
}

void TideComponent::predict_series(time_t start, int step_s, int count, float *out) {
  if (!this->station_.valid) {
    for (int i = 0; i < count; i++)
      out[i] = NAN;
    return;
  }
  Astro a = compute_astro((double) start);
  std::vector<ConstituentParam> ps;
  this->build_params_(a, ps);
  for (int i = 0; i < count; i++) {
    double dt_h = (double) ((long) i * step_s) / 3600.0;
    double sum = this->station_.z0;
    for (const auto &c : ps)
      sum += c.A * std::cos(c.w * dt_h + c.phi);
    out[i] = this->conv_((float) sum);
  }
}

float TideComponent::predict(time_t utc) {
  float v;
  this->predict_series(utc, 0, 1, &v);
  return v;
}

double TideComponent::eval_height_(double dt) const {
  double sum = this->station_.z0;
  for (const auto &c : this->params_)
    sum += c.A * std::cos(c.w * dt + c.phi);
  return sum;
}

double TideComponent::eval_deriv_(double dt) const {
  double sum = 0.0;
  for (const auto &c : this->params_)
    sum += -c.A * c.w * std::sin(c.w * dt + c.phi);
  return sum;
}

double TideComponent::eval_d2_(double dt) const {
  double sum = 0.0;
  for (const auto &c : this->params_)
    sum += -c.A * c.w * c.w * std::cos(c.w * dt + c.phi);
  return sum;
}

// Bisection on the height derivative in [a, b] (fa = deriv(a)) to ~1 second.
double TideComponent::bisect_deriv_(double a, double b, double fa) const {
  for (int i = 0; i < 40 && (b - a) > (1.0 / 3600.0); i++) {
    double m = 0.5 * (a + b);
    double fm = this->eval_deriv_(m);
    if (fm == 0.0)
      return m;
    if ((fa < 0.0) != (fm < 0.0)) {
      b = m;
    } else {
      a = m;
      fa = fm;
    }
  }
  return 0.5 * (a + b);
}

// Scan the derivative for sign changes over [-13h, +window] (covers one full
// tidal day plus the previous extreme), refine each root, classify high/low via
// the second derivative, and pick the extremes bounding "now".
void TideComponent::find_extremes_(time_t now) {
  const double step = 5.0 / 60.0;  // 5 minutes, in hours
  const double back = 13.0;
  const double fwd = this->window_h_;

  bool have_prev = false, have_next = false;
  double prev_dt = 0.0, next_dt = 0.0;
  float prev_level = NAN, next_level = NAN;
  bool prev_high = false, next_high = false;

  // First strictly-future high and low, tracked independently of the bracket.
  bool have_next_high = false, have_next_low = false;
  double nh_dt = 0.0, nl_dt = 0.0;
  float nh_level = NAN, nl_level = NAN;

  double t_prev = -back;
  double d_prev = this->eval_deriv_(t_prev);
  for (double t = -back + step; t <= fwd + 1e-9; t += step) {
    double d_cur = this->eval_deriv_(t);
    if ((d_prev < 0.0) != (d_cur < 0.0)) {
      double root = this->bisect_deriv_(t_prev, t, d_prev);
      bool is_high = this->eval_d2_(root) < 0.0;
      float level = (float) this->eval_height_(root);
      if (root <= 0.0) {
        // keep the latest (closest to now) past extreme
        prev_dt = root;
        prev_level = level;
        prev_high = is_high;
        have_prev = true;
      } else {
        if (!have_next) {
          next_dt = root;
          next_level = level;
          next_high = is_high;
          have_next = true;
        }
        if (is_high && !have_next_high) {
          have_next_high = true;
          nh_dt = root;
          nh_level = level;
        }
        if (!is_high && !have_next_low) {
          have_next_low = true;
          nl_dt = root;
          nl_level = level;
        }
      }
    }
    d_prev = d_cur;
    t_prev = t;
  }

  // Upcoming high/low are populated regardless of whether a bracket was found.
  if (have_next_high) {
    this->snap_.has_next_high = true;
    this->snap_.next_high_epoch = now + (time_t) llround(nh_dt * 3600.0);
    this->snap_.next_high_level = nh_level;
  }
  if (have_next_low) {
    this->snap_.has_next_low = true;
    this->snap_.next_low_epoch = now + (time_t) llround(nl_dt * 3600.0);
    this->snap_.next_low_level = nl_level;
  }

  if (!have_prev || !have_next)
    return;  // leave snapshot without a bracket (percentage stays NAN)

  time_t prev_epoch = now + (time_t) llround(prev_dt * 3600.0);
  time_t next_epoch = now + (time_t) llround(next_dt * 3600.0);

  // Percentage: linear in time between the bounding extrema. base 0 => heading
  // to low, base 50 => heading to high (matches the NOAA module semantics).
  double period = (double) (next_epoch - prev_epoch);
  double elapsed = (double) (now - prev_epoch);
  double ratio = period > 0.0 ? elapsed / period : 0.0;
  ratio = std::max(0.0, std::min(1.0, ratio));
  double base = next_high ? 50.0 : 0.0;
  this->snap_.percentage = (float) (base + ratio * 50.0);

  // Bracket high/low. Tides alternate, so prev and next are opposite types; the
  // level tie-break only guards against a spurious duplicate classification.
  bool prev_is_high = prev_high;
  if (prev_high == next_high)
    prev_is_high = prev_level >= next_level;

  if (prev_is_high) {
    this->snap_.high_epoch = prev_epoch;
    this->snap_.high_level = prev_level;
    this->snap_.low_epoch = next_epoch;
    this->snap_.low_level = next_level;
  } else {
    this->snap_.low_epoch = prev_epoch;
    this->snap_.low_level = prev_level;
    this->snap_.high_epoch = next_epoch;
    this->snap_.high_level = next_level;
  }
  this->snap_.has_high = true;
  this->snap_.has_low = true;
}

float TideComponent::current_height() {
  this->ensure_fresh_();
  return this->snap_.valid ? this->conv_(this->snap_.current_height) : NAN;
}

float TideComponent::tide_percentage() {
  this->ensure_fresh_();
  return this->snap_.valid ? this->snap_.percentage : NAN;
}

float TideComponent::high_level() {
  this->ensure_fresh_();
  return (this->snap_.valid && this->snap_.has_high) ? this->conv_(this->snap_.high_level) : NAN;
}

float TideComponent::low_level() {
  this->ensure_fresh_();
  return (this->snap_.valid && this->snap_.has_low) ? this->conv_(this->snap_.low_level) : NAN;
}

time_t TideComponent::high_epoch() {
  this->ensure_fresh_();
  return (this->snap_.valid && this->snap_.has_high) ? this->snap_.high_epoch : 0;
}

time_t TideComponent::low_epoch() {
  this->ensure_fresh_();
  return (this->snap_.valid && this->snap_.has_low) ? this->snap_.low_epoch : 0;
}

float TideComponent::next_high_level() {
  this->ensure_fresh_();
  return (this->snap_.valid && this->snap_.has_next_high) ? this->conv_(this->snap_.next_high_level) : NAN;
}

float TideComponent::next_low_level() {
  this->ensure_fresh_();
  return (this->snap_.valid && this->snap_.has_next_low) ? this->conv_(this->snap_.next_low_level) : NAN;
}

time_t TideComponent::next_high_epoch() {
  this->ensure_fresh_();
  return (this->snap_.valid && this->snap_.has_next_high) ? this->snap_.next_high_epoch : 0;
}

time_t TideComponent::next_low_epoch() {
  this->ensure_fresh_();
  return (this->snap_.valid && this->snap_.has_next_low) ? this->snap_.next_low_epoch : 0;
}

}  // namespace tide
}  // namespace esphome
