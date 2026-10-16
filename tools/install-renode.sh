#!/usr/bin/env bash
# Install official, pinned Renode into the project without requiring root.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
version="1.17.0"
url="https://github.com/renode/renode/releases/download/v1.17.0/renode-1.17.0.linux-portable.tar.gz"
expected="4ba7c68b59e2447f188ef4b4b112fcccd2802582c460beedceb63b84e5605a5f"
file="$root/.cache/renode/renode-$version.linux-portable.tar.gz"
target="$root/third_party/renode-$version"
mkdir -p "$root/.cache/renode" "$target"
if [[ ! -x "$target/renode" ]]; then
  if [[ ! -f "$file" ]]; then
    curl -fL --retry 2 --connect-timeout 15 --max-time 180 -o "$file.part" "$url"
    mv "$file.part" "$file"
  fi
  echo "$expected  $file" | sha256sum -c -
  tar -xf "$file" -C "$target" --strip-components=1
fi
"$target/renode" --version | tee "$root/.cache/renode/version.txt"
grep -Fq "Renode v1.17.0" "$root/.cache/renode/version.txt"
echo "PASS Renode v1.17.0: $target/renode"
