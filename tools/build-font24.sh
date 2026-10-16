#!/bin/bash
# 24×24 한글 추가 글꼴 → bin/MnHgFont24.prc (리소스 DB, 타입 HgFt, 크리에이터 MnHg, 리소스 HgJb 2)
#
# MnHangul.prc(본체, 16×16·12×12만 포함)와 따로 설치한다. 글꼴만 고칠 때는 이 파일만 다시 설치:
#   MnHangul All OFF → MnHgFont24.prc 설치 → Patch ON
# 입력: src/hangul/data/hangul24-pieces.json(편집기 내보내기). JSON을 인자로 주면 그 파일로 바꿔 넣는다.
# 데이터는 Neo둥근모(OFL 1.1) 파생이다. DB 이름에 "Neo둥근모"를 쓰지 않는다.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
if [ $# -ge 1 ]; then
  cp "$1" "$root/src/hangul/data/hangul24-pieces.json"
fi
python3 "$root/tools/gen-johab24.py" "$root/src/hangul/data/hangul24-pieces.json"

out="$root/build/font24"
rm -rf "$out"
mkdir -p "$out" "$root/bin"
cd "$out"
cp "$root/src/hangul/data/HgJ24.bin" HgJb0002.bin
# 리소스 DB 직접 작성(build-prc는 appl이 아니어도 code 리소스를 요구한다)
python3 - "$root/bin/MnHgFont24.prc" HgJb0002.bin <<'PY'
import struct, sys, time
out, src = sys.argv[1], sys.argv[2]
data = open(src, 'rb').read()
t = 0xE2A5B800                             # 고정 시각(로컬·CI 해시 일치용)
hdr = struct.pack('>32sHHIIIIIIIIIIH', b'MnHangul Font24', 0x0001, 1, t, t, 0, 0, 0, 0,
                  int.from_bytes(b'HgFt', 'big'), int.from_bytes(b'MnHg', 'big'), 0, 0, 1)
off = len(hdr) + 10 + 2
ent = b'HgJb' + struct.pack('>HI', 2, off)
open(out, 'wb').write(hdr + ent + b'\0\0' + data)
PY
ls -l "$root/bin/MnHgFont24.prc"
