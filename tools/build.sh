#!/bin/sh
# Builds TERRAIN for Digitone II from source with digikit (patched with digikit/digikit-terrain.patch).
# Usage: tools/build.sh DIGIKIT_DIR Digitone_II_OS1.12.syx OUT.syx
# Build and preflight only: this script never sends anything to the instrument.
set -e
REPO=$(cd "$(dirname "$0")/.." && pwd)
DK=$(cd "$1" && pwd); ORIG=$(cd "$(dirname "$2")" && pwd)/$(basename "$2"); OUT=$3
cd "$DK"
export PATCHKIT_MODULES="$REPO/modules"
uv run python -m tools.patchkit.patchkit build --force modules/core/adapters/dsp-voice modules/core/adapters/params modules/core/adapters/machines
for m in terreno pagine intro lfolength reloadconfirm fxmod arpmodes arpscale midiarp midiout; do
  uv run python -m tools.patchkit.patchkit build --force "$REPO/modules/$m"
done
uv run python -m tools.patchkit.patchkit patch --firmware "$ORIG" \
  --modules em/terreno,em/pagine,em/midiout,em/intro,em/lfolength,em/reloadconfirm,em/moddest,em/fxmod,em/arpmodes,em/arpscale,em/midiarp \
  -o "$OUT"
uv run python -m tools.patchkit.patchkit preflight "$OUT"
shasum -a 256 "$OUT" 2>/dev/null || sha256sum "$OUT"
