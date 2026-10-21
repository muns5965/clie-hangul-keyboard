#!/bin/bash
# MnKbdHan(시스템 키보드 화면 한글) → bin/MnKbdHan.prc (크리에이터 MnKh)
#   1) 자모 표시 코드(src/kbdhan/hook.c) → 리소스 KhHk 0. 오프셋 0 = 진입점 오프셋 표(4개), 16 = 원래 주소 칸, 32 = 버퍼 주소
#   2) 앱(src/kbdhan/app.c)
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/build/kbdhan"
mkdir -p "$out" "$root/bin"
cd "$out"

m68k-palmos-gcc -palmos4 -O2 -Wall -Wno-multichar \
  -fno-builtin -nostartfiles -nostdlib \
  -Wl,-e,KhKbd \
  "$root/src/kbdhan/hook.c" -o hook.coff
data=$(m68k-palmos-size hook.coff | awk 'NR==2 {print $2 + $3}')
[ "$data" = "0" ] || { echo "error: hook data+bss = $data" >&2; exit 1; }
for s in KhBase:0 KhOrig:16 KhBuf:32; do
  a=$(m68k-palmos-nm hook.coff | awk -v n="${s%%:*}" '$3==n {print $1}')
  [ "$((16#$a))" -eq "${s##*:}" ] || { echo "error: ${s%%:*} at $a (must be ${s##*:})" >&2; exit 1; }
done
if m68k-palmos-objdump -d hook.coff | grep -q '%a5'; then
  echo "error: hook references %a5" >&2; exit 1
fi
if m68k-palmos-objdump -r hook.coff | grep -qE '^[0-9a-f]{8} '; then
  echo "error: hook has relocations" >&2; exit 1
fi
m68k-palmos-objcopy -O binary -j .text hook.coff KhHk0000.bin
m68k-palmos-objdump -d hook.coff > hook.dis

m68k-palmos-gcc -palmos4 -O2 -Wall -Wno-multichar "$root/src/kbdhan/app.c" -o app 2>&1 | tee build.log
m68k-palmos-obj-res app
build-prc "$root/bin/MnKbdHan.prc" "MnKbdHan" MnKh *.app.grc KhHk0000.bin
ls -l "$root/bin/MnKbdHan.prc"
