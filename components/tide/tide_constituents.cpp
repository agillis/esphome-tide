#include "tide_constituents.h"

#include <cctype>

namespace esphome {
namespace tide {

// The 37 standard NOAA constituents. Doodson coefficients decoded from the
// extended-Doodson (XDO) strings in pytides constituent.py, in the spanning-set
// order [T+h-s, s, h, p, N, pp, 90]. Compound constituents carry the summed
// coefficients of their members.
const ConstituentDef CONSTITUENTS[] = {
    {"M2", {2, 0, 0, 0, 0, 0, 0}, NODE_M2},
    {"S2", {2, 2, -2, 0, 0, 0, 0}, NODE_UNITY},
    {"N2", {2, -1, 0, 1, 0, 0, 0}, NODE_M2},
    {"K1", {1, 1, 0, 0, 0, 0, -1}, NODE_K1},
    {"M4", {4, 0, 0, 0, 0, 0, 0}, NODE_M2_POW2},
    {"O1", {1, -1, 0, 0, 0, 0, 1}, NODE_O1},
    {"M6", {6, 0, 0, 0, 0, 0, 0}, NODE_M2_POW3},
    {"MK3", {3, 1, 0, 0, 0, 0, -1}, NODE_MK3},
    {"S4", {4, 4, -4, 0, 0, 0, 0}, NODE_UNITY},
    {"MN4", {4, -1, 0, 1, 0, 0, 0}, NODE_M2_POW2},
    {"NU2", {2, -1, 2, -1, 0, 0, 0}, NODE_M2},
    {"S6", {6, 6, -6, 0, 0, 0, 0}, NODE_UNITY},
    {"MU2", {2, -2, 2, 0, 0, 0, 0}, NODE_M2_POW2},
    {"2N2", {2, -2, 0, 2, 0, 0, 0}, NODE_M2},
    {"OO1", {1, 3, 0, 0, 0, 0, -1}, NODE_OO1},
    {"LAM2", {2, 1, -2, 1, 0, 0, 2}, NODE_M2},
    {"S1", {1, 1, -1, 0, 0, 0, 0}, NODE_UNITY},
    {"M1", {1, 0, 0, 0, 0, 0, 1}, NODE_M1},
    {"J1", {1, 2, 0, -1, 0, 0, -1}, NODE_J1},
    {"MM", {0, 1, 0, -1, 0, 0, 0}, NODE_MM},
    {"SSA", {0, 0, 2, 0, 0, 0, 0}, NODE_UNITY},
    {"SA", {0, 0, 1, 0, 0, 0, 0}, NODE_UNITY},
    {"MSF", {0, 2, -2, 0, 0, 0, 0}, NODE_MSF},
    {"MF", {0, 2, 0, 0, 0, 0, 0}, NODE_MF},
    {"RHO", {1, -2, 2, -1, 0, 0, 1}, NODE_RHO1},
    {"Q1", {1, -2, 0, 1, 0, 0, 1}, NODE_O1},
    {"T2", {2, 2, -3, 0, 0, 1, 0}, NODE_UNITY},
    {"R2", {2, 2, -1, 0, 0, -1, 2}, NODE_UNITY},
    {"2Q1", {1, -3, 0, 2, 0, 0, 1}, NODE_2Q1},
    {"P1", {1, 1, -2, 0, 0, 0, 1}, NODE_UNITY},
    {"2SM2", {2, 4, -4, 0, 0, 0, 0}, NODE_MSF},
    {"M3", {3, 0, 0, 0, 0, 0, 0}, NODE_M3},
    {"L2", {2, 1, 0, -1, 0, 0, 2}, NODE_L2},
    {"2MK3", {3, -1, 0, 0, 0, 0, 1}, NODE_2MK3},
    {"K2", {2, 2, 0, 0, 0, 0, 0}, NODE_K2},
    {"M8", {8, 0, 0, 0, 0, 0, 0}, NODE_M2_POW4},
    {"MS4", {4, 2, -2, 0, 0, 0, 0}, NODE_MS4},
};

const int NUM_CONSTITUENTS = sizeof(CONSTITUENTS) / sizeof(CONSTITUENTS[0]);

// Upper-case, strip spaces, and map known alternate spellings to the canonical
// names used in the table above.
static std::string normalize_name(const std::string &raw) {
  std::string s;
  s.reserve(raw.size());
  for (char c : raw) {
    if (std::isspace(static_cast<unsigned char>(c)))
      continue;
    s += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  if (s == "LAMBDA2" || s == "LDA2")
    return "LAM2";
  if (s == "RHO1")
    return "RHO";
  if (s == "NU2" || s == "MU2")  // pytides lowercase spellings normalize here
    return s;
  return s;
}

const ConstituentDef *find_constituent(const std::string &name) {
  std::string s = normalize_name(name);
  for (int i = 0; i < NUM_CONSTITUENTS; i++) {
    if (s == CONSTITUENTS[i].name)
      return &CONSTITUENTS[i];
  }
  return nullptr;
}

}  // namespace tide
}  // namespace esphome
