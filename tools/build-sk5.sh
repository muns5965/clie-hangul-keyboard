#!/bin/bash
# Mn 실크 키보드(OS 5 Sony Silk Plug-in) 빌드 → bin/MnSk5.prc (타입 Slkw, 크리에이터 MnS5, 리소스 code 0)
#
# Silk Manager가 code 0의 맨 앞 주소를 진입점으로 부르므로(docs/OS5_RESEARCH.md 9절) 시작 코드·데이터 세그먼트 없이 code 0 하나만 만든다.
# OS 5 고해상도 함수(WinSetCoordinateSystem·BmpCreateBitmapV3)를 쓰므로 Palm OS SDK 5r3로 빌드한다.
# 빌드 후 다음을 검사하고, 하나라도 어긋나면 실패한다.
#   - .data/.bss 크기 0 (전역변수 금지)
#   - 진입 함수 Sk5Entry가 .text 오프셋 0
#   - A5 상대 접근(전역변수 참조) 없음
#   - 재배치 항목 없음 (위치 독립 코드)
#   - kSkMark 값이 MnHangul5(src/os5/ime.c)와 같음
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/build/sk5"
rm -rf "$out"
mkdir -p "$out" "$root/bin"
cd "$out"

a=$(grep -oE '#define kSkMark +0x[0-9A-Fa-f]+' "$root/src/skwr5/sk5.c" | awk '{print $3}')
b=$(grep -oE '#define kSkMark +0x[0-9A-Fa-f]+' "$root/src/os5/ime.c" | awk '{print $3}')
if [ -z "$a" ] || [ "$a" != "$b" ]; then
  echo "error: kSkMark differs: sk5.c=$a ime.c=$b" >&2
  exit 1
fi

m68k-palmos-gcc -palmos5r3 ${SK5_CFLAGS:-} -Wa,-I"$root/src/sony/data" -O2 -Wall -Wno-multichar \
  -fno-builtin -nostartfiles -nostdlib \
  -Wl,-e,Sk5Entry \
  "$root/src/skwr5/sk5.c" -o sk5.coff

data=$(m68k-palmos-size sk5.coff | awk 'NR==2 {print $2 + $3}')
if [ "$data" != "0" ]; then
  echo "error: data+bss = $data (globals are not allowed)" >&2
  exit 1
fi

entry=$(m68k-palmos-nm sk5.coff | awk '$3=="Sk5Entry" {print $1}')
text=$(m68k-palmos-objdump -h sk5.coff | awk '$2==".text" {print $4}')
if [ -z "$entry" ] || [ "$((16#$entry))" -ne "$((16#$text))" ]; then
  echo "error: Sk5Entry ($entry) is not at start of .text ($text)" >&2
  exit 1
fi

if m68k-palmos-objdump -d sk5.coff | grep -q '%a5'; then
  echo "error: code references %a5 (globals)" >&2
  m68k-palmos-objdump -d sk5.coff | grep '%a5' >&2
  exit 1
fi

if m68k-palmos-objdump -r sk5.coff | grep -qE '^[0-9a-f]{8} '; then
  echo "error: relocations found (plug-in must be position independent)" >&2
  m68k-palmos-objdump -r sk5.coff >&2
  exit 1
fi

m68k-palmos-objcopy -O binary -j .text sk5.coff code0000.bin
m68k-palmos-objdump -d sk5.coff > sk5.dis

build-prc --no-check-resources -t Slkw \
  "$root/bin/MnSk5.prc" "Mn Keyboard" MnS5 code0000.bin

ls -l "$root/bin/MnSk5.prc"
