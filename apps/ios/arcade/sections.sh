#!/bin/sh
# The Mach-O modules' section headers (apps/ios/arcade/ios.mk): sections.sh <dir> <module>...
# <dir>/<module>.h puts every writable variable of the module's objects into __DATA,__rsgd<n> (initialised) and
# __DATA,__rsgb<n> (zero), n its place in the list (00, 01...). Written only when its text changes.
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
set -eu
DIR=$1
shift
mkdir -p "$DIR"
n=0
for m in "$@"; do
    s=$(printf '%02d' "$n")
    line="#pragma clang section bss=\"__DATA,__rsgb$s\" data=\"__DATA,__rsgd$s\""
    if [ ! -f "$DIR/$m.h" ] || [ "$(cat "$DIR/$m.h")" != "$line" ]; then
        printf '%s\n' "$line" > "$DIR/$m.h"
    fi
    n=$((n + 1))
done
