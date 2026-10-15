#!/bin/bash
# 1단계 진단 PRC 빌드 → bin/ClieDiag.prc (크리에이터 MnDg)
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$root/build/diag" "$root/bin"
cd "$root/build/diag"

m68k-palmos-gcc -palmos4 -O2 -Wall -Wno-multichar \
  "$root/src/diag/diag.c" -o diag 2>&1 | tee build.log
m68k-palmos-obj-res diag
build-prc "$root/bin/ClieDiag.prc" "ClieDiag" MnDg *.diag.grc
ls -l "$root/bin/ClieDiag.prc"
