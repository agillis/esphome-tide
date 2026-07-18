#include "tide_astronomy.h"

#include <cmath>

namespace esphome {
namespace tide {

// This is a direct port of pytides (sam-cox/pytides): astro.py for the mean
// longitudes (Meeus, Astronomical Algorithms) and nodal_corrections.py for the
// Schureman node factors. Everything is kept in degrees, matching pytides, and
// converted to radians only inside trig calls.

static constexpr double PI = 3.14159265358979323846;
static constexpr double D2R = PI / 180.0;
static constexpr double R2D = 180.0 / PI;

// Mean-longitude polynomials in T (Julian centuries from J2000.0), degrees.
// Meeus 45.1 (moon s), 24.2 (sun h), lunar perigee p, node N; solar perigee pp
// from pytides (Meeus 24.2 - 24.3). Obliquity omega from Meeus 21.3, already
// rescaled so the argument is T (not U = T/100).
static const double C_S[] = {218.3164591, 481267.88134236, -0.0013268, 1.0 / 538841.0, -1.0 / 65194000.0};
static const double C_H[] = {280.46645, 36000.76983, 0.0003032};
static const double C_P[] = {83.3532430, 4069.0137111, -0.0103238, -1.0 / 80053.0, 1.0 / 18999000.0};
static const double C_N[] = {125.0445550, -1934.1361849, 0.0020762, 1.0 / 467410.0, -1.0 / 60616000.0};
static const double C_PP[] = {280.46645 - 357.52910, 36000.76932 - 35999.05030, 0.0003032 + 0.0001559, 0.00000048};
static const double C_OM[] = {23.4392794444, -0.0130025833, -4.30555556e-8, 5.55347222e-7};
static const double LUNAR_INCLINATION = 5.145;  // degrees, ~constant (JPL Horizon)

static double poly(const double *c, int n, double x) {
  double r = 0.0, p = 1.0;
  for (int i = 0; i < n; i++) {
    r += c[i] * p;
    p *= x;
  }
  return r;
}

// Derivative of the polynomial with respect to its argument.
static double dpoly(const double *c, int n, double x) {
  double r = 0.0, p = 1.0;
  for (int i = 1; i < n; i++) {
    r += c[i] * i * p;
    p *= x;
  }
  return r;
}

static double wrap360(double x) {
  x = std::fmod(x, 360.0);
  if (x < 0.0)
    x += 360.0;
  return x;
}

Astro compute_astro(double epoch) {
  Astro a{};

  // JD directly from the UTC epoch: JD(1970-01-01T00:00:00Z) = 2440587.5.
  const double JD = 2440587.5 + epoch / 86400.0;
  const double T = (JD - 2451545.0) / 36525.0;
  // Polynomials are per Julian century; convert speeds to degrees/hour.
  const double dT_dHour = 1.0 / (24.0 * 365.25 * 100.0);

  const double s = poly(C_S, 5, T), ds = dpoly(C_S, 5, T) * dT_dHour;
  const double h = poly(C_H, 3, T), dh = dpoly(C_H, 3, T) * dT_dHour;
  const double p = poly(C_P, 5, T), dp = dpoly(C_P, 5, T) * dT_dHour;
  const double N = poly(C_N, 5, T), dN = dpoly(C_N, 5, T) * dT_dHour;
  const double pp = poly(C_PP, 4, T), dpp = dpoly(C_PP, 4, T) * dT_dHour;
  const double omega = poly(C_OM, 4, T);
  const double incl = LUNAR_INCLINATION;

  // Mean hour angle: fractional part of JD -> degrees (0 at noon, 180 at
  // midnight), speed 15 deg/hour. T+h-s is the standard first spanning element.
  const double hour_val = (JD - std::floor(JD)) * 360.0;
  const double Ths = hour_val + h - s;
  const double Ths_spd = 15.0 + dh - ds;

  a.val[0] = wrap360(Ths);
  a.val[1] = wrap360(s);
  a.val[2] = wrap360(h);
  a.val[3] = wrap360(p);
  a.val[4] = wrap360(N);
  a.val[5] = wrap360(pp);
  a.val[6] = 90.0;
  a.spd[0] = Ths_spd;
  a.spd[1] = ds;
  a.spd[2] = dh;
  a.spd[3] = dp;
  a.spd[4] = dN;
  a.spd[5] = dpp;
  a.spd[6] = 0.0;

  // --- Schureman auxiliary angles (see pytides astro.py _I, _xi, _nu, ...) ---
  const double Nr = a.val[4] * D2R;
  const double omr = omega * D2R;
  const double ir = incl * D2R;

  const double cosI = std::cos(ir) * std::cos(omr) - std::sin(ir) * std::sin(omr) * std::cos(Nr);
  const double I = std::acos(cosI);  // radians

  double e1 = std::atan(std::cos(0.5 * (omr - ir)) / std::cos(0.5 * (omr + ir)) * std::tan(0.5 * Nr));
  double e2 = std::atan(std::sin(0.5 * (omr - ir)) / std::sin(0.5 * (omr + ir)) * std::tan(0.5 * Nr));
  e1 -= 0.5 * Nr;
  e2 -= 0.5 * Nr;
  const double xi = -(e1 + e2);  // radians
  const double nu = (e1 - e2);   // radians

  // Schureman 224 / 232.
  const double nup = std::atan(std::sin(2 * I) * std::sin(nu) / (std::sin(2 * I) * std::cos(nu) + 0.3347));
  const double nupp = 0.5 * std::atan(std::sin(I) * std::sin(I) * std::sin(2 * nu) /
                                      (std::sin(I) * std::sin(I) * std::cos(2 * nu) + 0.0727));

  // Degree versions used in the node-factor formulas below.
  const double I_deg = R2D * I;
  const double xi_deg = R2D * xi;
  const double nu_deg = R2D * nu;
  const double nup_deg = R2D * nup;
  const double nupp_deg = R2D * nupp;
  const double P_deg = wrap360(a.val[3] - xi_deg);  // Schureman P = p - xi

  const double Ir = I_deg * D2R;
  const double Pr = P_deg * D2R;
  const double sin2i = std::sin(ir) * std::sin(ir);
  const double one_minus = 1.0 - 1.5 * sin2i;

  // f_M2 (Schureman 78/70) and its dependents.
  const double fM2 = std::pow(std::cos(0.5 * Ir), 4) /
                     (std::pow(std::cos(0.5 * omr), 4) * std::pow(std::cos(0.5 * ir), 4));
  a.f_M2 = fM2;
  a.u_M2 = 2.0 * xi_deg - 2.0 * nu_deg;

  // f_O1 (Schureman 75/67).
  const double fO1 = (std::sin(Ir) * std::pow(std::cos(0.5 * Ir), 2)) /
                     (std::sin(omr) * std::pow(std::cos(0.5 * omr), 2) * std::pow(std::cos(0.5 * ir), 4));
  a.f_O1 = fO1;
  a.u_O1 = 2.0 * xi_deg - nu_deg;

  // f_K1 (Schureman 227/226).
  const double k1_den = 0.5023 * std::sin(2 * omr) * one_minus + 0.1681;
  a.f_K1 = std::sqrt(0.2523 * std::pow(std::sin(2 * Ir), 2) + 0.1689 * std::sin(2 * Ir) * std::cos(nu) + 0.0283) /
           k1_den;
  a.u_K1 = -nup_deg;

  // f_J1 (Schureman 76/68).
  a.f_J1 = std::sin(2 * Ir) / (std::sin(2 * omr) * one_minus);
  a.u_J1 = -nu_deg;

  // f_K2 (Schureman 235/234).
  const double k2_den = 0.5023 * std::sin(omr) * std::sin(omr) * one_minus + 0.0365;
  a.f_K2 =
      std::sqrt(0.2523 * std::pow(std::sin(Ir), 4) + 0.0367 * std::pow(std::sin(Ir), 2) * std::cos(2 * nu) + 0.0013) /
      k2_den;
  a.u_K2 = -2.0 * nupp_deg;

  // f_L2 (Schureman 215/213/214).
  const double Ra_inv =
      std::sqrt(1.0 - 12.0 * std::pow(std::tan(0.5 * Ir), 2) * std::cos(2 * Pr) + 36.0 * std::pow(std::tan(0.5 * Ir), 4));
  a.f_L2 = fM2 * Ra_inv;
  const double R = R2D * std::atan(std::sin(2 * Pr) / ((1.0 / 6.0) * std::pow(std::tan(0.5 * Ir), -2) - std::cos(2 * Pr)));
  a.u_L2 = 2.0 * xi_deg - 2.0 * nu_deg - R;

  // f_M1 (Schureman 206/207/202).
  const double Qa_inv = std::sqrt(0.25 + 1.5 * std::cos(Ir) * std::cos(2 * Pr) * std::pow(std::cos(0.5 * Ir), -0.5) +
                                  2.25 * std::cos(Ir) * std::cos(Ir) * std::pow(std::cos(0.5 * Ir), -4));
  a.f_M1 = fO1 * Qa_inv;
  const double Q = R2D * std::atan((5 * std::cos(Ir) - 1) / (7 * std::cos(Ir) + 1) * std::tan(Pr));
  a.u_M1 = xi_deg - nu_deg + Q;

  // f_Mm (Schureman 73/65).
  const double mm_mean = (2.0 / 3.0 - std::sin(omr) * std::sin(omr)) * one_minus;
  a.f_Mm = (2.0 / 3.0 - std::sin(Ir) * std::sin(Ir)) / mm_mean;

  // f_Mf (Schureman 74/66).
  const double mf_mean = std::sin(omr) * std::sin(omr) * std::pow(std::cos(0.5 * ir), 4);
  a.f_Mf = std::sin(Ir) * std::sin(Ir) / mf_mean;
  a.u_Mf = -2.0 * xi_deg;

  // f_OO1 (Schureman 77/69).
  const double oo1_mean = std::sin(omr) * std::pow(std::sin(0.5 * omr), 2) * std::pow(std::cos(0.5 * ir), 4);
  a.f_OO1 = std::sin(Ir) * std::pow(std::sin(0.5 * Ir), 2) / oo1_mean;
  a.u_OO1 = -2.0 * xi_deg - nu_deg;

  return a;
}

void node_correction(NodeType type, const Astro &a, tnum_t &f, tnum_t &u) {
  switch (type) {
    case NODE_UNITY:
      f = 1.0;
      u = 0.0;
      break;
    case NODE_M2:
      f = a.f_M2;
      u = a.u_M2;
      break;
    case NODE_M2_POW2:
      f = a.f_M2 * a.f_M2;
      u = 2.0 * a.u_M2;
      break;
    case NODE_M2_POW3:
      f = a.f_M2 * a.f_M2 * a.f_M2;
      u = 3.0 * a.u_M2;
      break;
    case NODE_M2_POW4:
      f = a.f_M2 * a.f_M2 * a.f_M2 * a.f_M2;
      u = 4.0 * a.u_M2;
      break;
    case NODE_M3:
      f = std::pow(a.f_M2, 1.5);
      u = 1.5 * a.u_M2;
      break;
    case NODE_MSF:
      f = a.f_M2;
      u = -a.u_M2;
      break;
    case NODE_MS4:
      f = a.f_M2;
      u = a.u_M2;
      break;
    case NODE_MK3:
      f = a.f_M2 * a.f_K1;
      u = a.u_M2 + a.u_K1;
      break;
    case NODE_2MK3:
      f = a.f_M2 * a.f_O1;
      u = a.u_M2 + a.u_O1;
      break;
    case NODE_RHO1:
      f = a.f_M2 * a.f_K1;
      u = a.u_M2 - a.u_K1;
      break;
    case NODE_2Q1:
      f = a.f_M2 * a.f_J1;
      u = a.u_M2 - a.u_J1;
      break;
    case NODE_O1:
      f = a.f_O1;
      u = a.u_O1;
      break;
    case NODE_K1:
      f = a.f_K1;
      u = a.u_K1;
      break;
    case NODE_J1:
      f = a.f_J1;
      u = a.u_J1;
      break;
    case NODE_M1:
      f = a.f_M1;
      u = a.u_M1;
      break;
    case NODE_OO1:
      f = a.f_OO1;
      u = a.u_OO1;
      break;
    case NODE_L2:
      f = a.f_L2;
      u = a.u_L2;
      break;
    case NODE_K2:
      f = a.f_K2;
      u = a.u_K2;
      break;
    case NODE_MM:
      f = a.f_Mm;
      u = 0.0;
      break;
    case NODE_MF:
      f = a.f_Mf;
      u = a.u_Mf;
      break;
    default:
      f = 1.0;
      u = 0.0;
      break;
  }
}

}  // namespace tide
}  // namespace esphome
