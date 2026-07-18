// Standalone harness that exercises the REAL device engine (tide_astronomy.cpp
// + tide_constituents.cpp) without ESPHome, so we can diff it against the Python
// reference (which matches pytides). It parses a TIDE1 station string, then for
// each requested epoch computes the instantaneous height exactly as tide.cpp
// does (params at t0 = epoch, evaluated at dt = 0).
//
// Build:
//   g++ -std=c++17 -O2 engine_test.cpp \
//       ../components/tide/tide_astronomy.cpp ../components/tide/tide_constituents.cpp -o engine_test
// Run:
//   ./engine_test "<TIDE1|...>" <base_epoch> <count> <step_sec>   # prints epoch,height_m

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../components/tide/tide_astronomy.h"
#include "../components/tide/tide_constituents.h"

using namespace esphome::tide;

static constexpr double D2R = 3.14159265358979323846 / 180.0;

struct Active {
  const ConstituentDef *def;
  double H;
  double kappa;
};

static std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  size_t start = 0, pos;
  while ((pos = s.find(sep, start)) != std::string::npos) {
    out.push_back(s.substr(start, pos - start));
    start = pos + 1;
  }
  out.push_back(s.substr(start));
  return out;
}

int main(int argc, char **argv) {
  if (argc < 5) {
    fprintf(stderr, "usage: %s <string> <base_epoch> <count> <step_sec>\n", argv[0]);
    return 2;
  }
  std::string data = argv[1];
  long base = atol(argv[2]);
  int count = atoi(argv[3]);
  long step = atol(argv[4]);

  double z0 = 0.0;
  bool feet = false;
  std::vector<Active> cons;
  for (const auto &tok : split(data, '|')) {
    if (tok.empty() || tok == "TIDE1")
      continue;
    if (tok.rfind("U=", 0) == 0) {
      feet = (tok.substr(2) == "ft");
      continue;
    }
    if (tok.rfind("Z0=", 0) == 0) {
      z0 = atof(tok.c_str() + 3);
      continue;
    }
    if (tok.find('=') != std::string::npos)
      continue;  // MHW/MLW/D header tokens, not needed here
    auto p = split(tok, ':');
    if (p.size() != 3)
      continue;
    const ConstituentDef *def = find_constituent(p[0]);
    if (def == nullptr) {
      fprintf(stderr, "unknown constituent %s\n", p[0].c_str());
      continue;
    }
    cons.push_back({def, atof(p[1].c_str()), atof(p[2].c_str())});
  }
  const double ftm = 3.280839895;
  if (feet) {
    z0 /= ftm;
    for (auto &c : cons)
      c.H /= ftm;
  }

  for (int i = 0; i < count; i++) {
    long epoch = base + (long) i * step;
    Astro a = compute_astro((double) epoch);
    double sum = z0;
    for (const auto &c : cons) {
      double f, u;
      node_correction(c.def->node, a, f, u);
      double v0 = 0.0, speed = 0.0;
      for (int k = 0; k < 7; k++) {
        v0 += c.def->coeff[k] * a.val[k];
        speed += c.def->coeff[k] * a.spd[k];
      }
      (void) speed;  // dt = 0 here, so speed term drops out
      double phi = (v0 + u - c.kappa) * D2R;
      sum += c.H * f * std::cos(phi);
    }
    printf("%ld,%.9f\n", epoch, sum);
  }
  return 0;
}
