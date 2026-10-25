#!/usr/bin/env python3
"""Compile and exercise the ACTUAL OS4 and OS5 HgWordBounds functions on the host.

Only Palm's original TxtWordBounds / TxtCharAttr are substituted. The real
device's original-word selection and pen-event behavior still require ROM tests.
"""
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
typedef uint8_t Boolean;
typedef uint8_t UInt8;
typedef uint16_t UInt16;
typedef uint32_t UInt32;
typedef char Char;
typedef uint16_t WChar;
typedef uint16_t (*WCharFn)(WChar);
typedef uint16_t (*AttrFn)(WChar);
typedef Boolean (*WordFn)(const char *, uint32_t, uint32_t, uint32_t *, uint32_t *);
#define true 1
#define false 0
static int Hi(uint8_t b) { return b >= 0xA1 && b <= 0xFE; }
static uint32_t RunBefore(const uint8_t *t, uint32_t o) {
    uint32_t k = 0;
    while (o > 0 && Hi(t[o - 1])) { o--; k++; }
    return k;
}
static uint16_t mock_attr(uint16_t c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9')) ? 0x0232 : 0;
}
/* ROM 실측(에뮬레이터 진단 앱으로 원래 TxtWordBounds 직접 호출, TG50·NX70V·NR70V 같음):
 * 아포스트로피는 낱말 안 글자(don't @o -> 0..5), 밑줄은 낱말 밖(a_b가 @a -> 0..1). */
static int original_word_char(uint8_t c) {
    return mock_attr(c) != 0 || c == '\'';
}
static Boolean mock_word(const char *src, uint32_t n, uint32_t o,
                         uint32_t *s, uint32_t *e) {
    const uint8_t *t = (const uint8_t *)src;
    uint32_t a = o, b = o;
    if (o >= n || !original_word_char(t[o])) return false;
    while (a > 0 && original_word_char(t[a - 1])) a--;
    while (b < n && original_word_char(t[b])) b++;
    *s = a; *e = b;
    return true;
}
#define O_CharAttr 0
#define O_WordBounds 7
#define ORIG(i) ((i) == 0 ? (uintptr_t)mock_attr : (uintptr_t)mock_word)
#define Orig(i) ORIG(i)
#define Idle() 0
"""

def byte_array(s):
    return "{" + ",".join(str(b) for b in s.encode("euc_kr")) + ",0}"

# (문자열, 누를 글자(처음 나오는 것), 기대 선택). 에뮬레이터 실측과 같은 정책
CASES = [
    ("가나다", "나", "가나다"),
    ("한글 입력", "글", "한글"),
    ("ABC 가나다", "B", "ABC"),
    ("ABC 가나다", "나", "가나다"),
    ("don't 가나다", "o", "don't"),
    ("don't가", "o", "don't가"),      # MnHangul5 v13·MnHangul v74: 원래 낱말 범위부터 넓힘
    ("a_b 가나다", "a", "a"),          # 밑줄은 낱말 밖(ROM)
    ("a_b가", "a", "a"),
    ("a_b가", "b", "b가"),
    ("가ABC나", "B", "가ABC나"),
    ("가123나", "2", "가123나"),
    ("가,나", "나", "나"),
    ("「가나다」", "나", "「가나다」"),   # 2바이트 기호도 낱말 글자(현재 정책)
    ("ab cd", "c", "cd"),              # 한글 없음: 원래 함수 그대로
]
KNOWN_LIMITS = []

def build_cases():
    lines = ["int main(void) {", "int failures = 0;"]
    for i, (source, point, expected) in enumerate(CASES + KNOWN_LIMITS):
        at = source.index(point)
        o = len(source[:at].encode("euc_kr"))
        desired_start = source.find(expected)
        desired = (len(source[:desired_start].encode("euc_kr")),
                   len(source[:desired_start + len(expected)].encode("euc_kr")))
        label = "KNOWN" if i >= len(CASES) else "PASS"
        lines += [
            "{",
            "const char src[] = " + byte_array(source) + ";",
            "uint32_t s=9999,e=9999;",
            "int ok = HgWordBounds(src, sizeof(src)-1, " + str(o) + ", &s, &e);",
            'printf("' + label + ' ' + str(i) + ': %d [%u,%u] expected [' +
                str(desired[0]) + ',' + str(desired[1]) + ']\\n", ok,s,e);',
        ]
        if label == "PASS":
            lines.append("if (!ok || s!=" + str(desired[0]) + " || e!=" +
                         str(desired[1]) + ") failures++;")
        lines.append("}")
    lines += ["return failures ? 1 : 0;", "}"]
    return "\n".join(lines)

def run_one(path, target):
    source = (ROOT / path).read_text(encoding="utf-8")
    word = extract_function(source, "HgWordBounds")
    helper = extract_function(source, "Word1")
    if target == "os5":
        header = "#define kAlNum 0x0232\n"
    else:
        header = "#define charAttrAlNum 0x0232\n"
    c_source = PRELUDE + "\n" + header + helper + "\n" + word + "\n" + build_cases()
    with tempfile.TemporaryDirectory() as d:
        path_c = pathlib.Path(d) / "word.c"
        executable = pathlib.Path(d) / "word"
        path_c.write_text(c_source, encoding="utf-8")
        subprocess.run(["gcc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                        str(path_c), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
    print(target + ": core cases passed; KNOWN lines are non-blocking limitations")

if __name__ == "__main__":
    run_one("src/os5/text.c", "os5")
    run_one("src/hangul/intl.c", "os4")
