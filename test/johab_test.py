#!/usr/bin/env python3
"""patch.c의 조합형 글자 만들기(RankToSyl·Compose·GlyphRows)를 PC에서 시험한다.

patch.c에서 세 함수를 그대로 잘라 PC용 C 프로그램으로 컴파일하고(판정표는 asm 대신 PC 배열),
커밋된 글꼴 묶음(src/hangul/data/HgJ16.bin, HgJ12.bin)으로 KS X 1001 완성형 2,350자와 자모 94자를 그려,
이 파일의 compose()(tools/gen-johab.py와 같은 방법) 결과와 1픽셀 단위로 비교한다.
묶음 자체가 원본 글꼴과 같은지는 gen-johab.py가 생성 때 11,172자 전부 확인한다.
사용: python3 test/johab_test.py <작업 디렉터리>
"""
import os
import re
import struct
import subprocess
import sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
work = sys.argv[1]
src = open(os.path.join(root, 'src/hangul/patch.c'), encoding='utf-8').read()


def fn(name):
    m = re.search(r'\n(static [^\n]*\b' + name + r'\([^;{]*\)\n\{.*?\n\})', src, re.S)
    assert m, name
    return m.group(1)


code = '\n'.join(fn(n) for n in ['RdW', 'RankToSyl', 'Compose', 'GlyphRows'])
code = code.replace('asm("lea KscBits(%%pc),%0" : "=a" (bits));', 'bits = HostKscBits;')
code = code.replace('asm("lea KscCum(%%pc),%0" : "=a" (cum));', 'cum = HostKscCum;')
code = code.replace('static inline', 'static')
bits = [len(chr(0xAC00 + i).encode('euc_kr', 'replace')) == 2 for i in range(11172)]
tbl = bytearray((11172 + 7) // 8)
for i, ok in enumerate(bits):
    if ok:
        tbl[i >> 3] |= 0x80 >> (i & 7)
cum, n = [], 0
for blk in range(44):
    cum.append(n)
    n += sum(bits[blk * 256:(blk + 1) * 256])
prog = '''#include <stdio.h>
typedef unsigned char UInt8; typedef unsigned short UInt16; typedef short Int16; typedef unsigned char Boolean;
#define true 1
#define false 0
static const UInt8 HostKscBits[] = {%s};
static const UInt16 HostKscCum[] = {%s};
%s
int main(int argc, char **argv) {
  static UInt8 pk[70000]; UInt16 rows[16]; int H, a, b, i;
  FILE *f = fopen(argv[1], "rb"); fread(pk, 1, sizeof pk, f); fclose(f); H = pk[3];
  for (a = 0xB0; a <= 0xC8; a++) for (b = 0xA1; b <= 0xFE; b++) {
    GlyphRows(pk, a, b, rows); printf("%%02X%%02X", a, b); for (i = 0; i < H; i++) printf(" %%04x", rows[i]); printf("\\n"); }
  for (b = 0xA1; b <= 0xFE; b++) {
    GlyphRows(pk, 0xA4, b, rows); printf("A4%%02X", b); for (i = 0; i < H; i++) printf(" %%04x", rows[i]); printf("\\n"); }
  return 0; }
''' % (','.join(map(str, tbl)), ','.join(map(str, cum)), code)
os.makedirs(work, exist_ok=True)
open(os.path.join(work, 'johab.c'), 'w').write(prog)
exe = os.path.join(work, 'johab')
subprocess.check_call(['gcc', '-O1', '-w', '-o', exe, os.path.join(work, 'johab.c')])


def compose(pk, s):
    H, Nc, Nj = pk[3], pk[6], pk[7]
    o_p, n_p, o_e, n_e, o_s, n_s, o_j, _ = struct.unpack_from('>8H', pk, 112)
    rb = H * 2
    for i in range(n_s):
        at = o_s + i * (2 + rb)
        if struct.unpack_from('>H', pk, at)[0] == s:
            return list(struct.unpack_from('>%dH' % H, pk, at + 2))
    L, V, T = s // 588, (s % 588) // 28, s % 28
    ids = None
    for i in range(n_e):
        e = struct.unpack_from('>4H', pk, o_e + i * 8)
        if e[0] == s:
            ids = e[1:]
    if ids is None:
        ids = (pk[10 + V * 2 + (T > 0)] * 19 + L, Nc * 19 + pk[52 + L * 2 + (T > 0)] * 21 + V,
               (Nc * 19 + Nj * 21 + pk[90 + V] * 27 + T - 1) if T else 0xFFFF)
    rows = [0] * H
    for pid in ids:
        if pid != 0xFFFF:
            rows = [a | b for a, b in zip(rows, struct.unpack_from('>%dH' % H, pk, o_p + pid * rb))]
    return rows


fails = 0
total = 0
for name in ('HgJ16.bin', 'HgJ12.bin'):
    pk = open(os.path.join(root, 'src/hangul/data', name), 'rb').read()
    H = pk[3]
    o_j = struct.unpack_from('>H', pk, 124)[0]
    out = subprocess.check_output([exe, os.path.join(root, 'src/hangul/data', name)]).decode().split('\n')
    bad = 0
    for line in out:
        if not line:
            continue
        p = line.split()
        a, b = int(p[0][:2], 16), int(p[0][2:], 16)
        rows = [int(x, 16) for x in p[1:]]
        if a == 0xA4:
            want = list(struct.unpack_from('>%dH' % H, pk, o_j + (b - 0xA1) * H * 2))
        else:
            want = compose(pk, ord(bytes([a, b]).decode('euc_kr')) - 0xAC00)
        total += 1
        if rows != want:
            bad += 1
            if bad <= 5:
                print('FAIL %s %s' % (name, p[0]))
    fails += bad
print('johab: %d glyphs, %d fail' % (total, fails))
sys.exit(1 if fails else 0)
