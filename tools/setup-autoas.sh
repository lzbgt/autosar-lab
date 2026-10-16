#!/usr/bin/env bash
# Install the actual AutoAS build/code-generation toolchain in an isolated venv.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
bash tools/fetch-upstreams.sh autoas
venv="$root/.venv-autoas"
if [[ ! -x "$venv/bin/python" ]]; then
    if command -v uv >/dev/null 2>&1; then
        uv venv "$venv" --python python3
    else
        python3 -m venv "$venv"
    fi
fi
if command -v uv >/dev/null 2>&1; then
    uv pip install --python "$venv/bin/python" -r "$root/toolchains/autoas-requirements.txt"
else
    "$venv/bin/python" -m pip install -r "$root/toolchains/autoas-requirements.txt"
fi
"$venv/bin/python" - <<'PY'
import SCons, jinja2, serial, bitarray, ply
from pycrc.algorithms import Crc
assert Crc(width=16, poly=0x1021, reflect_in=False, xor_in=0xFFFF,
           reflect_out=False, xor_out=0).table_driven(b"123456789") == 10673
print("AutoAS generator dependencies OK; SCons", SCons.__version__,
      "Jinja2", jinja2.__version__)
PY
echo "Toolchain: $venv/bin/scons"
echo "Stack source: $root/third_party/autoas"
