#!/usr/bin/env python3
"""KSC5601(KS X 1001) 완성형 한글 2,350자 판정표 생성 → src/skwr/ksc_table.h

유니코드 음절 번호 i(0~11171)마다 1비트(MSB 먼저, 1397바이트) + 256비트 묶음별
앞선 개수 누계(44개, 16비트 빅엔디언). KSC5601 한글은 유니코드 순서와 같은 순서로
0xB0A1부터 배치되므로 순위 r → ((0xB0 + r/94) << 8) | (0xA1 + r%94).
플러그인은 전역 데이터를 못 쓰므로 표를 .text에 어셈블리로 넣는다.
"""
import os

bits = []
codes = []
for i in range(11172):
    b = chr(0xAC00 + i).encode('euc_kr', 'replace')
    ok = len(b) == 2
    bits.append(ok)
    if ok:
        codes.append((b[0] << 8) | b[1])

assert len(codes) == 2350, len(codes)
for r, c in enumerate(codes):  # 순서 검증
    assert c == ((0xB0 + r // 94) << 8) | (0xA1 + r % 94), (r, hex(c))

tbl = bytearray((11172 + 7) // 8)
for i, ok in enumerate(bits):
    if ok:
        tbl[i >> 3] |= 0x80 >> (i & 7)
if len(tbl) % 2:  # .even 채움 바이트가 어셈블러마다 다를 수 있어 명시적으로 0을 붙인다
    tbl.append(0)
cum = []
n = 0
for blk in range((11172 + 255) // 256):
    cum.append(n)
    n += sum(bits[blk * 256:(blk + 1) * 256])

out = ['/* tools/gen-ksc-table.py 가 생성. 직접 고치지 말 것 */',
       'asm(".text\\n.even\\nKscBits:\\n"']
for k in range(0, len(tbl), 16):
    out.append('    " .byte ' + ','.join('0x%02x' % x for x in tbl[k:k + 16]) + '\\n"')
out.append('    ".even\\nKscCum:\\n"')
for k in range(0, len(cum), 8):
    out.append('    " .word ' + ','.join('%d' % x for x in cum[k:k + 8]) + '\\n"')
out.append('    ".even\\n");')
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
with open(os.path.join(root, 'src/skwr/ksc_table.h'), 'w') as f:
    f.write('\n'.join(out) + '\n')
print('ok', len(tbl), 'bytes bits,', len(cum), 'cum')
