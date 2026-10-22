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
# 코드 부분(KscBits 표 앞)만 %a5 검사. 파이프(objdump | grep -q)는 pipefail에서 grep이 먼저 끝나면 objdump가 SIGPIPE로 실패해
# 검사가 거짓으로 통과할 수 있어(v9 조합 표를 넣고 확인) 파일로 쓴 뒤 검사한다
stop=$(m68k-palmos-nm hook.coff | awk '$3=="KscBits" {print $1}')
[ -n "$stop" ] || { echo "error: KscBits not found" >&2; exit 1; }
m68k-palmos-objdump -d --stop-address=0x$stop hook.coff > hook.code.dis
if grep -q '%a5' hook.code.dis; then
  echo "error: hook references %a5" >&2; exit 1
fi
if m68k-palmos-objdump -r hook.coff | grep -qE '^[0-9a-f]{8} '; then
  echo "error: hook has relocations" >&2; exit 1
fi
m68k-palmos-objcopy -O binary -j .text hook.coff KhHk0000.bin
m68k-palmos-objdump -d hook.coff > hook.dis

m68k-palmos-gcc -palmos4 -O2 -Wall -Wno-multichar "$root/src/kbdhan/app.c" -o app 2>&1 | tee build.log
m68k-palmos-obj-res app
m68k-palmos-objdump -d app > app.dis
# v13: 리셋 실행(전역변수 없음)·다른 앱의 All OFF에서 부르는 함수는 %a5(전역) 참조가 없어야 한다(build-sony.sh와 같은 검사)
for fn in GetF PrefGet PrefPut Supported AppDB HookTrap HookOn HookOff BuildJamo FreeJamo; do
  grep -q "<$fn>:" app.dis || { echo "error: $fn not found" >&2; exit 1; }
  n=$(awk -v f="<$fn>:" '$2==f {on=1; next} /^[0-9a-f]+ </ {on=0} on {l[++k]=$0; if ($0 ~ /\trts/) last=k}
      END {if (last == 0) last = k; c=0; for (i=1; i<=last; i++) if (l[i] ~ /%a5/) c++; print c}' app.dis)
  [ "$n" = "0" ] || { echo "error: $fn references globals (%a5) but runs at reset launch" >&2; exit 1; }
done
cp "$root/src/kbdhan/data/KhIc0000.bin" "$root/src/kbdhan/data/KhIc0001.bin" .   # v12: 한/영 표시 그림(tools/gen-kbd-mark.py)
cp "$root/src/sony/data/SyJm0000.bin" KhJm0000.bin   # v16: 키 글자 자모 그림(MnSonyKbd와 같은 데이터, tools/gen-sony-jamo.py)
build-prc "$root/bin/MnKbdHan.prc" "MnKbdHan" MnKh *.app.grc KhHk0000.bin KhIc0000.bin KhIc0001.bin KhJm0000.bin
ls -l "$root/bin/MnKbdHan.prc"
