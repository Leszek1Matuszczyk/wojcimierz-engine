#!/usr/bin/env python3
# Wojcimierz Engine - scripts/compare_benchmarks.py
# Na chwałę czcigodnego Wojcimierza.
"""Porownuje wynik wz_benchmarks (--json) z baseline'em z main i wypisuje delte w %.

Uzycie:  python3 scripts/compare_benchmarks.py BASELINE.json CURRENT.json [--summary PLIK.md]
Nigdy nie blokuje CI: zawsze zwraca 0 (brak/zepsuty baseline -> ::notice::).
Obsluguje schema 2 (obiekt z "frame"/"peak_rss_mb"/"benchmarks") i stary format (lista).
Dodatnia delta = wolniej / wiecej pamieci (gorzej).
"""
import json
import pathlib
import sys

WARN_PCT = 10.0  # tylko oznaczenie w tabeli, nie blad


def load(path):
    data = json.loads(pathlib.Path(path).read_text(encoding="utf-8"))
    if isinstance(data, list):  # format sprzed schema 2
        data = {"benchmarks": data}
    metrics = {}
    frame = data.get("frame") or {}
    if "avg_ms" in frame:
        metrics["frame.avg_ms"] = float(frame["avg_ms"])
    if "p95_ms" in frame:
        metrics["frame.p95_ms"] = float(frame["p95_ms"])
    if "peak_rss_mb" in data:
        metrics["peak_rss_mb"] = float(data["peak_rss_mb"])
    for b in data.get("benchmarks", []):
        metrics[f"{b['name']} (ns/op)"] = float(b["ns_per_op"])
    return metrics, data


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    summary = None
    if "--summary" in argv:
        i = argv.index("--summary")
        summary = argv[i + 1] if i + 1 < len(argv) else None
        args = [a for a in args if a != summary]
    if len(args) != 2:
        print(__doc__)
        return 0
    try:
        base, base_raw = load(args[0])
    except (OSError, ValueError, KeyError, TypeError) as e:
        print(f"::notice::Brak poprawnego baseline'u z main ({e}) - porownanie pominiete.")
        return 0
    try:
        cur, cur_raw = load(args[1])
    except (OSError, ValueError, KeyError, TypeError) as e:
        print(f"::warning::Nie moge wczytac biezacych wynikow ({e}).")
        return 0

    if base_raw.get("quick") != cur_raw.get("quick"):
        print("::notice::Baseline i biezacy przebieg maja rozny tryb --quick; delty orientacyjne.")

    rows = []
    for name, value in cur.items():
        if name in base and base[name] > 0:
            pct = (value - base[name]) / base[name] * 100.0
            flag = " ⚠" if pct > WARN_PCT else ""
            rows.append((name, base[name], value, f"{pct:+.1f}%{flag}"))
        else:
            rows.append((name, None, value, "nowy"))

    w = max(len(r[0]) for r in rows) if rows else 10
    lines = [f"{'metryka':<{w}} {'main':>12} {'ten build':>12} {'delta':>10}"]
    for name, b, c, d in rows:
        bs = f"{b:.3f}" if b is not None else "-"
        lines.append(f"{name:<{w}} {bs:>12} {c:>12.3f} {d:>10}")
    print("\n".join(lines))
    print(f"(dodatnia delta = gorzej; ⚠ = ponad {WARN_PCT:.0f}% - tylko informacja, runnery CI sa zaszumione)")

    if summary:
        md = ["### Benchmarki vs main", "", "| metryka | main | ten build | delta |", "|---|---:|---:|---:|"]
        for name, b, c, d in rows:
            md.append(f"| {name} | {'-' if b is None else f'{b:.3f}'} | {c:.3f} | {d} |")
        md.append("")
        with open(summary, "a", encoding="utf-8") as f:
            f.write("\n".join(md) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
