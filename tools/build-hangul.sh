#!/bin/bash
# CJKOS 대체 한글 모듈 시험 앱 → bin/MnHangul.prc (타입 appl, 크리에이터 MnHg)
#
# 여러 부분을 한 PRC에 넣는다.
#   1) 앱(src/hangul/app.c): 일반 Palm 앱. 패치 켜기·끄기, 기록 표시.
#   2) 패치 코드(src/hangul/patch.c) → 리소스 HgPt 0. 시스템 트랩에 끼워져 어느 앱에서나 불리므로
#      시작 코드·전역변수 없이 위치 독립 코드로 빌드한다. 오프셋 0 = 원래 함수 주소 칸, 오프셋 4 = 진입점.
#   2b) 호출 세기 코드(src/hangul/count.s) → 리소스 HgCt 0(폭·글자 경계 함수 호출 수 조사).
#   2c) 글자 경계 코드(src/hangul/intl.c) → 리소스 HgIt 0(Intl 루틴 7개 교체, 3a 단계).
#   2d) 폭 계산 코드(src/hangul/width.c) → 리소스 HgWd 0(폭 함수 트랩 5개 교체, 3b 단계).
#   2e) 키보드 대화상자 진단 코드(src/hangul/kbd.c) → 리소스 HgKd 0(SysKeyboardDialog 호출 기록, v28).
#   (v54: Sony 가상 키보드 한글은 tools/build-sony.sh → MnSonyKbd.prc)
#   3) 조합형 글꼴 묶음 HgJb 0(Neo둥근모 16×16)·1(물마루 기반 12×12)(src/hangul/data, OFL, tools/gen-johab.py).
# 패치 코드 검사(하나라도 어긋나면 실패): .data/.bss 0, PatchOrig 오프셋 0, PatchBlt 오프셋 4,
#   %a5 참조 없음, 재배치 없음.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
# 진단 빌드(기본, DIAG=1) → bin/. 배포 빌드(DIAG=0: 진단 기록을 뺌, src/common/diag.h) → build/release/
DIAG="${DIAG:-1}"
DFLAG="-DMN_DIAG=$DIAG"
outbin="$root/bin"
out="$root/build/hangul"
if [ "$DIAG" = "0" ]; then
  outbin="$root/build/release"
  out="$root/build/hangul-release"
fi
mkdir -p "$outbin"
rm -rf "$out"
mkdir -p "$out" "$root/bin"
cd "$out"

# 1) 패치 코드
m68k-palmos-gcc -palmos4 $DFLAG -O2 -Wall -Wno-multichar \
  -fno-builtin -nostartfiles -nostdlib \
  -Wl,-e,PatchBlt \
  "$root/src/hangul/patch.c" -o patch.coff

data=$(m68k-palmos-size patch.coff | awk 'NR==2 {print $2 + $3}')
[ "$data" = "0" ] || { echo "error: patch data+bss = $data" >&2; exit 1; }
orig=$(m68k-palmos-nm patch.coff | awk '$3=="PatchOrig" {print $1}')
entry=$(m68k-palmos-nm patch.coff | awk '$3=="PatchBlt" {print $1}')
[ "$((16#$orig))" -eq 0 ] || { echo "error: PatchOrig at $orig (must be 0)" >&2; exit 1; }
[ "$((16#$entry))" -eq 4 ] || { echo "error: PatchBlt at $entry (must be 4)" >&2; exit 1; }
# v69: 코드 부분(KscBits 표 앞)만, 파일로 검사 — 파이프(objdump | grep -q)는 pipefail에서 grep이 먼저 끝나면 거짓 통과·
#   그렇지 않으면 표 데이터를 명령으로 푼 '%a5'에 걸려 실패해, 결과가 그때그때 달랐다(build-kbdhan.sh와 같은 방식)
stop=$(m68k-palmos-nm patch.coff | awk '$3=="KscBits" {print $1}')
[ -n "$stop" ] || { echo "error: KscBits not found" >&2; exit 1; }
m68k-palmos-objdump -d --stop-address=0x$stop patch.coff > patch.code.dis
if grep -q '%a5' patch.code.dis; then
  echo "error: patch references %a5" >&2; exit 1
