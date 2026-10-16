#!/usr/bin/env python3
"""조합형 한글 글꼴 묶음(font pack) 생성 → src/hangul/data/HgJ16.bin(Neo둥근모 16×16), HgJ12.bin(물마루 12×12)

두 글꼴 모두 원래 초·중·종성 조각을 겹쳐 만든 글꼴이다(Neo둥근모 8×4×4, 물마루 10×6×4).
기기는 조각과 '벌 고르기 규칙'만 갖고 글자를 조합한다. 규칙에 안 맞는 음절은 예외표(조각 번호 직접 지정),
손으로 그린 음절은 특수 글자표(통째 그림)로 담는다. 생성 후 11,172자 전부를 원본 글꼴을 직접 그린 결과와
1픽셀 단위로 비교해 하나라도 다르면 실패한다.

묶음 형식(빅엔디언, 리소스 하나):
  0  'HJ'  2 판(1)  3 칸 높이 H  4 글자 폭 W  5 기준선(위에서 줄 수)
  6  초성 벌 수 Nc  7 중성 벌 수 Nj  8 종성 벌 수 Nt  9 0
  10 초성 규칙[21×2] (중성 V, 받침 유무) → 벌      52 중성 규칙[19×2] (초성 L, 받침 유무) → 벌
  90 종성 규칙[21] (중성 V) → 벌                    111 0
  112 조각 위치, 조각 수   116 예외표 위치, 개수   120 특수 글자 위치, 개수   124 자모표 위치, 0
  조각 번호: 초성 = 벌×19 + L, 중성 = Nc×19 + 벌×21 + V, 종성 = Nc×19 + Nj×21 + 벌×27 + (T−1), 그 뒤에 추가 조각
  조각/글자 그림: H줄 × 2바이트(왼쪽이 MSB)
  예외표: (음절 번호, 초, 중, 종 조각 번호; 없음 = 0xFFFF) 8바이트씩, 음절 번호 순
  특수 글자: (음절 번호) + 그림, 음절 번호 순.  자모표: KS X 1001 0xA4A1~0xA4FE 94자 그림(없으면 빈칸)

사용: python3 tools/gen-johab.py <neodgm.ttf> <Mulmaru.ttf> <Mulmaru.pfp>
  Neo둥근모: npm @kfonts/neodgm 0.5.0 src/neodgm.ttf (OFL 1.1)
  물마루: github.com/mushsooni/mulmaru 릴리스 v1.0 Mulmaru.zip (OFL 1.1, 예약 글꼴 이름 "물마루"·"Mulmaru" —
         형식을 바꾼 이 데이터는 그 이름을 쓰지 않는다)
"""
import collections
import json
import os
import struct
import sys

from fontTools.pens.recordingPen import DecomposingRecordingPen, RecordingPen
from fontTools.ttLib import TTFont

CHO = "ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ"
JUNG = "ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ"
JONG = "ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ"   # T-1
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def raster(tt, name, H, upp, base):
    """TTF 글자 하나를 H줄 × 16칸 비트맵(줄마다 정수, MSB 왼쪽)으로. upp = 1픽셀 단위, base = 기준선 위 줄 수"""
    gs = tt.getGlyphSet()
    pen = DecomposingRecordingPen(gs)
    gs[name].draw(pen)
    polys, cur = [], []
    for op, a in pen.value:
        if op == 'moveTo':
            cur = [a[0]]
        elif op == 'lineTo':
            cur.append(a[0])
        elif op in ('closePath', 'endPath'):
            polys.append(cur)
            cur = []
        else:
            raise ValueError(op)
    rows = []
    for y in range(H):
        fy = (base - y - 0.5) * upp
        v = 0
        for x in range(16):
            fx = (x + 0.5) * upp
            w = 0
            for p in polys:
                n = len(p)
                for i in range(n):
                    (x1, y1), (x2, y2) = p[i], p[(i + 1) % n]
                    if (y1 > fy) != (y2 > fy) and fx < x1 + (fy - y1) * (x2 - x1) / (y2 - y1):
                        w += 1 if y2 > y1 else -1
            if w:
                v |= 0x8000 >> x
        rows.append(v)
    return rows


def ksc_jamo():
    out = []
    for t in range(0xA1, 0xFF):
        try:
            out.append(bytes([0xA4, t]).decode('euc_kr'))
        except UnicodeDecodeError:
            out.append(None)
    return out


