#!/usr/bin/env python3
"""Beaver Rush asset build (placeholder while the art is drawn): an empty asset pack.
(c) 2026 Pierre-Louis Boyer (8BCraft). All rights reserved: games/beaverrush/LICENSE.
"""
import argparse
import os

ap = argparse.ArgumentParser()
ap.add_argument("--out", required=True)
ap.add_argument("--art", default="")
ap.add_argument("--tileset", default="")
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)
open(os.path.join(a.out, "assets.h"), "w").write(
    "#ifndef BR_ASSETS_H\n#define BR_ASSETS_H\n#include \"rs.h\"\nextern const rs_asset_entry br_assets[];\n#endif\n")
open(os.path.join(a.out, "assets.c"), "w").write(
    "#include \"assets.h\"\nconst rs_asset_entry br_assets[] = {{0, 0, 0}};\n")
