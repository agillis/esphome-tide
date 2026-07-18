#pragma once

#include "tide_constituents.h"

namespace esphome {
namespace tide {

// Tide astronomy needs sub-degree accuracy in mean longitudes whose polynomials
// have coefficients ~5e5; single precision loses minutes of tide timing, so all
// astronomy is done in double (the ESPHome `sun` component makes the same
// choice). See tide_astronomy.cpp for the source formulas.
using tnum_t = double;

// Astronomical state at one instant, plus the derived Schureman node factors and
// phases used to correct constituent amplitudes/phases for the 18.6-year cycle.
struct Astro {
  // Spanning-set values [T+h-s, s, h, p, N, pp, 90] in degrees, and their speeds
  // in degrees/hour. V0 and constituent speed are dot products with these.
  tnum_t val[7];
  tnum_t spd[7];

  // Base node factors (dimensionless) and phase corrections u (degrees).
  tnum_t f_M2, u_M2;
  tnum_t f_O1, u_O1;
  tnum_t f_K1, u_K1;
  tnum_t f_J1, u_J1;
  tnum_t f_K2, u_K2;
  tnum_t f_L2, u_L2;
  tnum_t f_M1, u_M1;
  tnum_t f_OO1, u_OO1;
  tnum_t f_Mm;
  tnum_t f_Mf, u_Mf;
};

// Compute the astronomical state from a UTC Unix epoch (seconds).
Astro compute_astro(double utc_epoch_seconds);

// Resolve the node factor f and phase correction u (degrees) for a constituent.
void node_correction(NodeType type, const Astro &a, tnum_t &f, tnum_t &u);

}  // namespace tide
}  // namespace esphome
