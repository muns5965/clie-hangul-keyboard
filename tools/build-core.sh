#!/bin/bash
# 자판 본체 빌드 → bin/MnSkCore.prc (타입 SkCo, 크리에이터 MnSk, 리소스 MnCo 0, DB 버전 = 로더 ABI)
#
# 시작 코드(crt0)·데이터 세그먼트 없이 코드 리소스 하나만 만든다.
# 로더(MnSkPlugin.prc)가 이 리소스를 잠가 맨 앞 주소로 바로 jsr 한다.
# 리소스 타입을 'code'가 아닌 'MnCo'로 하는 것은, 본체 DB가 열린 순간 앱의 DmGetResource('code', …)에
# 잘못 걸리지 않게 하기 위해서다.
# 빌드 후 다음을 검사하고, 하나라도 어긋나면 실패한다.
#   - .data/.bss 크기 0 (전역변수 금지)
#   - 진입 함수 SkwrEntry가 .text 오프셋 0
#   - A5 상대 접근(전역변수 참조) 없음
#   - 재배치 항목 없음 (다른 DB에서 불러와도 그대로 실행되는 위치 독립 코드)
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
# 진단 빌드(기본, DIAG=1) → bin/. 배포 빌드(DIAG=0: 진단 기록을 뺌, src/common/diag.h) → build/release/
DIAG="${DIAG:-1}"
DFLAG="-DMN_DIAG=$DIAG"
outbin="$root/bin"
out="$root/build/core"
if [ "$DIAG" = "0" ]; then
  outbin="$root/build/release"
  out="$root/build/core-release"
fi
mkdir -p "$outbin"
rm -rf "$out"
mkdir -p "$out" "$root/bin"
cd "$out"

m68k-palmos-gcc -palmos4 $DFLAG -O2 -Wall -Wno-multichar \
  -fno-builtin -nostartfiles -nostdlib \
  -Wl,-e,SkwrEntry \
  "$root/src/skwr/skwr.c" -o skwr.coff

# 전역변수 검사
data=$(m68k-palmos-size skwr.coff | awk 'NR==2 {print $2 + $3}')
if [ "$data" != "0" ]; then
  echo "error: data+bss = $data (globals are not allowed)" >&2
  exit 1
fi

# 진입 함수 위치 검사
entry=$(m68k-palmos-nm skwr.coff | awk '$3=="SkwrEntry" {print $1}')
text=$(m68k-palmos-objdump -h skwr.coff | awk '$2==".text" {print $4}')
if [ -z "$entry" ] || [ "$((16#$entry))" -ne "$((16#$text))" ]; then
  echo "error: SkwrEntry ($entry) is not at start of .text ($text)" >&2
  exit 1
fi

# A5 상대 접근 검사
if m68k-palmos-objdump -d skwr.coff | grep -q '%a5'; then
  echo "error: code references %a5 (globals)" >&2
  m68k-palmos-objdump -d skwr.coff | grep '%a5' >&2
  exit 1
fi

# 재배치 검사 (위치 독립)
if m68k-palmos-objdump -r skwr.coff | grep -qE '^[0-9a-f]{8} '; then
  echo "error: relocations found (core must be position independent)" >&2
  m68k-palmos-objdump -r skwr.coff >&2
  exit 1
fi

m68k-palmos-objcopy -O binary -j .text skwr.coff MnCo0000.bin
m68k-palmos-objdump -d skwr.coff > skwr.dis

# DB 버전 1 = 로더 ABI. 로더(loader.c kCoreAbi)와 맞아야 불러온다.
build-prc --no-check-resources -t SkCo -v 1 \
  "$outbin/MnSkCore.prc" "MnSkCore" MnSk MnCo0000.bin

ls -l "$outbin/MnSkCore.prc"
cat skwr.dis
