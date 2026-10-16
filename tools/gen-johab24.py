#!/usr/bin/env python3
"""24×24 조합형 한글 묶음 생성 → src/hangul/data/HgJ24.bin

입력: 편집기(tools/font-editor) 내보내기 JSON(format mnhangul-hangul-pieces, size 24, 조각 344개)과
      16×16 묶음 HgJ16.bin(같은 Neo둥근모 8×4×4 벌 규칙·자모표를 쓴다).
결과는 Neo둥근모(OFL 1.1) 파생 데이터다. 이름에 "Neo둥근모"를 쓰지 않는다.

형식은 gen-johab.py의 묶음과 같고, 다른 점만:
  2 판 = 2 (그림 한 줄 = 4바이트, 왼쪽이 비트 31. 24칸이면 비트 31~8)
  3 칸 높이 24, 4 글자 폭 24, 5 기준선 = JSON baseline(18)
  조각 순서·규칙·벌 수는 HgJ16.bin과 같다(조각 번호 = 초 벌×19+L, 중 Nc×19+벌×21+V, 종 Nc×19+Nj×21+벌×27+T−1).
  자모표(KS X 1001 0xA4A1~0xA4FE)는 HgJ16의 16×16 자모를 편집기의 자동 확대(JSON method)로 24×24로 늘린 것.
검사: 11,172자 전부, 조각 이름으로 직접 겹친 그림과 기기 방식(규칙→조각 번호) 조합 결과가 같아야 한다.

사용: python3 tools/gen-johab24.py <neodgm24-pieces.json> [미리보기.png]
"""
import json
import os
import struct
import sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
S = 24

# 편집기(editor.html)와 같은 자동 확대
METHODS = {'stroke': ((2, 3), (2, 3)), 'mid': ((2, 3), (1, 2)), 'thin': ((1, 2), (1, 2))}


def f15(r):
    return int(r * 1.5 + 0.5)          # JS Math.round와 같게(양수)


def scale_line(src, t1, t2):
    out = [0] * S
    n, i = len(src), 0
    while i < n:
        if not src[i]:
            i += 1
            continue
        j = i
        while j < n and src[j]:
            j += 1
        L, s = j - i, f15(i)
        e = s + t1 if L == 1 else s + t2 if L == 2 else max(f15(j - 1) + t1, f15(j - 2) + t2)
        for k in range(s, min(e, S)):
            out[k] = 1
        i = j
    return out


