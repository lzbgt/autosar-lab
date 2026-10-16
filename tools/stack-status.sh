#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
echo "Real AUTOSAR-related software on this host:"
for stack in autoas trampoline; do
    if [[ -d "third_party/$stack/.git" ]]; then
        printf '  %-12s ' "$stack"
        git -C "third_party/$stack" rev-parse --short=12 HEAD
    else
        echo "  $stack MISSING (bash tools/fetch-upstreams.sh $stack)"
    fi
done
echo "Build and configuration tools:"
for tool in .venv-autoas/bin/scons third_party/trampoline/goil/makefile-unix/goil \
            arm-none-eabi-gcc; do
    if [[ "$tool" == "arm-none-eabi-gcc" ]]; then
        if command -v "$tool" >/dev/null 2>&1; then
            echo "  $tool: installed"
        else
            echo "  $tool: missing"
        fi
    elif [[ -x "$tool" ]]; then
        echo "  $tool: ready"
    else
        echo "  $tool: not built/installed"
    fi
done
echo "Real Basic Software build artifacts:"
for f in third_party/autoas/build/posix/GCC/CanApp/CanApp \
         third_party/autoas/build/posix/GCC/prebuilt/libAsOne.so \
         third_party/autoas/build/posix/GCC/IsoTpSend/IsoTpSend \
         third_party/autoas/app/app/config/Com/GEN/CanIf_Cfg.c \
         third_party/autoas/app/app/config/Dcm/GEN/Dcm_Cfg.c; do
    if [[ -s "$f" ]]; then
        echo "  present $f"
    else
        echo "  missing $f"
    fi
done
echo "Run the genuine stack demo: python3 tools/test-autoas.py"
