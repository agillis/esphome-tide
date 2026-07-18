#include "tide_station.h"

#include <cmath>
#include <cstdlib>

#include "esphome/core/log.h"

namespace esphome {
namespace tide {

static const char *const TAG = "tide.station";

static const float FT_PER_M = 3.280839895f;

static std::string trim(const std::string &s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos)
    return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

// Split on `sep`, dropping empty fields.
static std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= s.size()) {
    size_t pos = s.find(sep, start);
    if (pos == std::string::npos) {
      out.push_back(s.substr(start));
      break;
    }
    out.push_back(s.substr(start, pos - start));
    start = pos + 1;
  }
  return out;
}

bool parse_station(const std::string &data, Station &station) {
  station = Station{};

  std::vector<std::string> tokens = split(trim(data), '|');
  if (tokens.empty() || trim(tokens[0]) != "TIDE1") {
    ESP_LOGE(TAG, "Station data missing 'TIDE1' header (got '%s')", tokens.empty() ? "" : tokens[0].c_str());
    return false;
  }

  bool in_feet = false;  // string is in meters unless U=ft
  // First pass: read header key=value tokens so the unit is known before we
  // scale Z0/MHW/MLW.
  for (size_t i = 1; i < tokens.size(); i++) {
    std::string tok = trim(tokens[i]);
    if (tok.empty())
      continue;
    size_t eq = tok.find('=');
    if (eq == std::string::npos)
      continue;  // constituent token, handled below
    std::string key = tok.substr(0, eq);
    std::string val = tok.substr(eq + 1);
    if (key == "U") {
      in_feet = (val == "ft" || val == "FT");
    } else if (key == "D") {
      station.datum = val;
    } else if (key == "Z0") {
      station.z0 = std::atof(val.c_str());
    } else if (key == "MHW") {
      station.mhw = std::atof(val.c_str());
    } else if (key == "MLW") {
      station.mlw = std::atof(val.c_str());
    } else {
      ESP_LOGW(TAG, "Unknown header key '%s' (ignored)", key.c_str());
    }
  }

  if (in_feet) {
    station.z0 /= FT_PER_M;
    if (!std::isnan(station.mhw))
      station.mhw /= FT_PER_M;
    if (!std::isnan(station.mlw))
      station.mlw /= FT_PER_M;
  }

  // Second pass: constituent tokens NAME:amplitude:phase.
  int unknown = 0;
  for (size_t i = 1; i < tokens.size(); i++) {
    std::string tok = trim(tokens[i]);
    if (tok.empty() || tok.find('=') != std::string::npos)
      continue;
    std::vector<std::string> parts = split(tok, ':');
    if (parts.size() != 3) {
      ESP_LOGW(TAG, "Malformed constituent token '%s' (ignored)", tok.c_str());
      continue;
    }
    const ConstituentDef *def = find_constituent(parts[0]);
    if (def == nullptr) {
      ESP_LOGW(TAG, "Unknown constituent '%s' (skipped)", parts[0].c_str());
      unknown++;
      continue;
    }
    float amp = std::atof(parts[1].c_str());
    if (in_feet)
      amp /= FT_PER_M;
    float kappa = std::atof(parts[2].c_str());
    if (amp <= 0.0f)
      continue;  // zero-amplitude constituents contribute nothing
    station.constituents.push_back(ActiveConstituent{def, amp, kappa});
  }

  if (station.constituents.empty()) {
    ESP_LOGE(TAG, "No usable constituents parsed from station data");
    return false;
  }

  station.valid = true;
  ESP_LOGI(TAG, "Parsed station: %d constituents, Z0=%.3f m, datum=%s%s", (int) station.constituents.size(),
           station.z0, station.datum.c_str(), unknown ? " (some unknown names skipped)" : "");
  return true;
}

}  // namespace tide
}  // namespace esphome
