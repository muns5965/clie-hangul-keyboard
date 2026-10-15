#!/bin/bash
# skwr.c 한글 조합 로직을 PC에서 시험 (Palm API는 test/host_stub.h 흉내)
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$root/build/test"
gcc -O1 -Wall -Wno-multichar -Wno-unused-function -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -shared -fPIC -I"$root/test" \
  "$root/test/compose_host.c" -o "$root/build/test/compose.so"
python3 "$root/test/compose_test.py" "$root/build/test/compose.so"
