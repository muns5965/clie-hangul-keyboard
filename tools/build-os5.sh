#!/bin/bash
# Palm OS 5(Sony TG50·NX70V) 한글 표시·입력 → bin/MnHangul5.prc (타입 appl, 크리에이터 MnH5)
#
#   1) ARM 코드(src/os5/*.c, ctx.S) → 리소스 armc 1001. 표 8의 그리기·폭·Text Manager·EvtGetEvent 칸을 바꾸는 함수들.
#      위치 독립 한 덩어리로 링크(link.ld, ctx 맨 앞). armc 1000 = 표 8 주소 얻기(table8.S).
#   2) 칸 표(src/os5/slots.txt) → 리소스 H5Tb 1000: n, ctx 오프셋, 칸마다 (칸, 함수 오프셋, O_ 번호, 0, 허용 값 4개) 빅엔디언.
#   3) 글꼴 묶음 HgJp 1000 = src/hangul/data/HgJ16.bin(Neo둥근모 16×16 기반 OFL),
#      HgJp 1001 = HgJ24.bin(24×24, 위 16×16 조각을 확대·손질한 파생 OFL — 큰 글꼴용),
#      KS X 1001 표 Ksct 1000 = tools/gen-ksc-table.py --bin.
#   4) 앱(src/os5/app.c, 68k).
# 검사(하나라도 어긋나면 실패):
#   ARM: .text 밖 구역 없음, switch 상수 표(CSWTCH) 없음, 재배치 없음, ctx가 0, slots.txt 순서 = os5.h enum 순서, 모든 함수 있음.
#   앱: 리셋 실행·리셋 완료 알림에서 부르는 함수에 %a5(전역) 참조 없음.
# 필요: arm-none-eabi-gcc(Ubuntu gcc-arm-none-eabi), m68k-palmos-gcc(prc-tools-remix), build-prc.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
# 진단 빌드(기본, DIAG=1) → bin/. 배포 빌드(DIAG=0: 키 기록 등 진단을 뺌) → build/release/
DIAG="${DIAG:-1}"
outbin="$root/bin"
out="$root/build/os5"
if [ "$DIAG" = "0" ]; then
  outbin="$root/build/release"
  out="$root/build/os5-release"
fi
src="$root/src/os5"
mkdir -p "$outbin"
rm -rf "$out"
mkdir -p "$out"
cd "$out"

# 1) ARM
AF="-march=armv5te -marm -O2 -ffreestanding -nostdlib -fno-pic -fno-jump-tables -fno-tree-switch-conversion
    -fno-builtin -ffixed-r9 -Wall -Werror -DMN_DIAG=$DIAG"
arm-none-eabi-gcc $AF -c "$src/ctx.S" -o ctx.o
for f in draw text ime; do
  arm-none-eabi-gcc $AF -c "$src/$f.c" -o $f.o
done
arm-none-eabi-gcc $AF -T "$src/link.ld" -e 0 -Wl,--emit-relocs ctx.o draw.o text.o ime.o -lgcc -o hook.elf
arm-none-eabi-objcopy -O binary hook.elf armc03e9.bin
arm-none-eabi-gcc $AF -c "$src/table8.S" -o table8.o
arm-none-eabi-ld -Ttext=0 -e 0 table8.o -o table8.elf
arm-none-eabi-objcopy -O binary table8.elf armc03e8.bin

secs=$(arm-none-eabi-objdump -h hook.elf | awk '$1 ~ /^[0-9]+$/ {print $2}' | grep -v '^\.ARM\.attributes$' | tr '\n' ' ')
[ "$secs" = ".text " ] || { echo "error: hook.elf sections: $secs" >&2; exit 1; }
if arm-none-eabi-nm hook.elf | grep -q 'CSWTCH'; then
  echo "error: switch lookup table (absolute addresses) in hook.elf" >&2; exit 1
