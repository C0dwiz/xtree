#!/bin/sh
set -eu

root=${1:?usage: generate_tree.sh DIR [DEPTH] [BRANCHES] [FILES]}
depth=${2:-5}
branches=${3:-6}
files=${4:-12}

mkdir -p "$root"
create() {
  dir=$1
  level=$2
  i=0
  while [ "$i" -lt "$files" ]; do
    printf 'file=%s level=%s index=%s\n' "$dir" "$level" "$i" > "$dir/file_${i}.txt"
    i=$((i + 1))
  done
  [ "$level" -ge "$depth" ] && return
  i=0
  while [ "$i" -lt "$branches" ]; do
    child="$dir/dir_${level}_${i}"
    mkdir -p "$child"
    create "$child" $((level + 1))
    i=$((i + 1))
  done
}
create "$root" 0
