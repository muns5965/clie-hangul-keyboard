#!/usr/bin/env python3
"""Compile and exercise the ACTUAL OS5 HgCompare / HgCaselessCompare on the host.

Only Palm's original TxtCompare / TxtCaselessCompare are substituted by a small
model of the ROM behaviour measured on TG50 (CmpDiag): primary pass ignores
case, Compare breaks ties by case (lowercase first). Device order and Address
Look Up still require emulator tests.
"""
import functools
import itertools
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def extract_function(source, name):
    import re
    match = re.search(r"\b" + name + r"\s*\([^;{}]*\)\s*\{", source)
    if not match:
        raise AssertionError("missing function: " + name)
    start = source.rfind("\n", 0, match.start()) + 1
    depth = 0
    for i in range(match.end() - 1, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start:i + 1]
    raise AssertionError("unclosed function: " + name)


PRELUDE = r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t s16;
static int Hi(uint8_t b) { return b >= 0xA1 && b <= 0xFE; }
typedef s16 (*CmpFn)(const char *, u32, u16 *, const char *, u32, u16 *);
static int W(u8 c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
/* ROM 모형: 바이트마다 Latin-1 가중치(여기서는 대소문자만 무시), 일치 길이는 바이트 */
static s16 mock_caseless(const char *s1, u32 n1, u16 *m1, const char *s2, u32 n2, u16 *m2) {
    const u8 *a = (const u8 *)s1, *b = (const u8 *)s2;
    u32 i = 0;
    /* 실측: 길이는 최대값, NUL에서 끝 */
    { u32 k = 0; while (k < n1 && a[k]) k++; n1 = k; k = 0; while (k < n2 && b[k]) k++; n2 = k; }
    while (i < n1 && i < n2 && W(a[i]) == W(b[i])) i++;
    if (m1) *m1 = (u16)i;
    if (m2) *m2 = (u16)i;
    if (i < n1 && i < n2) return (s16)(W(a[i]) - W(b[i]));
    return n1 == n2 ? 0 : (n1 < n2 ? -1 : 1);
}
static s16 mock_compare(const char *s1, u32 n1, u16 *m1, const char *s2, u32 n2, u16 *m2) {
    const u8 *a = (const u8 *)s1, *b = (const u8 *)s2;
    u32 i;
    s16 r = mock_caseless(s1, n1, m1, s2, n2, m2);
    if (r) return r;
    { u32 k = 0; while (k < n1 && a[k]) k++; n1 = k; }
    for (i = 0; i < n1; i++)   /* 같은 길이 — 대소문자 차이: 소문자가 앞, 일치 길이 0 (TG50 실측) */
        if (a[i] != b[i]) {
            if (m1) *m1 = 0;
            if (m2) *m2 = 0;
            return (a[i] >= 'a' && a[i] <= 'z') ? -1 : 1;
        }
    return 0;
}
#define O_Compare 0
#define O_Caseless 1
#define ORIG(i) ((i) == O_Compare ? (uintptr_t)mock_compare : (uintptr_t)mock_caseless)
#define Idle() 0
"""

WORDS = ["", "A", "a", "ab", "Ab", "aB", "ABC", "abc", "abd", "ab가", "abc가", "AB가", "Ab가", "aB가",
         "가", "가A", "가a", "가b", "가나", "가나다", "가나마", "각", "간", "값", "걋", "겯", "나", "다",
         "한", "ABC가", "ABC나", "가ABC", "가abc", "가 ", " 가", "「가」", "　", "一"]


def enc(s):
    return s.encode("euc_kr")


def c_array(b):
    return "{" + "".join(str(x) + "," for x in b) + "0}"


def build_main():
    lines = ["int main(void) {"]
    for k, w in enumerate(WORDS):
        lines.append("static const char w%d[] = %s;" % (k, c_array(enc(w))))
    lines.append("const char *ws[] = {" + ",".join("w%d" % k for k in range(len(WORDS))) + "};")
    lines.append("const u32 ns[] = {" + ",".join(str(len(enc(w))) for w in WORDS) + "};")
    lines += [
        "u32 i, j;",
        "for (i = 0; i < %d; i++) for (j = 0; j < %d; j++) {" % (len(WORDS), len(WORDS)),
        "  u16 a = 999, b = 999, c = 999, d = 999;",
        "  s16 r = HgCompare(ws[i], ns[i], &a, ws[j], ns[j], &b);",
        "  s16 q = HgCaselessCompare(ws[i], ns[i], &c, ws[j], ns[j], &d);",
        '  printf("%u %u %d %u %u %d %u %u\\n", i, j, r, a, b, q, c, d);',
        "  { u16 e = 999, f = 999, g = 999, h = 999;   /* Address Look Up: 길이 0xFFFF, NUL에서 끝 */",
        "    s16 r2 = HgCompare(ws[i], 0xFFFF, &e, ws[j], 0xFFFF, &f);",
        "    s16 q2 = HgCaselessCompare(ws[i], 0xFFFF, &g, ws[j], 0xFFFF, &h);",
        "    if ((r2 > 0) != (r > 0) || (r2 < 0) != (r < 0) || e != a || f != b ||",
        "        (q2 > 0) != (q > 0) || (q2 < 0) != (q < 0) || g != c || h != d)",
        '      printf("NUL %u %u\\n", i, j); }',
        "}",
        "return 0;", "}",
    ]
    return "\n".join(lines)


def sign(v):
    return (v > 0) - (v < 0)


def is_boundary(b, m):
    """m이 글자 경계인지(앞에서부터 한글 쌍을 두 바이트로 셈)."""
    i = 0
    while i < m:
        i += 2 if i + 1 < len(b) and 0xA1 <= b[i] <= 0xFE and 0xA1 <= b[i + 1] <= 0xFE else 1
    return i == m


def check(results):
    fail = []
    idx = {w: k for k, w in enumerate(WORDS)}

    def get(x, y):
        return results[(idx[x], idx[y])]

    n = len(WORDS)
    for which, name in ((0, "Compare"), (1, "Caseless")):
        cmp = {key: sign(v[which * 3]) for key, v in results.items()}
        for i in range(n):
            if cmp[(i, i)] != 0:
                fail.append("%s reflexive %r" % (name, WORDS[i]))
            for j in range(n):
                if cmp[(i, j)] != -cmp[(j, i)]:
                    fail.append("%s antisymmetric %r %r" % (name, WORDS[i], WORDS[j]))
                r, m1, m2 = results[(i, j)][which * 3: which * 3 + 3]
                if not (is_boundary(enc(WORDS[i]), m1) and is_boundary(enc(WORDS[j]), m2)):
                    fail.append("%s matchlen mid-char %r %r (%d,%d)" % (name, WORDS[i], WORDS[j], m1, m2))
        for i, j, k in itertools.product(range(n), repeat=3):
            if cmp[(i, j)] <= 0 and cmp[(j, k)] <= 0 and cmp[(i, k)] > 0:
                fail.append("%s transitive %r %r %r" % (name, WORDS[i], WORDS[j], WORDS[k]))

    # (A, B, Compare 부호, Caseless 부호, Caseless 일치 길이) — A-1 실측과 v14 정책
    expect = [
        ("가", "나", -1, -1, (0, 0)),
        ("나", "다", -1, -1, (0, 0)),
        ("다", "한", -1, -1, (0, 0)),
        ("각", "간", -1, -1, (0, 0)),          # 원래 (1,1)
        ("걋", "겯", -1, -1, (0, 0)),          # 원래 Caseless 0 (2,2)
        ("가나다", "가나마", -1, -1, (4, 4)),
        ("ABC가", "ABC나", -1, -1, (3, 3)),
        ("가", "가나", -1, -1, (2, 2)),
        ("ABC", "AB가", -1, -1, (2, 2)),      # 1바이트 글자가 앞(원래는 AB가가 앞)
        ("abc가", "ab가", -1, -1, (2, 2)),
        ("가A", "가a", 1, 0, (3, 3)),          # 대소문자 차이는 Compare만(소문자 앞)
        ("aB가", "Ab가", -1, 0, (4, 4)),
        ("가 ", "가A", -1, -1, (2, 2)),
        ("가abc", "가ABC", -1, 0, (5, 5)),
    ]
    for a, b, rc, rq, mq in expect:
        r, _, _, q, c, d = get(a, b)
        if sign(r) != rc or sign(q) != rq or (c, d) != mq:
            fail.append("expect %r:%r -> Compare %d Caseless %d (%d,%d)" % (a, b, r, q, c, d))

    names = ["다", "가", "값", "나", "한", "가나"]
    order = names[:]
    order.sort(key=functools.cmp_to_key(lambda x, y: get(x, y)[3]))
    if order != ["가", "가나", "값", "나", "다", "한"]:
        fail.append("address order %r" % order)
    return fail


def main():
    source = (ROOT / "src/os5/text.c").read_text(encoding="utf-8")
    parts = [extract_function(source, n) for n in ("CLen", "HasPair", "HgCmp", "HgCompare", "HgCaselessCompare")]
    c_source = PRELUDE + "\n" + "\n".join(parts) + "\n" + build_main()
    with tempfile.TemporaryDirectory() as d:
        c = pathlib.Path(d) / "cmp.c"
        exe = pathlib.Path(d) / "cmp"
        c.write_text(c_source, encoding="utf-8")
        subprocess.run(["gcc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                        str(c), "-o", str(exe)], check=True)
        out = subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout
    results = {}
    for line in out.split("\n"):
        if line.startswith("NUL"):
            raise SystemExit("FAIL length 0xFFFF differs: " + line)
        if line:
            v = [int(x) for x in line.split()]
            results[(v[0], v[1])] = tuple(v[2:])
    fail = check(results)
    for f in fail:
        print("FAIL", f)
    if fail:
        raise SystemExit(1)
    print("os5: compare %d words, %d pairs, antisymmetry/transitivity/boundaries/expectations passed"
          % (len(WORDS), len(results)))


if __name__ == "__main__":
    main()
