#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p third_party

fetch_one() {
  local name="$1" url="$2" revision="$3"
  local target="third_party/$name"
  if [[ -d "$target/.git" ]]; then
    if [[ "$(git -C "$target" rev-parse HEAD)" == "$revision" ]]; then
      echo "$name already at $revision"
      return
    fi
    echo "$target exists at another revision; do not overwrite. Reconcile manually." >&2
    return 1
  fi
  if [[ -e "$target" ]]; then
    echo "$target is an unmanaged path. Leaving it untouched." >&2
    return 1
  fi
  mkdir "$target"
  git -C "$target" init -q
  git -C "$target" remote add origin "$url"
  GIT_TERMINAL_PROMPT=0 git -C "$target" fetch --depth=1 origin "$revision"
  git -C "$target" checkout -q --detach FETCH_HEAD
  echo "$name: $(git -C "$target" rev-parse HEAD)"
}
case "${1:-}" in
  trampoline)
    fetch_one trampoline https://github.com/TrampolineRTOS/trampoline.git ff287023252bf73273067f00d2dbc8bbc9a20401
    ;;
  autoas)
    fetch_one autoas https://github.com/autoas/as.git 7bf0210644a932ae45e6cabbe8388729808af7fd
    ;;
  all)
    fetch_one trampoline https://github.com/TrampolineRTOS/trampoline.git ff287023252bf73273067f00d2dbc8bbc9a20401
    fetch_one autoas https://github.com/autoas/as.git 7bf0210644a932ae45e6cabbe8388729808af7fd
    ;;
  *)
    echo "Usage: bash tools/fetch-upstreams.sh trampoline|autoas|all" >&2
    exit 2
    ;;
esac
