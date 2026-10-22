#!/bin/bash
# MnSkKbd(Mn 가상 키보드 설정) → bin/MnSkKbd.prc (크리에이터 MnSw)
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$root/build/skkbd" "$root/bin"
cd "$root/build/skkbd"

m68k-palmos-gcc -palmos4 -O2 -Wall -Wno-multichar "$root/src/skkbd/app.c" -o app 2>&1 | tee build.log
m68k-palmos-obj-res app
build-prc "$root/bin/MnSkKbd.prc" "MnSkKbd" MnSw *.app.grc
ls -l "$root/bin/MnSkKbd.prc"
