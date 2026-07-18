#!/usr/bin/env python3
"""Assert the Python constituent table matches the C++ one exactly.

The firmware (tide_constituents.cpp) and the generator (constituent_index.py)
each carry the constituent list; if they drift, the generator could emit a
string the firmware silently mis-decodes. This guards against that.
"""
import re
import sys

from constituent_index import CONSTITUENTS

CPP = "../components/tide/tide_constituents.cpp"
ROW = re.compile(r'\{"([^"]+)",\s*\{([-0-9,\s]+)\},\s*NODE_([A-Z0-9_]+)\}')


def main():
    with open(CPP, encoding="utf-8") as fh:
        text = fh.read()
    cpp_rows = []
    for m in ROW.finditer(text):
        name = m.group(1)
        coeff = tuple(int(x) for x in m.group(2).split(","))
        node = m.group(3)
        cpp_rows.append((name, coeff, node))

    py_rows = [(n, tuple(c), node) for n, c, node in CONSTITUENTS]

    if cpp_rows == py_rows:
        print(f"OK: {len(cpp_rows)} constituents match between C++ and Python")
        return
    print("MISMATCH between tide_constituents.cpp and constituent_index.py:")
    for i, (a, b) in enumerate(zip(cpp_rows, py_rows)):
        if a != b:
            print(f"  row {i}: C++={a}  Python={b}")
    if len(cpp_rows) != len(py_rows):
        print(f"  length differs: C++={len(cpp_rows)} Python={len(py_rows)}")
    sys.exit(1)


if __name__ == "__main__":
    main()
