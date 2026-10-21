#!/bin/bash
# KbdScan(다른 기종 화면 키보드 진단) → bin/KbdScan.prc (크리에이터 MnKd)
#   1) 기록 코드(src/kdiag/hook.c) → 리소스 KdHk 0. 오프셋 0 = 진입점 오프셋 표(11개), 44 = 원래 주소 칸, 88 = 버퍼 주소
#   2) 앱(src/kdiag/app.c)
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/build/kdiag"
mkdir -p "$out" "$root/bin"
cd "$out"

m68k-palmos-gcc -palmos4 -O2 -Wall -Wno-multichar \
  -fno-builtin -nostartfiles -nostdlib \
  -Wl,-e,KdKbd \
  "$root/src/kdiag/hook.c" -o hook.coff
data=$(m68k-palmos-size hook.coff | awk 'NR==2 {print $2 + $3}')
[ "$data" = "0" ] || { echo "error: hook data+bss = $data" >&2; exit 1; }
for s in KdBase:0 KdOrig:44 KdBuf:88; do
  a=$(m68k-palmos-nm hook.coff | awk -v n="${s%%:*}" '$3==n {print $1}')
  [ "$((16#$a))" -eq "${s##*:}" ] || { echo "error: ${s%%:*} at $a (must be ${s##*:})" >&2; exit 1; }
done
if m68k-palmos-objdump -d hook.coff | grep -q '%a5'; then
  echo "error: hook references %a5" >&2; exit 1
fi
if m68k-palmos-objdump -r hook.coff | grep -qE '^[0-9a-f]{8} '; then
  echo "error: hook has relocations" >&2; exit 1
fi
m68k-palmos-objcopy -O binary -j .text hook.coff KdHk0000.bin
m68k-palmos-objdump -d hook.coff > hook.dis

m68k-palmos-gcc -palmos4 -O2 -Wall -Wno-multichar "$root/src/kdiag/app.c" -o app 2>&1 | tee build.log
m68k-palmos-obj-res app
build-prc "$root/bin/KbdScan.prc" "KbdScan" MnKd *.app.grc KdHk0000.bin
ls -l "$root/bin/KbdScan.prc"
