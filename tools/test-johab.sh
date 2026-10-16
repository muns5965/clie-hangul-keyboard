#!/bin/bash
# patch.c 조합형 글자 만들기를 PC에서 시험 (test/johab_test.py)
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
python3 "$root/test/johab_test.py" "$root/build/test"
