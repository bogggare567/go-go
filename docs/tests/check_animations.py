#!/usr/bin/env python3
"""Frame-by-frame sanity check for the animated SVGs in img/.

Parses every @keyframes block in each SVG's <style>, groups the ones that
represent mutually-exclusive "screen" states (a device only ever shows one
screen at a time), and walks the 0-100% timeline in 0.1% steps looking for:

  - OVERLAP: two states with opacity > 0 at the same instant (would render
    as garbled double-exposed screens)
  - GAP: no state visible at all (a blank/black frame)

A few sub-percent gaps at hard screen-transition points are expected (see
generate_demo.py's k-boot/k-go/k-gosent/k-panic keyframes) and are reported
but not treated as failures — anything wider than ~2% is worth a look.

Usage: python3 docs/tests/check_animations.py  (run from anywhere; paths
are resolved relative to the repo root)
"""
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Which keyframe names represent one mutually-exclusive "screen" group per
# animation. Extend this if a new scenario animation adds another group.
# allow_gaps=True marks a group of transient overlays on top of an
# always-visible base screen that isn't itself one of these keyframes (e.g.
# the LoRa gateway's go/panic "command received" flashes over its base
# LINK OK screen) - a gap there just means the base screen alone is
# showing, not a blank frame. Still checked for OVERLAP either way.
GROUPS = {
    "img/gogo-demo.svg": [("main", ["k-boot", "k-go", "k-gosent", "k-panic"], False)],
    "img/scenario-ble.svg": [("main", ["k-boot", "k-go", "k-gosent", "k-panic"], False)],
    "img/scenario-lora.svg": [
        ("TX", ["k-boot", "k-go", "k-gosent", "k-panic"], False),
        ("RX", ["k-go-rx", "k-gosent-rx", "k-panic-rx"], True),
    ],
}

GAP_FAIL_THRESHOLD_PCT = 2.0  # wider than this is a real bug, not a transition blip


def extract_style(path):
    s = open(path, encoding="utf-8").read()
    m = re.search(r"<style[^>]*>(.*?)</style>", s, re.S)
    return m.group(1) if m else ""


def parse_keyframes(css):
    """Balanced-brace parse - a naive non-greedy regex stops at the first
    inner '}' and silently mis-parses multi-step keyframes. Don't simplify
    this to a one-liner regex again (bit us once already)."""
    kfs = {}
    for m in re.finditer(r"@keyframes\s+([\w-]+)\s*\{", css):
        name = m.group(1)
        start = m.end()
        depth = 1
        j = start
        while depth > 0 and j < len(css):
            if css[j] == "{":
                depth += 1
            elif css[j] == "}":
                depth -= 1
            j += 1
        body = css[start:j - 1]
        steps = []
        for sm in re.finditer(r"([\d.,%\s]+)\{([^}]*)\}", body):
            pcts = [float(p.strip().rstrip("%")) for p in sm.group(1).split(",") if p.strip()]
            steps.append((pcts, sm.group(2).strip()))
        kfs[name] = steps
    return kfs


def opacity_series(steps):
    anchors = []
    for pcts, props in steps:
        m = re.search(r"opacity\s*:\s*([\d.]+)", props)
        if m:
            op = float(m.group(1))
            anchors.extend((p, op) for p in pcts)
    anchors.sort()
    return anchors


def opacity_at(anchors, pct):
    val = anchors[0][1] if anchors else None
    for p, op in anchors:
        if p <= pct + 1e-9:
            val = op
        else:
            break
    return val


def check_file(relpath, groups):
    path = os.path.join(REPO, relpath)
    if not os.path.exists(path):
        return [f"{relpath}: MISSING"]

    css = extract_style(path)
    kfs = parse_keyframes(css)
    problems = []

    for label, names, allow_gaps in groups:
        present = [n for n in names if n in kfs]
        missing = [n for n in names if n not in kfs]
        if missing:
            problems.append(f"{relpath} [{label}]: keyframes not found: {missing}")
        series = {n: opacity_series(kfs[n]) for n in present}

        issues = []
        for tenth in range(0, 1001):
            pct = tenth / 10.0
            on = [n for n in present if (opacity_at(series[n], pct) or 0) > 0.01]
            if len(on) > 1:
                issues.append((pct, "OVERLAP", tuple(sorted(on))))
            elif len(on) == 0:
                issues.append((pct, "GAP", None))

        collapsed = []
        for pct, kind, info in issues:
            if collapsed and collapsed[-1][1] == kind and collapsed[-1][3] == info and pct - collapsed[-1][2] <= 0.15:
                collapsed[-1] = (collapsed[-1][0], kind, pct, info)
            else:
                collapsed.append((pct, kind, pct, info))

        for start, kind, end, info in collapsed:
            width = end - start
            if kind == "OVERLAP":
                problems.append(f"{relpath} [{label}]: OVERLAP {start:.1f}%-{end:.1f}% {info}")
            elif kind == "GAP" and not allow_gaps and width > GAP_FAIL_THRESHOLD_PCT:
                problems.append(f"{relpath} [{label}]: GAP {start:.1f}%-{end:.1f}% ({width:.1f}% wide)")
    return problems


def main():
    all_problems = []
    for relpath, groups in GROUPS.items():
        all_problems += check_file(relpath, groups)

    if all_problems:
        print(f"FAIL - {len(all_problems)} issue(s):")
        for p in all_problems:
            print("  " + p)
        sys.exit(1)

    print(f"OK - checked {len(GROUPS)} animation(s), no overlaps or unexplained gaps.")


if __name__ == "__main__":
    main()
