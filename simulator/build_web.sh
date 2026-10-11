#!/bin/sh
set -eu
cd "$(dirname "$0")"
command -v emcc >/dev/null || { echo 'Activate emsdk 4.0.15 first (source emsdk_env.sh).'; exit 1; }
test -d third_party/lvgl/src || { echo 'Run make bootstrap first.'; exit 1; }
make -f web.mk -j6 web
make -f web.mk ORIENTATION=portrait -j6 web
mkdir -p web/dist/maps
cp ../assets/maps/highfield.osm ../assets/maps/README.md web/dist/maps/
mkdir -p web/dist/vendor
cp web/index.html web/app.js web/vercel.json web/dist/
# Tie the loader and WASM URLs to their contents so an open browser cannot
# reuse an older firmware binary after a local rebuild or deployment.
python3 - <<'PYBUILD'
from pathlib import Path
import hashlib
out = Path('web/dist')
inputs = ['r1-portrait.wasm', 'r1-landscape.wasm', 'r1-portrait.js', 'r1-landscape.js', 'app.js']
build_id = hashlib.sha256(b''.join((out / name).read_bytes() for name in inputs)).hexdigest()[:16]
for name in ['app.js', 'index.html']:
    path = out / name
    path.write_text(path.read_text().replace('__R1_BUILD_ID__', build_id))
PYBUILD
cp web/vendor/* web/dist/vendor/
mkdir -p web/dist/trial
cp web/trial/index.html web/trial/trial.css web/trial/trial.js web/trial/pixel-preview.js web/trial/channels-data.js web/dist/trial/
{
    printf 'LVGL 8.4.0\n\n'
    cat third_party/lvgl/LICENCE.txt
    printf '\n\nblueimp-md5 2.19.0\n\n'
    cat web/vendor/blueimp-md5-LICENSE.txt
} > web/dist/third-party-notices.txt
printf 'Standalone demo ready in simulator/web/dist/\n'
