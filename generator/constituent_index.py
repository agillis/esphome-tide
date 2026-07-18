"""Canonical tidal-constituent table.

This is the single Python source of truth that MUST stay in lock-step with the
C++ table in ``components/tide/tide_constituents.cpp``. Doodson coefficients are
in the pytides spanning-set order ``[T+h-s, s, h, p, N, pp, 90]`` and the node
type selects the Schureman node-factor/phase correction.

``tools/check_tables.py`` asserts this list matches the C++ file exactly.
"""

# (name, doodson-coefficients, node-type) — order mirrors the C++ array.
CONSTITUENTS = [
    ("M2", (2, 0, 0, 0, 0, 0, 0), "M2"),
    ("S2", (2, 2, -2, 0, 0, 0, 0), "UNITY"),
    ("N2", (2, -1, 0, 1, 0, 0, 0), "M2"),
    ("K1", (1, 1, 0, 0, 0, 0, -1), "K1"),
    ("M4", (4, 0, 0, 0, 0, 0, 0), "M2_POW2"),
    ("O1", (1, -1, 0, 0, 0, 0, 1), "O1"),
    ("M6", (6, 0, 0, 0, 0, 0, 0), "M2_POW3"),
    ("MK3", (3, 1, 0, 0, 0, 0, -1), "MK3"),
    ("S4", (4, 4, -4, 0, 0, 0, 0), "UNITY"),
    ("MN4", (4, -1, 0, 1, 0, 0, 0), "M2_POW2"),
    ("NU2", (2, -1, 2, -1, 0, 0, 0), "M2"),
    ("S6", (6, 6, -6, 0, 0, 0, 0), "UNITY"),
    ("MU2", (2, -2, 2, 0, 0, 0, 0), "M2_POW2"),
    ("2N2", (2, -2, 0, 2, 0, 0, 0), "M2"),
    ("OO1", (1, 3, 0, 0, 0, 0, -1), "OO1"),
    ("LAM2", (2, 1, -2, 1, 0, 0, 2), "M2"),
    ("S1", (1, 1, -1, 0, 0, 0, 0), "UNITY"),
    ("M1", (1, 0, 0, 0, 0, 0, 1), "M1"),
    ("J1", (1, 2, 0, -1, 0, 0, -1), "J1"),
    ("MM", (0, 1, 0, -1, 0, 0, 0), "MM"),
    ("SSA", (0, 0, 2, 0, 0, 0, 0), "UNITY"),
    ("SA", (0, 0, 1, 0, 0, 0, 0), "UNITY"),
    ("MSF", (0, 2, -2, 0, 0, 0, 0), "MSF"),
    ("MF", (0, 2, 0, 0, 0, 0, 0), "MF"),
    ("RHO", (1, -2, 2, -1, 0, 0, 1), "RHO1"),
    ("Q1", (1, -2, 0, 1, 0, 0, 1), "O1"),
    ("T2", (2, 2, -3, 0, 0, 1, 0), "UNITY"),
    ("R2", (2, 2, -1, 0, 0, -1, 2), "UNITY"),
    ("2Q1", (1, -3, 0, 2, 0, 0, 1), "2Q1"),
    ("P1", (1, 1, -2, 0, 0, 0, 1), "UNITY"),
    ("2SM2", (2, 4, -4, 0, 0, 0, 0), "MSF"),
    ("M3", (3, 0, 0, 0, 0, 0, 0), "M3"),
    ("L2", (2, 1, 0, -1, 0, 0, 2), "L2"),
    ("2MK3", (3, -1, 0, 0, 0, 0, 1), "2MK3"),
    ("K2", (2, 2, 0, 0, 0, 0, 0), "K2"),
    ("M8", (8, 0, 0, 0, 0, 0, 0), "M2_POW4"),
    ("MS4", (4, 2, -2, 0, 0, 0, 0), "MS4"),
]

CANONICAL = {name for name, _, _ in CONSTITUENTS}
BY_NAME = {name: (coeff, node) for name, coeff, node in CONSTITUENTS}

# Alternate spellings from various databases -> canonical name.
ALIASES = {
    "LAMBDA2": "LAM2",
    "LDA2": "LAM2",
    "RHO1": "RHO",
}


def normalize(name: str) -> str:
    """Upper-case, strip spaces, and map known aliases to canonical names."""
    s = "".join(name.split()).upper()
    return ALIASES.get(s, s)
