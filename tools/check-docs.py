#!/usr/bin/env python3
"""Check Markdown links, code fences, and in-line expansions of key AUTOSAR terms.

Excludes code fences and inline code: identifiers in program text must remain
exactly as the compiler/protocol expects. The full expansion is required on
each *prose* use; module identifiers in literal examples are intentionally exempt.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
PATHS = [ROOT / "README.md", *sorted((ROOT / "docs").glob("*.md")),
         *sorted((ROOT / "hardware").rglob("*.md")), ROOT / "examples/README.md"]

EXPANSIONS = {
    "BSW": ("Basic Software",),
    "MCAL": ("Microcontroller Abstraction Layer",),
    "RTE": ("Runtime Environment",),
    "SWC": ("Software Component",),
    "ECU": ("Electronic Control Unit",),
    "UDS": ("Unified Diagnostic Services",),
    "CanIf": ("CAN Interface", "Controller Area Network Interface"),
    "CanTp": ("CAN Transport Layer",),
    "PduR": ("Protocol Data Unit Router",),
    "Dcm": ("Diagnostic Communication Manager",),
    "PDU": ("Protocol Data Unit",),
}
bt = chr(96)
FENCE = re.compile(r"^\s*("+bt+r"{3,}|~{3,})")
INLINE = re.compile(bt + r"+[^" + bt + r"]*" + bt + r"+")
LINK = re.compile(r"\]\(([^)]+)\)")
TERM = re.compile(
    r"(?<![A-Za-z0-9_-])(?:"
    + "|".join(re.escape(k) for k in sorted(EXPANSIONS, key=len, reverse=True))
    + r")(?![A-Za-z0-9_-])"
)

def check() -> list[str]:
    problems = []
    for path in PATHS:
        label = str(path.relative_to(ROOT))
        start = None
        for number, line in enumerate(path.read_text().splitlines(), 1):
            marker = FENCE.match(line)
            if marker:
                token = marker.group(1)
                if start is None:
                    start = (token[0], len(token), number)
                elif start[0] == token[0] and len(token) >= start[1]:
                    start = None
                continue
            if start is not None:
                continue
            for ref in LINK.findall(line):
                if ref.startswith(("http:", "https:", "mailto:", "#")):
                    continue
                local = ref.split("#", 1)[0]
                # Local file links only; GitHub anchors without filenames are checked manually.
                if local and not (path.parent / local).exists():
                    problems.append(f"{label}:{number}: missing Markdown link {local}")
            # Preserve the readable label but not link destination, which can contain identifiers.
            visible = re.sub(r"\]\([^)]*\)", "]", line)
            visible = INLINE.sub("", visible)
            for hit in TERM.finditer(visible):
                alias = hit.group()
                context_before = visible[:hit.start()]
                context_after = visible[hit.end():]
                if context_after.startswith(")") and any(
                    context_before.casefold().endswith(full.casefold()+" (")
                    for full in EXPANSIONS[alias]
                ):
                    continue
                problems.append(
                    f"{label}:{number}: unexplained {alias}: {line.strip()[:135]}"
                )
        if start is not None:
            problems.append(f"{label}:{start[2]}: unclosed Markdown code fence")
    return problems


if __name__ == "__main__":
    errors = check()
    for error in errors:
        print("ERROR:", error)
    if errors:
        print(f"FAIL: {len(errors)} documentation issues")
        sys.exit(1)
    print(f"PASS: {len(PATHS)} Markdown files; local links, fences and key inline terms")