def upscale(rows16, method):
    a = [[(r >> (15 - x)) & 1 for x in range(16)] for r in rows16]
    if method == 'nearest':
        g = [[a[y * 2 // 3][x * 2 // 3] for x in range(S)] for y in range(S)]
    else:
        (x1, x2), (y1, y2) = METHODS.get(method, METHODS['stroke'])
        mid = [scale_line(a[y], x1, x2) for y in range(16)]
        g = [[0] * S for _ in range(S)]
        for x in range(S):
            col = scale_line([mid[y][x] for y in range(16)], y1, y2)
            for y in range(S):
                g[y][x] = col[y]
    return [sum(b << (31 - x) for x, b in enumerate(row)) for row in g]


def main():
    js = json.load(open(sys.argv[1], encoding='utf-8'))
    assert js['format'] == 'mnhangul-hangul-pieces' and js['size'] == S, 'not a 24×24 piece export'
    p16 = open(os.path.join(root, 'src/hangul/data/HgJ16.bin'), 'rb').read()
    assert p16[:2] == b'HJ' and p16[3] == 16
    Nc, Nj, Nt = p16[6], p16[7], p16[8]
    o_p, n_p, o_e, n_e, o_s, n_s, o_j, _ = struct.unpack_from('>8H', p16, 112)
    assert n_e == 0 and n_s == 0 and n_p == Nc * 19 + Nj * 21 + Nt * 27

    def pid(name):
        kind, st, jm = name.split('_')
        st, jm = int(st), int(jm)
        if kind == 'cho':
            return st * 19 + jm
        if kind == 'jung':
            return Nc * 19 + st * 21 + jm
        return Nc * 19 + Nj * 21 + st * 27 + (jm - 1)

    pieces = [None] * n_p
    by_name = {}
    for name, hx in zip(js['names'], js['pieces']):
        assert len(hx) == S * 6
        rows = [int(hx[i * 6:i * 6 + 6], 16) << 8 for i in range(S)]
        by_name[name] = rows
        pieces[pid(name)] = rows
    assert all(p is not None for p in pieces), 'missing pieces'

    jamo = []
    for k in range(94):
        r16 = struct.unpack_from('>16H', p16, o_j + k * 32)
        jamo.append(upscale(r16, js.get('method', 'stroke')) if any(r16) else [0] * S)

    out = bytearray(b'HJ' + bytes([2, S, S, js['baseline'], Nc, Nj, Nt, 0]))
    out += p16[10:112]
    hdr = len(out)
    out += bytes(16)
    rb = lambda rows: b''.join(struct.pack('>I', r) for r in rows)
    o_p2 = len(out)
    for r in pieces:
        out += rb(r)
    o_x = len(out)
    o_j2 = len(out)
    for r in jamo:
        out += rb(r)
    struct.pack_into('>8H', out, hdr, o_p2, n_p, o_x, 0, o_x, 0, o_j2, 0)
    assert len(out) < 65000

    # 검사: 기기 방식(규칙) 조합 == 조각 이름으로 직접 겹친 그림
    CHO = list(range(19))
    bad = 0
    names = {pid(n): n for n in by_name}
    for s in range(11172):
        L, V, T = s // 588, (s % 588) // 28, s % 28
        ids = [out[10 + V * 2 + (T > 0)] * 19 + L, Nc * 19 + out[52 + L * 2 + (T > 0)] * 21 + V]
        if T:
            ids.append(Nc * 19 + Nj * 21 + out[90 + V] * 27 + T - 1)
        a = [0] * S
        for i in ids:
            pr = struct.unpack_from('>%dI' % S, out, o_p2 + i * S * 4)
            a = [x | y for x, y in zip(a, pr)]
        b = [0] * S
        for i in ids:
            b = [x | y for x, y in zip(b, by_name[names[i]])]
        bad += a != b
    print('HgJ24.bin: %d bytes, pieces %d, edited %d, method %s, jamo %d, mismatches %d'
          % (len(out), n_p, len(js.get('edited', [])), js.get('method'), sum(any(j) for j in jamo), bad))
    if bad:
        sys.exit(1)
    open(os.path.join(root, 'src/hangul/data/HgJ24.bin'), 'wb').write(out)
    if len(sys.argv) > 2:
        preview(bytes(out), sys.argv[2])


def compose(pk, s):
    S_ = pk[3]
    Nc, Nj = pk[6], pk[7]
    o_p = struct.unpack_from('>H', pk, 112)[0]
    L, V, T = s // 588, (s % 588) // 28, s % 28
    ids = [pk[10 + V * 2 + (T > 0)] * 19 + L, Nc * 19 + pk[52 + L * 2 + (T > 0)] * 21 + V]
    if T:
        ids.append(Nc * 19 + Nj * 21 + pk[90 + V] * 27 + T - 1)
    a = [0] * S_
    for i in ids:
        a = [x | y for x, y in zip(a, struct.unpack_from('>%dI' % S_, pk, o_p + i * S_ * 4))]
    return a


def preview(pk, path):
    from PIL import Image
    lines = ['다람쥐 헌 쳇바퀴에 타고파', '키스의 고유조건은 입술끼리', '똠방각하 펲시 뷁 쓁 읢 갉',
             '가나다라마바사아자차카타파하']
    o_j = struct.unpack_from('>H', pk, 124)[0]
    jl = 'ㄱㄴㄷㄹㅁㅂㅅㅇㅈㅊㅋㅌㅍㅎㅏㅑㅓㅕㅗㅛㅜㅠㅡㅣ'
    rows_txt = lines + [jl]
    cw, sc = 26, 3
    W = max(len(t) for t in rows_txt) * cw * sc + 20
    img = Image.new('L', (W, len(rows_txt) * 28 * sc + 20), 255)
    px = img.load()
    for li, t in enumerate(rows_txt):
        for ci, ch in enumerate(t):
            c = ord(ch)
            if 0xAC00 <= c <= 0xD7A3:
                g = compose(pk, c - 0xAC00)
            elif ch != ' ':
                k = ch.encode('euc_kr')[1] - 0xA1
                g = struct.unpack_from('>24I', pk, o_j + k * 96)
            else:
                continue
            for y in range(24):
                for x in range(24):
                    if (g[y] >> (31 - x)) & 1:
                        for dy in range(sc):
                            for dx in range(sc):
                                px[10 + (ci * cw + 1 + x) * sc + dx, 10 + (li * 28 + 2 + y) * sc + dy] = 0
    img.save(path)


main()
