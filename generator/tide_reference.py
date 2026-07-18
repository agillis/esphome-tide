"""Pure-Python reference implementation of the on-device tide engine.

This is a faithful port of ``components/tide/tide_astronomy.cpp`` and the height
/ extrema logic in ``tide.cpp``. It lets us validate the algorithm (and a
generated station string) on a PC without flashing hardware. Because it uses the
exact same formulas as the firmware, agreement between this module and the
device is expected to be at the float-vs-double level (< 1 mm).
"""

import math

from constituent_index import BY_NAME, normalize

D2R = math.pi / 180.0
R2D = 180.0 / math.pi

C_S = [218.3164591, 481267.88134236, -0.0013268, 1.0 / 538841.0, -1.0 / 65194000.0]
C_H = [280.46645, 36000.76983, 0.0003032]
C_P = [83.3532430, 4069.0137111, -0.0103238, -1.0 / 80053.0, 1.0 / 18999000.0]
C_N = [125.0445550, -1934.1361849, 0.0020762, 1.0 / 467410.0, -1.0 / 60616000.0]
C_PP = [280.46645 - 357.52910, 36000.76932 - 35999.05030, 0.0003032 + 0.0001559, 0.00000048]
C_OM = [23.4392794444, -0.0130025833, -4.30555556e-8, 5.55347222e-7]
LUNAR_INCLINATION = 5.145


def _poly(c, x):
    return sum(ci * x**i for i, ci in enumerate(c))


def _dpoly(c, x):
    return sum(ci * i * x ** (i - 1) for i, ci in enumerate(c) if i >= 1)


def _wrap360(x):
    return x % 360.0


def compute_astro(epoch):
    """Return an astro dict matching the C++ ``Astro`` struct."""
    jd = 2440587.5 + epoch / 86400.0
    T = (jd - 2451545.0) / 36525.0
    dT_dHour = 1.0 / (24.0 * 365.25 * 100.0)

    s, ds = _poly(C_S, T), _dpoly(C_S, T) * dT_dHour
    h, dh = _poly(C_H, T), _dpoly(C_H, T) * dT_dHour
    p, dp = _poly(C_P, T), _dpoly(C_P, T) * dT_dHour
    N, dN = _poly(C_N, T), _dpoly(C_N, T) * dT_dHour
    pp, dpp = _poly(C_PP, T), _dpoly(C_PP, T) * dT_dHour
    omega = _poly(C_OM, T)
    incl = LUNAR_INCLINATION

    hour_val = (jd - math.floor(jd)) * 360.0
    ths = hour_val + h - s
    ths_spd = 15.0 + dh - ds

    val = [_wrap360(ths), _wrap360(s), _wrap360(h), _wrap360(p), _wrap360(N), _wrap360(pp), 90.0]
    spd = [ths_spd, ds, dh, dp, dN, dpp, 0.0]

    Nr, omr, ir = val[4] * D2R, omega * D2R, incl * D2R
    cosI = math.cos(ir) * math.cos(omr) - math.sin(ir) * math.sin(omr) * math.cos(Nr)
    I = math.acos(cosI)

    e1 = math.atan(math.cos(0.5 * (omr - ir)) / math.cos(0.5 * (omr + ir)) * math.tan(0.5 * Nr))
    e2 = math.atan(math.sin(0.5 * (omr - ir)) / math.sin(0.5 * (omr + ir)) * math.tan(0.5 * Nr))
    e1 -= 0.5 * Nr
    e2 -= 0.5 * Nr
    xi = -(e1 + e2)
    nu = e1 - e2

    nup = math.atan(math.sin(2 * I) * math.sin(nu) / (math.sin(2 * I) * math.cos(nu) + 0.3347))
    nupp = 0.5 * math.atan(
        math.sin(I) ** 2 * math.sin(2 * nu) / (math.sin(I) ** 2 * math.cos(2 * nu) + 0.0727)
    )

    I_deg, xi_deg, nu_deg = R2D * I, R2D * xi, R2D * nu
    nup_deg, nupp_deg = R2D * nup, R2D * nupp
    P_deg = _wrap360(val[3] - xi_deg)

    Ir, Pr = I_deg * D2R, P_deg * D2R
    sin2i = math.sin(ir) ** 2
    one_minus = 1.0 - 1.5 * sin2i

    a = {"val": val, "spd": spd}

    fM2 = math.cos(0.5 * Ir) ** 4 / (math.cos(0.5 * omr) ** 4 * math.cos(0.5 * ir) ** 4)
    a["f_M2"], a["u_M2"] = fM2, 2.0 * xi_deg - 2.0 * nu_deg

    fO1 = (math.sin(Ir) * math.cos(0.5 * Ir) ** 2) / (
        math.sin(omr) * math.cos(0.5 * omr) ** 2 * math.cos(0.5 * ir) ** 4
    )
    a["f_O1"], a["u_O1"] = fO1, 2.0 * xi_deg - nu_deg

    k1_den = 0.5023 * math.sin(2 * omr) * one_minus + 0.1681
    a["f_K1"] = (
        math.sqrt(0.2523 * math.sin(2 * Ir) ** 2 + 0.1689 * math.sin(2 * Ir) * math.cos(nu) + 0.0283) / k1_den
    )
    a["u_K1"] = -nup_deg

    a["f_J1"] = math.sin(2 * Ir) / (math.sin(2 * omr) * one_minus)
    a["u_J1"] = -nu_deg

    k2_den = 0.5023 * math.sin(omr) ** 2 * one_minus + 0.0365
    a["f_K2"] = (
        math.sqrt(0.2523 * math.sin(Ir) ** 4 + 0.0367 * math.sin(Ir) ** 2 * math.cos(2 * nu) + 0.0013) / k2_den
    )
    a["u_K2"] = -2.0 * nupp_deg

    ra_inv = math.sqrt(1.0 - 12.0 * math.tan(0.5 * Ir) ** 2 * math.cos(2 * Pr) + 36.0 * math.tan(0.5 * Ir) ** 4)
    a["f_L2"] = fM2 * ra_inv
    R = R2D * math.atan(math.sin(2 * Pr) / ((1.0 / 6.0) * math.tan(0.5 * Ir) ** -2 - math.cos(2 * Pr)))
    a["u_L2"] = 2.0 * xi_deg - 2.0 * nu_deg - R

    qa_inv = math.sqrt(
        0.25
        + 1.5 * math.cos(Ir) * math.cos(2 * Pr) * math.cos(0.5 * Ir) ** -0.5
        + 2.25 * math.cos(Ir) ** 2 * math.cos(0.5 * Ir) ** -4
    )
    a["f_M1"] = fO1 * qa_inv
    Q = R2D * math.atan((5 * math.cos(Ir) - 1) / (7 * math.cos(Ir) + 1) * math.tan(Pr))
    a["u_M1"] = xi_deg - nu_deg + Q

    mm_mean = (2.0 / 3.0 - math.sin(omr) ** 2) * one_minus
    a["f_Mm"] = (2.0 / 3.0 - math.sin(Ir) ** 2) / mm_mean

    mf_mean = math.sin(omr) ** 2 * math.cos(0.5 * ir) ** 4
    a["f_Mf"] = math.sin(Ir) ** 2 / mf_mean
    a["u_Mf"] = -2.0 * xi_deg

    oo1_mean = math.sin(omr) * math.sin(0.5 * omr) ** 2 * math.cos(0.5 * ir) ** 4
    a["f_OO1"] = math.sin(Ir) * math.sin(0.5 * Ir) ** 2 / oo1_mean
    a["u_OO1"] = -2.0 * xi_deg - nu_deg
    return a


