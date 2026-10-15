#!/bin/bash
# SilkWare 플러그인 빌드 → bin/MnSkPlugin.prc (타입 SkWr, 크리에이터 MnSk)
#
# 일반 앱과 달리 시작 코드(crt0)·데이터 세그먼트 없이 code 0 리소스 하나만 만든다.
# SilkExtn이 code 0의 맨 앞 주소로 바로 jsr 하기 때문이다.
# 빌드 후 다음을 검사하고, 하나라도 어긋나면 실패한다.
#   - .data/.bss 크기 0 (전역변수 금지)
#   - 진입 함수 SkwrEntry가 .text 오프셋 0
#   - A5 상대 접근(전역변수 참조) 없음
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/build/skwr"
rm -rf "$out"
mkdir -p "$out" "$root/bin"
cd "$out"

m68k-palmos-gcc -palmos4 -O2 -Wall -Wno-multichar \
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

m68k-palmos-objcopy -O binary -j .text skwr.coff code0000.bin
m68k-palmos-objdump -d skwr.coff > skwr.dis

build-prc --no-check-resources -t SkWr \
  "$root/bin/MnSkPlugin.prc" "MnSkPlugin" MnSk code0000.bin

ls -l "$root/bin/MnSkPlugin.prc"
cat skwr.dis
