#!/bin/bash
# Sony 가상 키보드 한글 모듈 → bin/MnSonyKbd.prc (타입 appl, 크리에이터 MnSy)
#
#   1) 앱(src/sony/app.c): 켜기·끄기, 리셋 뒤 자동 시작, Update 리셋, 판정 기록 페이지.
#   2) 키 글자 오버레이(src/sony/spatch.c) → 리소스 SyPt 0. BltDrawChars 패치. 오프셋 0 = 아래 함수 주소 칸, 4 = 진입점.
#   3) 입력·'한A'(src/sony/ime.c) → 리소스 SyIm 0. EvtGetEvent·WinDrawBitmap. 오프셋 0·4 = 진입점 오프셋, 8·12 = 원래 주소.
#   4) SyIc 0·1 '한A' 그림(tools/gen-caps-icon.py, Galmuri11 OFL), SyJm 0 자모 그림(tools/gen-sony-jamo.py, Neo둥근모 기반 OFL).
# 코드 리소스 검사(하나라도 어긋나면 실패): .data/.bss 0, 진입점 위치, %a5 참조 없음, 재배치 없음.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/build/sony"
rm -rf "$out"
mkdir -p "$out" "$root/bin"
cd "$out"

check_code() {   # $1 = coff, $2 = 코드 끝 기호(없으면 전체)
  local f=$1 stop=${2:-}
  local data
  data=$(m68k-palmos-size "$f" | awk 'NR==2 {print $2 + $3}')
  [ "$data" = "0" ] || { echo "error: $f data+bss = $data" >&2; exit 1; }
  if [ -n "$stop" ]; then
    local a
    a=$(m68k-palmos-nm "$f" | awk -v s="$stop" '$3==s {print $1}')
    [ -n "$a" ] || { echo "error: $stop not found in $f" >&2; exit 1; }
    m68k-palmos-objdump -d --stop-address=0x$a "$f" > "$f.code.dis"
  else
    m68k-palmos-objdump -d "$f" > "$f.code.dis"
  fi
  if grep -q '%a5' "$f.code.dis"; then
    echo "error: $f references %a5" >&2; exit 1
  fi
  if m68k-palmos-objdump -r "$f" | grep -qE '^[0-9a-f]{8} '; then
    echo "error: $f has relocations" >&2; exit 1
  fi
}

# 1) 오버레이 패치
m68k-palmos-gcc -palmos4 -O2 -Wall -Wno-multichar -fno-builtin -nostartfiles -nostdlib \
  -Wl,-e,PatchBlt "$root/src/sony/spatch.c" -o spatch.coff
a=$(m68k-palmos-nm spatch.coff | awk '$3=="PatchOrig" {print $1}')
[ "$((16#$a))" -eq 0 ] || { echo "error: PatchOrig at $a" >&2; exit 1; }
a=$(m68k-palmos-nm spatch.coff | awk '$3=="PatchBlt" {print $1}')
[ "$((16#$a))" -eq 4 ] || { echo "error: PatchBlt at $a" >&2; exit 1; }
check_code spatch.coff
m68k-palmos-objcopy -O binary -j .text spatch.coff SyPt0000.bin

# 2) 입력·'한A'
m68k-palmos-gcc -palmos4 -O2 -Wall -Wno-multichar -fno-builtin -nostartfiles -nostdlib \
  -Wl,-e,HgImGet "$root/src/sony/ime.c" -o ime.coff
a=$(m68k-palmos-nm ime.coff | awk '$3=="ImBase" {print $1}')
[ "$((16#$a))" -eq 0 ] || { echo "error: ImBase at $a" >&2; exit 1; }
a=$(m68k-palmos-nm ime.coff | awk '$3=="ImOrig" {print $1}')
[ "$((16#$a))" -eq 8 ] || { echo "error: ImOrig at $a" >&2; exit 1; }
check_code ime.coff KscBits   # 코드 뒤 KSC 표 데이터는 검사에서 뺀다(MnHangul v48)
m68k-palmos-objcopy -O binary -j .text ime.coff SyIm0000.bin

# 3) 앱
m68k-palmos-gcc -palmos4 -O2 -Wall -Wno-multichar "$root/src/sony/app.c" -o app 2>&1 | tee build.log
m68k-palmos-obj-res app
m68k-palmos-objdump -d app > app.dis
# 리셋 알림(앱 전역변수 없음)에서 부르는 함수는 %a5(전역) 참조가 없어야 한다
for fn in GetF PrefGet PrefPut AppDB Protect RomBlt PatchOn ImOn SonyOn; do
  grep -q "<$fn>:" app.dis || { echo "error: $fn not found" >&2; exit 1; }
  n=$(awk -v f="<$fn>:" '$2==f {on=1; next} /^[0-9a-f]+ </ {on=0} on && /%a5/' app.dis | wc -l)
  [ "$n" = "0" ] || { echo "error: $fn references globals (%a5) but runs at reset launch" >&2; exit 1; }
done

# 4) 데이터
cp "$root/src/sony/data/SyIc0000.bin" "$root/src/sony/data/SyIc0001.bin" "$root/src/sony/data/SyJm0000.bin" .

build-prc "$root/bin/MnSonyKbd.prc" "MnSonyKbd" MnSy *.app.grc SyPt0000.bin SyIm0000.bin SyIc0000.bin SyIc0001.bin SyJm0000.bin
ls -l "$root/bin/MnSonyKbd.prc"
