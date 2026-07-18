#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "tide_constituents.h"

namespace esphome {
namespace tide {

// One constituent's per-station harmonic constants.
struct ActiveConstituent {
  const ConstituentDef *def;  // universal metadata (speed, Doodson, node type)
  float amplitude;            // H, meters
  float kappa;                // Greenwich phase lag, degrees
};

// A parsed station. All heights are in meters relative to the chart datum
// (typically MLLW), matching the NOAA prediction output.
struct Station {
  bool valid{false};
  double z0{0.0};                  // datum offset added to the harmonic sum (m)
  float mhw{NAN};                  // mean high water above chart datum (m)
  float mlw{NAN};                  // mean low water above chart datum (m)
  std::string datum{"MLLW"};       // chart datum name (informational)
  std::vector<ActiveConstituent> constituents;
};

// Parse a "TIDE1|...|NAME:H:kappa|..." station string. Returns false and leaves
// station.valid == false if the header/version is missing or no constituent
// could be resolved. Unknown constituent names are logged and skipped.
bool parse_station(const std::string &data, Station &station);

}  // namespace tide
}  // namespace esphome
