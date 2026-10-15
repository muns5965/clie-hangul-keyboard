#!/bin/bash
# SkWr 로더 빌드 → bin/MnSkPlugin.prc (타입 SkWr, 크리에이터 MnSk, 리소스 code 0)
#
# SilkExtn이 code 0의 맨 앞 주소로 바로 jsr 하므로 시작 코드·데이터 세그먼트 없이 code 0 하나만 만든다.
# 로더는 한 번 설치하면 바꾸지 않는다(교체에는 DisS·리셋이 필요). 자판 본체는 build-core.sh.
# 빌드 후 다음을 검사하고, 하나라도 어긋나면 실패한다.
#   - .data/.bss 크기 0 (전역변수 금지)
#   - 진입 함수 LoaderEntry가 .text 오프셋 0
#   - A5 상대 접근(전역변수 참조) 없음
#   - 재배치 항목 없음 (다른 DB에서 불러와도 그대로 실행되는 위치 독립 코드)
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/build/loader"
rm -rf "$out"
mkdir -p "$out" "$root/bin"
cd "$out"

m68k-palmos-gcc -palmos4 -O2 -Wall -Wno-multichar \
  -fno-builtin -nostartfiles -nostdlib \
  -Wl,-e,LoaderEntry \
  "$root/src/skwr/loader.c" -o skwr.coff

# 전역변수 검사
data=$(m68k-palmos-size skwr.coff | awk 'NR==2 {print $2 + $3}')
if [ "$data" != "0" ]; then
  echo "error: data+bss = $data (globals are not allowed)" >&2
  exit 1
fi

# 진입 함수 위치 검사
entry=$(m68k-palmos-nm skwr.coff | awk '$3=="LoaderEntry" {print $1}')
text=$(m68k-palmos-objdump -h skwr.coff | awk '$2==".text" {print $4}')
if [ -z "$entry" ] || [ "$((16#$entry))" -ne "$((16#$text))" ]; then
  echo "error: LoaderEntry ($entry) is not at start of .text ($text)" >&2
  exit 1
fi

# A5 상대 접근 검사
if m68k-palmos-objdump -d skwr.coff | grep -q '%a5'; then
  echo "error: code references %a5 (globals)" >&2
  m68k-palmos-objdump -d skwr.coff | grep '%a5' >&2
  exit 1
fi

# 재배치 검사
if m68k-palmos-objdump -r skwr.coff | grep -qE '^[0-9a-f]{8} '; then
  echo "error: relocations found " >&2
  m68k-palmos-objdump -r skwr.coff >&2
  exit 1
fi

m68k-palmos-objcopy -O binary -j .text skwr.coff code0000.bin
m68k-palmos-objdump -d skwr.coff > skwr.dis

build-prc --no-check-resources -t SkWr \
  "$root/bin/MnSkPlugin.prc" "MnSkPlugin" MnSk code0000.bin

ls -l "$root/bin/MnSkPlugin.prc"
