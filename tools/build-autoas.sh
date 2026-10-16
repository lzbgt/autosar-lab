#!/usr/bin/env bash
# Compile a real Classic Platform BSW ECU and its upstream tester (not our tutorial code).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
bash "$root/tools/setup-autoas.sh"
cd "$root/third_party/autoas"
actual="$(git rev-parse HEAD)"
expected="7bf0210644a932ae45e6cabbe8388729808af7fd"
if [[ "$actual" != "$expected" ]]; then
    echo "AutoAS revision mismatch: $actual, expected $expected" >&2
    exit 1
fi
scons="$root/.venv-autoas/bin/scons"
# Order matters: actual ECU modules; shared tester transports; tester executable.
"$scons" --app=CanApp -j4
"$scons" --lib=AsOne -j4
"$scons" --app=IsoTpSend -j4
for target in build/posix/GCC/CanApp/CanApp \
              build/posix/GCC/prebuilt/libAsOne.so \
              build/posix/GCC/IsoTpSend/IsoTpSend; do
    if [[ ! -s "$target" ]]; then
        echo "Missing required real AutoAS artifact: $target" >&2
        exit 1
    fi
done
echo "PASS: real AutoAS ECU, shared ISO-TP library, tester"
echo "Real generated CanIf: $PWD/app/app/config/Com/GEN/CanIf_Cfg.c"
echo "Real generated CanTp: $PWD/app/app/config/CanTp/GEN/CanTp_Cfg.c"
echo "Real generated Dcm: $PWD/app/app/config/Dcm/GEN/Dcm_Cfg.c"
