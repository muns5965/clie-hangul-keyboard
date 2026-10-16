#!/bin/bash
# CJKOS 대체 한글 모듈 시험 앱 → bin/MnHangul.prc (타입 appl, 크리에이터 MnHg)
#
# 세 부분을 한 PRC에 넣는다.
#   1) 앱(src/hangul/app.c): 일반 Palm 앱. 패치 켜기·끄기, 기록 표시.
#   2) 패치 코드(src/hangul/patch.c) → 리소스 HgPt 0. 시스템 트랩에 끼워져 어느 앱에서나 불리므로
#      시작 코드·전역변수 없이 위치 독립 코드로 빌드한다. 오프셋 0 = 원래 함수 주소 칸, 오프셋 4 = 진입점.
#   3) 글자 데이터 HgFn 0·1(src/hangul/data, 시험용 Neo둥근모 16×16, OFL).
# 패치 코드 검사(하나라도 어긋나면 실패): .data/.bss 0, PatchOrig 오프셋 0, PatchBlt 오프셋 4,
#   %a5 참조 없음, 재배치 없음.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/build/hangul"
rm -rf "$out"
mkdir -p "$out" "$root/bin"
cd "$out"

# 1) 패치 코드
m68k-palmos-gcc -palmos4 -O2 -Wall -Wno-multichar \
  -fno-builtin -nostartfiles -nostdlib \
  -Wl,-e,PatchBlt \
  "$root/src/hangul/patch.c" -o patch.coff

data=$(m68k-palmos-size patch.coff | awk 'NR==2 {print $2 + $3}')
[ "$data" = "0" ] || { echo "error: patch data+bss = $data" >&2; exit 1; }
orig=$(m68k-palmos-nm patch.coff | awk '$3=="PatchOrig" {print $1}')
entry=$(m68k-palmos-nm patch.coff | awk '$3=="PatchBlt" {print $1}')
[ "$((16#$orig))" -eq 0 ] || { echo "error: PatchOrig at $orig (must be 0)" >&2; exit 1; }
[ "$((16#$entry))" -eq 4 ] || { echo "error: PatchBlt at $entry (must be 4)" >&2; exit 1; }
if m68k-palmos-objdump -d patch.coff | grep -q '%a5'; then
  echo "error: patch references %a5" >&2; exit 1
fi
if m68k-palmos-objdump -r patch.coff | grep -qE '^[0-9a-f]{8} '; then
  echo "error: patch has relocations" >&2; exit 1
fi
m68k-palmos-objcopy -O binary -j .text patch.coff HgPt0000.bin
m68k-palmos-objdump -d patch.coff > patch.dis

# 2) 앱
m68k-palmos-gcc -palmos4 -O2 -Wall -Wno-multichar "$root/src/hangul/app.c" -o app 2>&1 | tee build.log
m68k-palmos-obj-res app
# 3) 글자 데이터(시험용 Neo둥근모 16×16 완성형, tools/gen-hangul16.py로 생성해 커밋된 파일)
cp "$root/src/hangul/data/HgFn0000.bin" "$root/src/hangul/data/HgFn0001.bin" .
build-prc "$root/bin/MnHangul.prc" "MnHangul" MnHg *.app.grc HgPt0000.bin HgFn0000.bin HgFn0001.bin
ls -l "$root/bin/MnHangul.prc"
