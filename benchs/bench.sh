#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
xtree_bin=${XTREE_BIN:-$root_dir/xtree}
tree_bin=${TREE_BIN:-$(command -v tree 2>/dev/null || true)}
work=${BENCH_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/xtree-bench.XXXXXX")}
keep=${KEEP_BENCH_DIR:-0}
trap '[ "$keep" = 1 ] || rm -rf "$work"' EXIT

[ -x "$xtree_bin" ] || { printf 'Build xtree first or set XTREE_BIN.\n' >&2; exit 2; }
[ -n "$tree_bin" ] && [ -x "$tree_bin" ] || {
  printf 'SKIP: tree is not installed (set TREE_BIN to its path).\n'
  exit 0
}

"$root_dir/benchs/generate_tree.sh" "$work/tree" \
  "${BENCH_DEPTH:-5}" "${BENCH_BRANCHES:-5}" "${BENCH_FILES:-10}"

printf 'benchmark root: %s\n' "$work/tree"
printf 'xtree: %s\ntree:  %s\n' "$xtree_bin" "$tree_bin"
printf '\n%-10s %12s %12s\n' command avg_real_s avg_rss_kb

run() {
  name=$1
  shift
  if command -v /usr/bin/time >/dev/null 2>&1; then
    /usr/bin/time -f "$name %e %M" "$@" >/dev/null 2>>"$work/time.log"
  else
    time "$@" >/dev/null 2>>"$work/time.log"
  fi
}

: > "$work/time.log"
i=0
repeats=${BENCH_REPEATS:-3}
while [ "$i" -lt "$repeats" ]; do
  run xtree "$xtree_bin" --no-color "$work/tree"
  run tree "$tree_bin" "$work/tree"
  i=$((i + 1))
done

if [ -s "$work/time.log" ]; then
  awk '{time[$1] += $2; rss[$1] += $3; count[$1]++} END {
    for (name in time)
      printf "%-10s %12.4f %12.0f (%d runs)\n", name, time[name] / count[name], rss[name] / count[name], count[name]
  }' "$work/time.log" | sort
fi