fi
if m68k-palmos-objdump -r patch.coff | grep -qE '^[0-9a-f]{8} '; then
  echo "error: patch has relocations" >&2; exit 1
fi
m68k-palmos-objcopy -O binary -j .text patch.coff HgPt0000.bin
m68k-palmos-objdump -d patch.coff > patch.dis

# 1b) 호출 세기 코드(src/hangul/count.s) → 리소스 HgCt 0. StubN = 100 + 18n (n = 0~23), 재배치 없음
m68k-palmos-as "$root/src/hangul/count.s" -o count.o
for n in 0 1 23; do
  a=$(m68k-palmos-nm count.o | awk -v s="Stub$n" '$3==s {print $1}')
  [ "$((16#$a))" -eq $((100 + 18 * n)) ] || { echo "error: Stub$n at $a" >&2; exit 1; }
done
if m68k-palmos-objdump -r count.o | grep -qE '^[0-9a-f]{8} '; then
  echo "error: count has relocations" >&2; exit 1
fi
m68k-palmos-objcopy -O binary -j .text count.o HgCt0000.bin
[ "$(stat -c %s HgCt0000.bin)" -eq $((100 + 18 * 24)) ] || { echo "error: HgCt size" >&2; exit 1; }

# 1c) 글자 경계 코드(src/hangul/intl.c) → 리소스 HgIt 0. 오프셋 0 = 진입점 오프셋 표, 28 = 원래 주소 칸
m68k-palmos-gcc -palmos4 $DFLAG -O2 -Wall -Wno-multichar \
  -fno-builtin -nostartfiles -nostdlib \
  -Wl,-e,HgCharAttr \
  "$root/src/hangul/intl.c" -o intl.coff
data=$(m68k-palmos-size intl.coff | awk 'NR==2 {print $2 + $3}')
[ "$data" = "0" ] || { echo "error: intl data+bss = $data" >&2; exit 1; }
a=$(m68k-palmos-nm intl.coff | awk '$3=="IntlBase" {print $1}')
[ "$((16#$a))" -eq 0 ] || { echo "error: IntlBase at $a" >&2; exit 1; }
a=$(m68k-palmos-nm intl.coff | awk '$3=="IntlOrig" {print $1}')
[ "$((16#$a))" -eq 28 ] || { echo "error: IntlOrig at $a" >&2; exit 1; }
if m68k-palmos-objdump -d intl.coff | grep -q '%a5'; then
  echo "error: intl references %a5" >&2; exit 1
fi
if m68k-palmos-objdump -r intl.coff | grep -qE '^[0-9a-f]{8} '; then
  echo "error: intl has relocations" >&2; exit 1
fi
m68k-palmos-objcopy -O binary -j .text intl.coff HgIt0000.bin
m68k-palmos-objdump -d intl.coff > intl.dis

# 1d) 폭 계산 코드(src/hangul/width.c) → 리소스 HgWd 0. 오프셋 0 = 진입점 오프셋 표, 20 = 원래 주소 칸
m68k-palmos-gcc -palmos4 $DFLAG -O2 -Wall -Wno-multichar \
  -fno-builtin -nostartfiles -nostdlib \
  -Wl,-e,HgCharsWidth \
  "$root/src/hangul/width.c" -o width.coff
data=$(m68k-palmos-size width.coff | awk 'NR==2 {print $2 + $3}')
[ "$data" = "0" ] || { echo "error: width data+bss = $data" >&2; exit 1; }
a=$(m68k-palmos-nm width.coff | awk '$3=="WidthBase" {print $1}')
[ "$((16#$a))" -eq 0 ] || { echo "error: WidthBase at $a" >&2; exit 1; }
a=$(m68k-palmos-nm width.coff | awk '$3=="WidthOrig" {print $1}')
[ "$((16#$a))" -eq 20 ] || { echo "error: WidthOrig at $a" >&2; exit 1; }
if m68k-palmos-objdump -d width.coff | grep -q '%a5'; then
  echo "error: width references %a5" >&2; exit 1
fi
if m68k-palmos-objdump -r width.coff | grep -qE '^[0-9a-f]{8} '; then
  echo "error: width has relocations" >&2; exit 1
