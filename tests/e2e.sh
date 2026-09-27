#!/usr/bin/env bash
# End-to-end tests: run the real binary against throwaway folders.
# Usage: tests/e2e.sh [path/to/filemgr]
set -u

BIN="$(cd "$(dirname "${1:-./filemgr}")" && pwd)/$(basename "${1:-./filemgr}")"
WORK="$(mktemp -d /tmp/filemgr-e2e-XXXXXX)"
trap 'rm -rf "$WORK"' EXIT

export FILEMGR_STATE_DIR="$WORK/state"
export FILEMGR_CONFIG="$WORK/config"
export NO_COLOR=1
unset FILEMGR_ROOT

PASS=0
FAIL=0
DL=""

fresh() {
    DL="$WORK/dl-$RANDOM$RANDOM"
    mkdir -p "$DL"
}

run() { "$BIN" --path "$DL" "$@"; }

check() {
    local desc="$1"; shift
    if "$@"; then
        PASS=$((PASS + 1)); echo "  ok   $desc"
    else
        FAIL=$((FAIL + 1)); echo "  FAIL $desc"
    fi
}

# --- usage and errors -------------------------------------------------------
"$BIN" >/dev/null 2>&1;            check "no args exits 1"            [ $? -eq 1 ]
"$BIN" --help >/dev/null;          check "--help exits 0"             [ $? -eq 0 ]
"$BIN" --version | grep -q filemgr; check "--version prints name"     [ $? -eq 0 ]
"$BIN" bogus 2>/dev/null;          check "unknown command exits 2"    [ $? -eq 2 ]
fresh; run old abc 2>/dev/null;    check "bad number exits 2"         [ $? -eq 2 ]
"$BIN" --path "$WORK/nope" sort 2>/dev/null; check "missing root exits 1" [ $? -eq 1 ]
"$BIN" sort --help | grep -q "Usage: filemgr sort"; check "per-command help" [ $? -eq 0 ]

# --- sort -------------------------------------------------------------------
fresh
touch "$DL/a.JPG" "$DL/b.pdf" "$DL/c.unknown"
mkdir -p "$DL/PROTECTED" && touch "$DL/PROTECTED/d.png"
run -n sort >/dev/null
check "dry run leaves files"        [ -f "$DL/a.JPG" ]
check "dry run creates no folders"  [ ! -d "$DL/IMAGES" ]
run sort >/dev/null
check "sort moves images"           [ -f "$DL/IMAGES/a.JPG" ]
check "sort moves documents"        [ -f "$DL/DOCUMENTS/b.pdf" ]
check "sort leaves unknown types"   [ -f "$DL/c.unknown" ]
check "sort ignores user folders"   [ -f "$DL/PROTECTED/d.png" ]

# --- undo -------------------------------------------------------------------
run undo >/dev/null
check "undo restores image"         [ -f "$DL/a.JPG" ]
check "undo restores document"      [ -f "$DL/b.pdf" ]
run history | grep -q "undone by";  check "history shows undone run" [ $? -eq 0 ]

# --- dedup ------------------------------------------------------------------
fresh
echo same > "$DL/one.txt"; echo same > "$DL/two.txt"; echo other > "$DL/three.txt"
out="$(run dedup)"
check "dedup moves one copy"        [ "$(ls "$DL/DUPLICATES" | wc -l | tr -d ' ')" = "1" ]
check "dedup keeps unique file"     [ -f "$DL/three.txt" ]
echo "$out" | grep -q "Moved 1 duplicate"; check "dedup summary" [ $? -eq 0 ]

# --- old --------------------------------------------------------------------
fresh
touch -t 202001010000 "$DL/ancient.txt"; touch "$DL/new.txt"
run old 30 >/dev/null
check "old archives stale file"     [ -f "$DL/OLD/ancient.txt" ]
check "old keeps fresh file"        [ -f "$DL/new.txt" ]

# --- config -----------------------------------------------------------------
fresh
"$BIN" config init >/dev/null;      check "config init writes file" [ -f "$FILEMGR_CONFIG" ]
printf '[categories]\nSHEETS = csv\n' >> "$FILEMGR_CONFIG"
touch "$DL/data.csv"
run sort >/dev/null
check "custom category used"        [ -f "$DL/SHEETS/data.csv" ]
printf '[bad\n' >> "$FILEMGR_CONFIG"
run sort 2>/dev/null;               check "bad config exits 1" [ $? -eq 1 ]
rm -f "$FILEMGR_CONFIG"

# --- quiet ------------------------------------------------------------------
fresh; touch "$DL/x.png"
out="$(run -q sort)"
check "quiet prints only summary"   [ "$out" = "Sorted 1 file" ]

echo
echo "$PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
