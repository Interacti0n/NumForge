"""Independent standard-library Decimal references for graph audit findings."""
import argparse
import json
import re
from decimal import Decimal as D, localcontext
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("recording", type=Path)
parser.add_argument("--output", required=True, type=Path)
args = parser.parse_args()
narrow_parameters = {}


def reference(name, x):
    if name in narrow_parameters:
        sign, centre, width = narrow_parameters[name]
        return sign / (1 + ((x - centre) / width) ** 2)
    if name == "lorentz-narrow":
        return 1 / (1 + ((x - D(".1234")) / D(".04")) ** 2)
    if name == "gaussian-narrow":
        return (-((x - D(".1234")) / D(".04")) ** 2).exp()
    if name in ("hyperbolic-steep", "cosh-steep"):
        a = (10 * x).exp()
        b = (-10 * x).exp()
        return (a - b if name == "hyperbolic-steep" else a + b) / 2
    if name.startswith("nested-log"):
        return 1 / x.sqrt().ln().ln() ** 2
    if name == "complex-abs":
        return abs(x).sqrt()
    raise ValueError(name)


recording = json.loads(args.recording.read_text(encoding="utf-8"))
for row in recording["rows"]:
    match = re.fullmatch(r"(-?1)/\(1\+\(\(x-\(([-\d.]+)\)\)/([\d.]+)\)\^2\)", row["expression"])
    if match:
        narrow_parameters[row["case"]] = tuple(D(value) for value in match.groups())
rows = []
for row in recording["rows"]:
    if row["version"] != "current" or row["repeat"] != 1 or (row["case"] not in narrow_parameters and row["case"] not in (
        "lorentz-narrow", "gaussian-narrow", "hyperbolic-steep", "cosh-steep",
        "nested-log", "nested-log-wide", "complex-abs"
    )):
        continue
    data = json.loads(row["payload"])
    count, error, convergence = 0, D(0), D(0)
    for p in data["points"]:
        compact = isinstance(p, list)
        if (p[3] != 0 if compact else p["kind"] != "real"):
            continue
        x, y = D(str(p[0] if compact else p["x"])), D(str(p[1] if compact else p["y"]))
        if not D(str(row["camera"]["ymin"])) <= y <= D(str(row["camera"]["ymax"])):
            continue
        with localcontext() as context:
            context.prec = 100
            a = reference(row["case"], x)
        with localcontext() as context:
            context.prec = 140
            b = reference(row["case"], x)
            scale = max(D(1), abs(b))
            convergence = max(convergence, abs(a - b) / scale)
            error = max(error, abs(y - b) / scale)
        count += 1
    if convergence >= D("1e-90"):
        raise ValueError("Reference precision did not converge")
    worst = row.get("worst")
    with localcontext() as context:
        context.prec = 140
        worst_reference = str(reference(row["case"], D(str(worst["x"])))) if worst else None
    rows.append({"case": row["case"], "visible_point_checks": count,
                 "max_scaled_point_error": str(error),
                 "max_reference_100_vs_140_digits": str(convergence),
                 "worst_render_coordinate": worst["x"] if worst else None,
                 "worst_render_decimal_reference": worst_reference})
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps({"reference": "Python Decimal 100 and 140 digits; sampled visible point values only, not interpolation or continuity proofs", "rows": rows}, indent=2) + "\n", encoding="utf-8")
print(json.dumps(rows, indent=2))