fi
m68k-palmos-objcopy -O binary -j .text width.coff HgWd0000.bin
m68k-palmos-objdump -d width.coff > width.dis

# 1e) v28 키보드 대화상자 진단 코드(src/hangul/kbd.c) → 리소스 HgKd 0. 오프셋 0 = 진입점 오프셋 표, 32 = 원래 주소 칸
m68k-palmos-gcc -palmos4 $DFLAG -O2 -Wall -Wno-multichar \
  -fno-builtin -nostartfiles -nostdlib \
  -Wl,-e,HgKbdDialog \
  "$root/src/hangul/kbd.c" -o kbd.coff
data=$(m68k-palmos-size kbd.coff | awk 'NR==2 {print $2 + $3}')
[ "$data" = "0" ] || { echo "error: kbd data+bss = $data" >&2; exit 1; }
a=$(m68k-palmos-nm kbd.coff | awk '$3=="KbdBase" {print $1}')
[ "$((16#$a))" -eq 0 ] || { echo "error: KbdBase at $a" >&2; exit 1; }
a=$(m68k-palmos-nm kbd.coff | awk '$3=="KbdOrig" {print $1}')
[ "$((16#$a))" -eq 32 ] || { echo "error: KbdOrig at $a" >&2; exit 1; }
if m68k-palmos-objdump -d kbd.coff | grep -q '%a5'; then
  echo "error: kbd references %a5" >&2; exit 1
fi
if m68k-palmos-objdump -r kbd.coff | grep -qE '^[0-9a-f]{8} '; then
  echo "error: kbd has relocations" >&2; exit 1
fi
m68k-palmos-objcopy -O binary -j .text kbd.coff HgKd0000.bin
m68k-palmos-objdump -d kbd.coff > kbd.dis

# 2) 앱
m68k-palmos-gcc -palmos4 $DFLAG -O2 -Wall -Wno-multichar "$root/src/hangul/app.c" -o app 2>&1 | tee build.log
m68k-palmos-obj-res app
# 3) 조합형 글꼴 묶음(tools/gen-johab.py로 생성해 커밋된 파일): HgJb 0 = 16×16, HgJb 1 = 12×12
cp "$root/src/hangul/data/HgJ16.bin" HgJb0000.bin
cp "$root/src/hangul/data/HgJ12.bin" HgJb0001.bin
# v25: 리셋 알림(앱 전역변수 없음)에서 부르는 함수는 %a5(전역) 참조가 없어야 한다
m68k-palmos-objdump -d app > app.dis
# v67: + 지우기 알림(sysAppLaunchCmdNotify, 전역 없음)에서 부르는 함수. 마지막 rts 뒤(다음 함수의 문자열 상수)는 뺀다
for fn in ResetDiag AutoGet SkipGet SkipSet CjkOn HgSupported InstallAll InstPatch BltFind InstIntl InstWidth LockGlyphs Lock24 Installed AppDB GetF WidthTrap IntlSel \
          NotifyReg NotifyLog NotifyOff NotifyOffCheck WhoIn TopWho ProtectCount KbdTrap UninstKbd UninstWidth UninstIntl UninstPatch UnlockGlyphs Unlock24; do
  n=$(awk -v f="<$fn>:" '$2==f {on=1; next} /^[0-9a-f]+ </ {on=0} on {l[++k]=$0; if ($0 ~ /\trts/) last=k}
      END {if (last == 0) last = k; c=0; for (i=1; i<=last; i++) if (l[i] ~ /%a5/) c++; print c}' app.dis)
  grep -q "<$fn>:" app.dis || { echo "error: $fn not found" >&2; exit 1; }
  [ "$n" = "0" ] || { echo "error: $fn references globals (%a5) but runs at reset launch" >&2; exit 1; }
done

build-prc "$outbin/MnHangul.prc" "MnHangul" MnHg *.app.grc HgPt0000.bin HgCt0000.bin HgIt0000.bin HgWd0000.bin HgKd0000.bin HgJb0000.bin HgJb0001.bin
ls -l "$outbin/MnHangul.prc"
