#!/usr/bin/env python3
"""Build real AutoAS SWC descriptions and RTE interface headers.

The upstream CanIC application has a separate unresolved VicServer include
issue. This tool invokes the same upstream Rte generator on two smaller
components, with version-verified dependencies, without patching AutoAS.
"""
from __future__ import annotations

import importlib.util
import os
from pathlib import Path
import subprocess
import sys
from xml.etree import ElementTree

ROOT = Path(__file__).resolve().parent.parent
AUTOAS = ROOT / "third_party" / "autoas"
DOWNLOAD = ROOT / "third_party" / "download"
REQUIRED_AUTOAS = "7bf0210644a932ae45e6cabbe8388729808af7fd"
COMPONENTS = ("Gauge", "Telltale")
REPOS = (
    ("arxml", "https://github.com/autoas/arxml.git",
     "694b75ffe4c1cd7c4c65f23af036f37dd2eabfdd"),
    ("cfile", "https://github.com/autoas/cfile.git",
     "7ed5d2584e4c711d2fb219dfdf228f100f609186"),
)


def git(*args: str, cwd: Path) -> str:
    call = subprocess.run(["git", *args], cwd=cwd, check=True,
                          text=True, capture_output=True)
    return call.stdout.strip()


def ensure_commit(name: str, url: str, commit: str) -> Path:
    target = DOWNLOAD / name
    if target.exists() and not (target / ".git").is_dir():
        raise RuntimeError(f"Refusing to overwrite non-Git dependency: {target}")
    if not target.exists():
        target.mkdir(parents=True)
        git("init", "-q", cwd=target)
        git("remote", "add", "origin", url, cwd=target)
    if git("remote", "get-url", "origin", cwd=target) != url:
        raise RuntimeError(f"{name} origin differs; refusing to change existing repository")
    current = git("rev-parse", "--verify", "HEAD", cwd=target) if (
        (target / ".git" / "HEAD").exists() and
        subprocess.run(["git", "rev-parse", "--verify", "HEAD"],
                       cwd=target, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, check=False).returncode == 0
    ) else None
    if current != commit:
        # Do not clobber local changes. Upstream's '.autoas.version.done'
        # is its own marker and is left untouched; generated files are not
        # treated as portable source or silently overwritten here.
        changes = git("status", "--porcelain", "--untracked-files=no", cwd=target)
        if changes:
            raise RuntimeError(f"Modified dependency at {target}; reconcile before pinning")
        git("fetch", "--depth=1", "origin", commit, cwd=target)
        git("checkout", "-q", "--detach", commit, cwd=target)
    if git("rev-parse", "HEAD", cwd=target) != commit:
        raise RuntimeError(f"Wrong revision for {name}")
    print(f"PINNED {name}: {commit[:12]}", flush=True)
    return target


def main() -> int:
    venv = ROOT / ".venv-autoas" / "bin" / "python"
    if not venv.exists():
        raise RuntimeError("Missing generator environment: bash tools/setup-autoas.sh")
    if Path(sys.executable).resolve() != venv.resolve():
        # Execute with the real isolated interpreter; caller need not activate.
        os.execv(str(venv), [str(venv), str(Path(__file__).resolve()), *sys.argv[1:]])
    if not AUTOAS.is_dir():
        raise RuntimeError("AutoAS not checked out: bash tools/setup-autoas.sh")
    if git("rev-parse", "HEAD", cwd=AUTOAS) != REQUIRED_AUTOAS:
        raise RuntimeError("AutoAS source revision differs from pinned lab")
    com_cfg = AUTOAS / "app/app/config/Com/GEN/Com_Cfg.c"
    if not com_cfg.exists():
        raise RuntimeError("First generate communication configuration: bash tools/build-autoas.sh")
    arxml = ensure_commit(*REPOS[0])
    cfile = ensure_commit(*REPOS[1])
    # Same module resolution order as upstream Rte.py plus its input configuration.
    sys.path[:0] = [
        str(AUTOAS / "tools"),
        str(arxml / "src"),
        str(arxml / "examples/template"),
        str(cfile / "src"),
        str(AUTOAS / "app/app/config/Com/GEN"),
    ]
    import lxml.etree  # noqa: F401: upstream arxml needs lxml
    outputs = []
    for component in COMPONENTS:
        folder = AUTOAS / "app/app/config/SWC" / component
        spec = importlib.util.spec_from_file_location(
            "autosar_rte_example_" + component, folder / "rte.py")
        if spec is None or spec.loader is None:
            raise RuntimeError(f"Missing upstream component generator for {component}")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        generated = folder / "GEN"
        generated.mkdir(exist_ok=True)
        module.main(str(generated))
        header = generated / f"Rte_{component}.h"
        xml_files = sorted(generated.glob("*.arxml"))
        if not header.is_file() or len(xml_files) < 1:
            raise RuntimeError(f"Failed to generate Runtime Environment for {component}")
        # Actual XML parse, not a filename-only test.
        for path in xml_files:
            ElementTree.parse(path)
        body = header.read_text()
        if ("Rte_" not in body or len(body) < 30):
            raise RuntimeError(f"RTE header contents invalid for {component}")
        outputs.extend([header, *xml_files])
        print(f"PASS {component}: Rte_{component}.h and "
              f"{len(xml_files)} parseable ARXML files", flush=True)
    print("PASS: upstream RTE software-component generator produced "
          f"{len(outputs)} verified artifacts", flush=True)
    print("LIMIT: generation warns that EcuM CurrentMode ModeSwitchInterface "
          "is not supported; outputs are a teaching subset, not a complete "
          "integrated production RTE", flush=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.CalledProcessError, OSError, ElementTree.ParseError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
