#!/bin/bash
# The language test (docs/i18n.md): runs a game's screens (title, pause, game over, the results...) in every language
# at every width, and fails when the house kit logs a layout error ("i18n: ...": text off the screen or over a panel's
# edge, an accent over other text, a glyph no font has, a full glyph pool) or the game fails.
#
#   i18n_test.sh <game>_headless <game> <out dir> [width...]       (widths: default 320)
#   LANGS="fr ja" i18n_test.sh ...                                   (default: the 22 languages)
#   SHOTS=1 i18n_test.sh ...                                         (keep the screenshots: <lang>-<width>-<screen>.png)
#
# The screens are games/<game>/tests/i18n_runs.txt, one headless run per line:
#   <name> <frames> <shot frame> [headless options...] [; <input script line> ; ...]
# e.g.   pause 640 630 --opt bot=1 --opt seed=7 --opt ready=1 ; 600 tap SELECT
# MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
H=$1
G=$2
O=${3:-/tmp/$G-i18n}
shift 3
WIDTHS=${*:-320}
LANGS=${LANGS:-en cs da de el es fi fr id it ja ko nb nl pl pt_BR pt_PT ru sv tr uk zh_CN}
CFG=$(dirname "$0")/../games/$G/tests/i18n_runs.txt
[ -x "$H" ] || { echo "i18n_test: no runner $H"; exit 2; }
[ -f "$CFG" ] || { echo "i18n_test: no $CFG"; exit 2; }
mkdir -p "$O"
rm -f "$O"/*.log "$O"/*.input "$O"/jobs.txt
n=0
while IFS= read -r line; do
    line=${line%%#*}
    [ -z "${line// }" ] && continue
    run=${line%%;*}
    input=""
    [ "$run" != "$line" ] && input=${line#*;}
    set -- $run
    name=$1 frames=$2 shot=$3
    shift 3
    if [ -n "$input" ]; then
        echo "$input" | tr ';' '\n' | sed 's/^ *//' > "$O/$name.input"
        set -- "$@" --input "$O/$name.input"
    fi
    for w in $WIDTHS; do
        for l in $LANGS; do
            echo "$l $w $name $frames $shot${*:+ $*}" >> "$O/jobs.txt"
            n=$((n + 1))
        done
    done
done < "$CFG"

one() {   # lang width name frames shot options...
    l=$1 w=$2 name=$3 frames=$4 shot=$5
    shift 5
    log="$O/$l-$w-$name.log"
    "$H" --opt music=0 --opt lang=$l --width "$w" --frames "$frames" --shot "$shot:$O/$l-$w-$name.png" "$@" > "$log" 2>&1
    rc=$?
    [ $rc -ne 0 ] && echo "i18n: the runner failed (exit $rc)" >> "$log"
}
export -f one
export H O
xargs -P "$(nproc 2>/dev/null || echo 4)" -L 1 bash -c 'one "$@"' _ < "$O/jobs.txt"

fail=$(grep -l "i18n: " "$O"/*.log 2>/dev/null | wc -l)
if [ "$fail" -gt 0 ]; then
    grep -h "i18n: " "$O"/*.log | sort | uniq -c | sort -rn | head -40
    echo "  FAIL $G: $fail of $n runs have layout errors (logs in $O)"
else
    echo "  ok   $G: $n runs (screens x languages x widths $WIDTHS), no layout error"
fi
[ -n "$SHOTS" ] || rm -f "$O"/*.png
[ "$fail" -eq 0 ]
