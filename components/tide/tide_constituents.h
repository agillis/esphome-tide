#pragma once

#include <cstdint>
#include <string>

namespace esphome {
namespace tide {

// How the node factor (f) and equilibrium-phase correction (u) are derived for a
// given constituent. Base constituents map directly; compound constituents are
// expressed as products/sums of the base corrections (mirrors pytides
// constituent.py / nodal_corrections.py exactly).
enum NodeType {
  NODE_UNITY,    // f = 1, u = 0            (S2, S4, S6, S1, Sa, Ssa, T2, R2, P1)
  NODE_M2,       // f_M2,  u = u_M2          (M2, N2, NU2, 2N2, LAM2)
  NODE_M2_POW2,  // f_M2^2, u = 2*u_M2       (M4, MN4, MU2)
  NODE_M2_POW3,  // f_M2^3, u = 3*u_M2       (M6)
  NODE_M2_POW4,  // f_M2^4, u = 4*u_M2       (M8)
  NODE_M3,       // f_M2^1.5, u = 1.5*u_M2   (M3)
  NODE_MSF,      // f_M2,  u = -u_M2         (MSF, 2SM2)
  NODE_MS4,      // f_M2,  u = +u_M2         (MS4)
  NODE_MK3,      // f_M2*f_K1, u = u_M2+u_K1 (MK3)
  NODE_2MK3,     // f_M2*f_O1, u = u_M2+u_O1 (2MK3)
  NODE_RHO1,     // f_M2*f_K1, u = u_M2-u_K1 (RHO)
  NODE_2Q1,      // f_M2*f_J1, u = u_M2-u_J1 (2Q1)
  NODE_O1,       // f_O1,  u = u_O1          (O1, Q1)
  NODE_K1,       // f_K1,  u = u_K1
  NODE_J1,       // f_J1,  u = u_J1
  NODE_M1,       // f_M1,  u = u_M1
  NODE_OO1,      // f_OO1, u = u_OO1
  NODE_L2,       // f_L2,  u = u_L2
  NODE_K2,       // f_K2,  u = u_K2
  NODE_MM,       // f_Mm,  u = 0
  NODE_MF,       // f_Mf,  u = u_Mf
};

// A universal (station-independent) tidal constituent. `coeff` are the Doodson
// coefficients in the pytides spanning set order: [T+h-s, s, h, p, N, pp, 90].
// The equilibrium argument V0 and the constituent speed are the dot product of
// these coefficients with the astronomical values / speeds respectively.
struct ConstituentDef {
  const char *name;
  int8_t coeff[7];
  NodeType node;
};

extern const ConstituentDef CONSTITUENTS[];
extern const int NUM_CONSTITUENTS;

// Case-insensitive lookup with alias normalization (e.g. "lambda2" -> "LAM2").
// Returns nullptr if the name is unknown.
const ConstituentDef *find_constituent(const std::string &name);

}  // namespace tide
}  // namespace esphome
