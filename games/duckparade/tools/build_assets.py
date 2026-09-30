#!/usr/bin/env python3
"""Duck Parade asset build (stub, replaced by the real one)."""
import argparse, os
ap = argparse.ArgumentParser()
ap.add_argument("--out", required=True)
ap.add_argument("--art", default="")
ap.add_argument("--tileset", default="")
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)
open(os.path.join(a.out, "assets.h"), "w").write('#ifndef DP_ASSETS_H\n#define DP_ASSETS_H\n#include "rs.h"\nextern const rs_asset_entry dp_assets[];\n#endif\n')
open(os.path.join(a.out, "assets.c"), "w").write('#include "assets.h"\nconst rs_asset_entry dp_assets[] = {{0, 0, 0}};\n')
