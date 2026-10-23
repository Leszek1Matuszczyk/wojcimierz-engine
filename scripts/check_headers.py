#!/usr/bin/env python3
"""Sprawdza, czy kazdy plik zrodlowy ma standardowy naglowek Wojcimierza.

Skanowane katalogi: engine/, editor/, sandbox/, tests/, benchmarks/ (i render_tests/).
Pliki: *.h, *.hpp, *.cpp. Wymagana linia zawierajaca:
    Na chwałę czcigodnego Wojcimierza
w pierwszych 15 liniach pliku.

Uzycie:  python3 scripts/check_headers.py [--fix]
  --fix  dopisuje brakujacy naglowek na poczatku pliku.
Kod wyjscia: 0 = OK, 1 = brakujace naglowki.
"""
import pathlib
import sys

MARKER = "Na chwałę czcigodnego Wojcimierza"
DIRS = ["engine", "editor", "sandbox", "tests", "benchmarks", "render_tests"]
EXTS = {".h", ".hpp", ".cpp"}
HEAD_LINES = 15


def header_for(rel: str) -> str:
    return (
        "// =============================================================================\n"
        f"// Wojcimierz Engine - {rel}\n"
        f"// {MARKER}.\n"
        "// =============================================================================\n"
    )


def main() -> int:
    root = pathlib.Path(__file__).resolve().parent.parent
    fix = "--fix" in sys.argv[1:]
    missing = []
    checked = 0
    for d in DIRS:
        base = root / d
        if not base.is_dir():
            continue
        for p in sorted(base.rglob("*")):
            if p.suffix not in EXTS or not p.is_file():
                continue
            checked += 1
            text = p.read_text(encoding="utf-8")
            head = text.splitlines()[:HEAD_LINES]
            if any(MARKER in line for line in head):
                continue
            rel = p.relative_to(root).as_posix()
            if fix:
                p.write_text(header_for(rel) + text, encoding="utf-8")
                print(f"[naprawiono] {rel}")
            else:
                missing.append(rel)
    if missing:
        print(f"Brak naglowka '{MARKER}' w {len(missing)} plikach:")
        for m in missing:
            print(f"  - {m}")
        print("Napraw: python3 scripts/check_headers.py --fix")
        return 1
    print(f"OK: {checked} plikow ma naglowek Wojcimierza.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
