#!/bin/bash
# 도구 체인 동작 확인용: prc-tools-remix 예제(hello.c)를 PRC로 빌드
# 사용법: ./tools/build-hello.sh <prc-tools-remix 경로>
set -euo pipefail

src="${1:?prc-tools-remix 경로를 지정하세요}/tools/hello-world-app/hello.c"
out="$(cd "$(dirname "$0")/.." && pwd)/test"
mkdir -p "$out/build" && cd "$out/build"

m68k-palmos-gcc -palmos4 "$src" -o hello
m68k-palmos-obj-res hello
build-prc ../hello.prc "Hello, World" WRLD *.hello.grc
ls -l ../hello.prc
