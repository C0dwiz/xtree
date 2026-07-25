#!/bin/sh
set -eu

bin=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/xtree-test.XXXXXX")
trap 'rm -rf "$work"' EXIT

fail() { printf 'FAIL: %s\n' "$1" >&2; exit 1; }
contains() { printf '%s\n' "$1" | grep -F -- "$2" >/dev/null || fail "missing: $2"; }
not_contains() { if printf '%s\n' "$1" | grep -F -- "$2" >/dev/null; then fail "unexpected: $2"; fi; }

mkdir -p "$work/src" "$work/cache/deep" "$work/nested"
printf '%s\n' '*.log' 'cache/' 'src/*.tmp' > "$work/.gitignore"
printf '%s\n' '*.secret' '!keep.secret' > "$work/nested/.gitignore"
printf 'one\n' > "$work/src/main.cpp"
printf 'tmp\n' > "$work/src/a.tmp"
printf 'cache\n' > "$work/cache/deep/blob.bin"
printf 'log\n' > "$work/debug.log"
printf 'nested\n' > "$work/nested/file.txt"
printf 'secret\n' > "$work/nested/a.secret"
printf 'keep\n' > "$work/nested/keep.secret"

out=$($bin --help)
contains "$out" '--gitignore'

out=$($bin --all --gitignore --no-color "$work")
contains "$out" 'main.cpp'
contains "$out" '.gitignore'
not_contains "$out" 'a.tmp'
not_contains "$out" 'blob.bin'
not_contains "$out" 'debug.log'
not_contains "$out" 'a.secret'
contains "$out" 'keep.secret'

out=$($bin --all --gitignore --stats --no-color "$work")
contains "$out" 'Files: 5, Lines: 8'

out=$($bin --all --gitignore --depth 0 --no-color "$work")
contains "$out" 'src'
not_contains "$out" 'main.cpp'

if $bin --gitignore=bad "$work" >/dev/null 2>&1; then
  fail 'invalid --gitignore value accepted'
fi

out=$($bin --all --no-color "$work")
contains "$out" 'a.tmp'
contains "$out" 'blob.bin'
contains "$out" 'debug.log'

printf 'PASS: CLI tests\n'