fi
# 코드가 스스로 가리키는 절대 주소가 있으면 복사한 자리에서 틀린다(--emit-relocs로 남긴 재배치 중 R_ARM_ABS32)
if arm-none-eabi-readelf -r hook.elf | grep -q 'R_ARM_ABS32'; then
  echo "error: absolute relocations in hook.elf" >&2; arm-none-eabi-readelf -r hook.elf | grep R_ARM_ABS32 >&2; exit 1
fi
ctx=$(arm-none-eabi-nm hook.elf | awk '$3=="CtxPtr" {print $1}')
[ "$((16#$ctx))" -eq 0 ] || { echo "error: CtxPtr at $ctx" >&2; exit 1; }

# 2) 칸 표
python3 - "$src/slots.txt" "$src/os5.h" hook.elf H5Tb03e8.bin <<'PY'
import re, struct, subprocess, sys
slots, hdr, elf, outp = sys.argv[1:]
enum = re.search(r'enum \{([^}]*)\}', open(hdr).read()).group(1)
names = [x.strip() for x in enum.split(',') if x.strip() and x.strip() != 'O_COUNT']
sym = {}
for l in subprocess.check_output(['arm-none-eabi-nm', elf]).decode().splitlines():
    f = l.split()
    if len(f) == 3:
        sym[f[2]] = int(f[0], 16)
rows = [l.split() for l in open(slots) if l.startswith('0x')]
if [r[1] for r in rows] != names:
    sys.exit('error: slots.txt order != os5.h enum')
w = [len(rows), sym['ctx']]
for i, r in enumerate(rows):
    if r[2] not in sym:
        sys.exit('error: %s not in hook.elf' % r[2])
    exp = [int(x, 16) for x in (r[3] + '/' + r[4]).split('/')]   # TG50 값들 + NX70V 값들
    if len(exp) > 4:
        sys.exit('error: at most four values per slot')
    exp += [exp[0]] * (4 - len(exp))
    w += [int(r[0], 16), sym[r[2]], i, 0]
    for v in exp:
        w += [v >> 16, v & 0xffff]
open(outp, 'wb').write(struct.pack('>%dH' % len(w), *w))
print('slots', len(rows), 'ctx', hex(sym['ctx']), 'code', hex(max(sym.values())))
PY

# 3) 자료
cp "$root/src/hangul/data/HgJ16.bin" HgJp03e8.bin
cp "$root/src/hangul/data/HgJ24.bin" HgJp03e9.bin
python3 "$root/tools/gen-ksc-table.py" --bin Ksct03e8.bin > /dev/null

# 4) 앱
m68k-palmos-gcc -palmos4 -DMN_DIAG=$DIAG -O2 -Wall -Wno-multichar "$src/app.c" -o app 2>&1 | tee build.log
m68k-palmos-objdump -d app > app.dis
for fn in On Check CopyRes Table8Addr PrefGet SelfDB Detached; do
  grep -q "<$fn>:" app.dis || { echo "error: $fn not found" >&2; exit 1; }
  # 함수의 마지막 rts 뒤(다음 함수의 문자열 상수가 이어 붙는 자리)는 코드가 아니므로 뺀다(build-kbdhan.sh와 같음)
  n=$(awk -v f="<$fn>:" '$2==f {on=1; next} /^[0-9a-f]+ </ {on=0} on {l[++k]=$0; if ($0 ~ /\trts/) last=k}
      END {if (last == 0) last = k; c=0; for (i=1; i<=last; i++) if (l[i] ~ /%a5/) c++; print c}' app.dis)
  [ "$n" = "0" ] || { echo "error: $fn references globals (%a5) but runs at reset launch" >&2; exit 1; }
done

build-prc "$outbin/MnHangul5.prc" "MnHangul5" MnH5 app armc03e8.bin armc03e9.bin H5Tb03e8.bin HgJp03e8.bin HgJp03e9.bin Ksct03e8.bin
echo "ok: $outbin/MnHangul5.prc"