def derive(parts, H):
    """parts[s] = {'cho': (벌, 자모, 그림), ...} 또는 ('special', 그림). 규칙·예외·조각 표 만들기"""
    cnt = {k: collections.defaultdict(collections.Counter) for k in ('cho', 'jung', 'jong')}
    nsets = {'cho': 0, 'jung': 0, 'jong': 0}
    std = {}          # (kind, set, jamo) -> rows
    extra = []        # 표준 자리가 없는 조각 그림
    extra_id = {}
    for s, p in enumerate(parts):
        if p[0] == 'special':
            continue
        L, V, T = s // 588, (s % 588) // 28, s % 28
        for kind, (st, jm, rows) in p[1].items():
            if st is None:
                continue
            nsets[kind] = max(nsets[kind], st + 1)
            key = (V, T > 0) if kind == 'cho' else (L, T > 0) if kind == 'jung' else V
            cnt[kind][key][st] += 1
            prev = std.get((kind, st, jm))
            assert prev is None or prev == rows, ('piece differs', kind, st, jm)
            std[(kind, st, jm)] = rows
    rule = {k: {key: c.most_common(1)[0][0] for key, c in cnt[k].items()} for k in cnt}
    Nc, Nj, Nt = nsets['cho'], nsets['jung'], nsets['jong']

    def pid(kind, st, jm):
        if kind == 'cho':
            return st * 19 + jm
        if kind == 'jung':
            return Nc * 19 + st * 21 + jm
        return Nc * 19 + Nj * 21 + st * 27 + (jm - 1)

    base_n = Nc * 19 + Nj * 21 + Nt * 27
    pieces = [[0] * H for _ in range(base_n)]
    for (kind, st, jm), rows in std.items():
        pieces[pid(kind, st, jm)] = rows
    exc, spec = [], []
    for s, p in enumerate(parts):
        if p[0] == 'special':
            spec.append((s, p[1]))
            continue
        L, V, T = s // 588, (s % 588) // 28, s % 28
        want = []
        for kind in ('cho', 'jung', 'jong'):
            if kind not in p[1]:
                want.append(0xFFFF)
                continue
            st, jm, rows = p[1][kind]
            if st is None:
                key = tuple(rows)
                if key not in extra_id:
                    extra_id[key] = base_n + len(extra)
                    extra.append(rows)
                want.append(extra_id[key])
            else:
                want.append(pid(kind, st, jm))
        ruled = [pid('cho', rule['cho'][(V, T > 0)], L), pid('jung', rule['jung'][(L, T > 0)], V),
                 pid('jong', rule['jong'][V], T) if T else 0xFFFF]
        if want != ruled:
            exc.append((s, want))
    return dict(Nc=Nc, Nj=Nj, Nt=Nt, rule=rule, pieces=pieces + extra, exc=exc, spec=spec)


def pack(d, H, W, base, jamo):
    out = bytearray(b'HJ' + bytes([1, H, W, base, d['Nc'], d['Nj'], d['Nt'], 0]))
    for V in range(21):
        for h in (False, True):
            out.append(d['rule']['cho'].get((V, h), 0))
    for L in range(19):
        for h in (False, True):
            out.append(d['rule']['jung'].get((L, h), 0))
    for V in range(21):
        out.append(d['rule']['jong'].get(V, 0))
    out.append(0)
    assert len(out) == 112
    hdr_at = len(out)
    out += bytes(16)

    def rows_bytes(rows):
        return b''.join(struct.pack('>H', r) for r in rows)

    o_p = len(out)
    for r in d['pieces']:
        out += rows_bytes(r)
    o_e = len(out)
    for s, ids in d['exc']:
        out += struct.pack('>4H', s, *ids)
    o_s = len(out)
    for s, rows in d['spec']:
        out += struct.pack('>H', s) + rows_bytes(rows)
    o_j = len(out)
    for rows in jamo:
        out += rows_bytes(rows)
    struct.pack_into('>8H', out, hdr_at, o_p, len(d['pieces']), o_e, len(d['exc']), o_s, len(d['spec']), o_j, 0)
    assert len(out) < 65000
    return bytes(out)


def compose(pk, s):
    """기기 코드(patch.c)와 같은 방법으로 묶음에서 음절 s를 조합(검증용)"""
    H = pk[3]
    Nc, Nj = pk[6], pk[7]
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
        ids = (pk[10 + V * 2 + (T > 0)] * 19 + L,
               Nc * 19 + pk[52 + L * 2 + (T > 0)] * 21 + V,
               (Nc * 19 + Nj * 21 + pk[90 + V] * 27 + T - 1) if T else 0xFFFF)
    rows = [0] * H
    for pid in ids:
        if pid == 0xFFFF:
            continue
        pr = struct.unpack_from('>%dH' % H, pk, o_p + pid * rb)
        rows = [a | b for a, b in zip(rows, pr)]
    return rows