def node_correction(node, a):
    if node == "UNITY":
        return 1.0, 0.0
    if node == "M2":
        return a["f_M2"], a["u_M2"]
    if node == "M2_POW2":
        return a["f_M2"] ** 2, 2.0 * a["u_M2"]
    if node == "M2_POW3":
        return a["f_M2"] ** 3, 3.0 * a["u_M2"]
    if node == "M2_POW4":
        return a["f_M2"] ** 4, 4.0 * a["u_M2"]
    if node == "M3":
        return a["f_M2"] ** 1.5, 1.5 * a["u_M2"]
    if node == "MSF":
        return a["f_M2"], -a["u_M2"]
    if node == "MS4":
        return a["f_M2"], a["u_M2"]
    if node == "MK3":
        return a["f_M2"] * a["f_K1"], a["u_M2"] + a["u_K1"]
    if node == "2MK3":
        return a["f_M2"] * a["f_O1"], a["u_M2"] + a["u_O1"]
    if node == "RHO1":
        return a["f_M2"] * a["f_K1"], a["u_M2"] - a["u_K1"]
    if node == "2Q1":
        return a["f_M2"] * a["f_J1"], a["u_M2"] - a["u_J1"]
    if node == "O1":
        return a["f_O1"], a["u_O1"]
    if node == "K1":
        return a["f_K1"], a["u_K1"]
    if node == "J1":
        return a["f_J1"], a["u_J1"]
    if node == "M1":
        return a["f_M1"], a["u_M1"]
    if node == "OO1":
        return a["f_OO1"], a["u_OO1"]
    if node == "L2":
        return a["f_L2"], a["u_L2"]
    if node == "K2":
        return a["f_K2"], a["u_K2"]
    if node == "MM":
        return a["f_Mm"], 0.0
    if node == "MF":
        return a["f_Mf"], a["u_Mf"]
    return 1.0, 0.0


class Predictor:
    """Evaluate a station's tide height. `constituents` = list of (name, H, kappa)."""

    def __init__(self, constituents, z0=0.0):
        self.constituents = [(normalize(n), H, k) for n, H, k in constituents]
        self.z0 = z0

    def _params(self, t0):
        a = compute_astro(t0)
        out = []
        for name, H, kappa in self.constituents:
            if name not in BY_NAME:
                raise KeyError(f"unknown constituent {name}")
            coeff, node = BY_NAME[name]
            f, u = node_correction(node, a)
            v0 = sum(coeff[k] * a["val"][k] for k in range(7))
            speed = sum(coeff[k] * a["spd"][k] for k in range(7))
            out.append((H * f, speed * D2R, (v0 + u - kappa) * D2R))
        return out

    def height(self, epoch, t0=None):
        """Height (metres above chart datum) at `epoch`. `t0` is the reference
        instant for node factors (default: `epoch` itself)."""
        if t0 is None:
            t0 = epoch
        params = self._params(t0)
        dt = (epoch - t0) / 3600.0
        return self.z0 + sum(A * math.cos(w * dt + phi) for A, w, phi in params)