def neo(path):
    tt = TTFont(path)
    cmap, gs = tt.getBestCmap(), tt.getGlyphSet()
    assert tt['head'].unitsPerEm == 1024 and tt['hhea'].ascent == 768
    piece_rows = {}
    parts, ref = [], []
    for s in range(11172):
        pen = RecordingPen()
        gs[cmap[0xAC00 + s]].draw(pen)
        p = {}
        for op, a in pen.value:
            assert op == 'addComponent' and tuple(a[1]) == (1, 0, 0, 1, 0, 0)
            kind, st, jm = a[0].split('_')
            if a[0] not in piece_rows:
                piece_rows[a[0]] = raster(tt, a[0], 16, 64, 12)
            p[kind] = (int(st), int(jm), piece_rows[a[0]])
        parts.append(('pieces', p))
    for s in range(11172):
        ref.append(raster(tt, cmap[0xAC00 + s], 16, 64, 12))
    jamo = [raster(tt, cmap[ord(c)], 16, 64, 12) if c and ord(c) in cmap else [0] * 16 for c in ksc_jamo()]
    return parts, ref, jamo


def mulmaru(ttf, pfp):
    tt = TTFont(ttf)
    cmap = tt.getBestCmap()
    assert tt['head'].unitsPerEm == 192 and tt['hhea'].ascent == 160
    d = json.load(open(pfp))
    by = {g['unicode']: g for g in d['glyphs'] if 'unicode' in g}

    def rows_of(data):
        """pfp 그림(줄 수가 12보다 적으면 아래쪽에 맞춤) → 12줄 정수"""
        off = 12 - len(data)
        rows = [0] * 12
        for y, line in enumerate(data):
            for x, ch in enumerate(line[:16]):
                if ch == '#' and 0 <= y + off < 12:
                    rows[y + off] |= 0x8000 >> x
        return rows

    kinds = {'choseong': ('cho', CHO), 'jungseong': ('jung', JUNG), 'jongseong': ('jong', JONG)}
    parts, ref = [], []
    for s in range(11172):
        g = by[0xAC00 + s]
        if 'components' not in g:
            parts.append(('special', rows_of(g['data'])))
        else:
            p = {}
            L, V, T = s // 588, (s % 588) // 28, s % 28
            unnamed = []
            for c in g['components']:
                pc = by[c]
                rows = rows_of(pc['data'])
                if 'name' in pc:
                    _, k, st, jm = [x.strip() for x in pc['name'].split('|')]
                    kind, table = kinds[k]
                    idx = table.index(jm) + (1 if kind == 'jong' else 0)
                    p[kind] = (int(st) - 1, idx, rows)
                else:
                    unnamed.append(rows)
            # 이름 없는 추가 조각: 이름 있는 조각을 다 넣은 뒤 빈 자리(초→중→종)에 넣는다(조각 순서는 일정하지 않음)
            for rows in unnamed:
                for kind in ('cho', 'jung', 'jong'):
                    if kind not in p and not (kind == 'jong' and T == 0):
                        p[kind] = (None, None, rows)
                        break
            parts.append(('pieces', p))
        ref.append(raster(tt, cmap[0xAC00 + s], 12, 16, 10))
    jamo = [raster(tt, cmap[ord(c)], 12, 16, 10) if c and ord(c) in cmap else [0] * 12 for c in ksc_jamo()]
    return parts, ref, jamo


def build(name, parts, ref, jamo, H, W, base):
    d = derive(parts, H)
    pk = pack(d, H, W, base, jamo)
    bad = [s for s in range(11172) if compose(pk, s) != ref[s]]
    print('%s: %d bytes, pieces %d, exceptions %d, specials %d, mismatches %d'
          % (name, len(pk), len(d['pieces']), len(d['exc']), len(d['spec']), len(bad)))
    if bad:
        print('  first:', ''.join(chr(0xAC00 + s) for s in bad[:30]))
        sys.exit(1)
    open(os.path.join(root, 'src/hangul/data', name), 'wb').write(pk)


build('HgJ16.bin', *neo(sys.argv[1]), H=16, W=16, base=12)
build('HgJ12.bin', *mulmaru(sys.argv[2], sys.argv[3]), H=12, W=11, base=10)
