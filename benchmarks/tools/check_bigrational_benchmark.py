"""Independent Fraction fixtures, validation and serial rational recordings."""
import argparse
import csv
from fractions import Fraction
from decimal import Decimal, localcontext, ROUND_DOWN, ROUND_UP, ROUND_FLOOR, ROUND_CEILING, ROUND_HALF_UP, ROUND_HALF_EVEN
import hashlib
import json
from pathlib import Path
import platform
import random
import subprocess
import sys
import tempfile
from datetime import datetime, timezone

# Dispatch-boundary references include complete squares of 8193-bit parts.
if hasattr(sys, "set_int_max_str_digits"):
    sys.set_int_max_str_digits(100000)

def fixtures(random_count=0):
    pairs = []
    for bits in (32, 256, 1024):
        n = 2**bits
        for shape, a, b in (
            ("equal", Fraction(n + 1, n - 1), Fraction(n + 3, n - 1)),
            ("coprime", Fraction(n + 1, n - 1), Fraction(n + 7, n + 9)),
            ("shared", Fraction(1, 3 * n), Fraction(5, 9 * n)),
            ("cancel", Fraction(n + 1, n - 1), Fraction(n - 1, n + 1)),
            ("integer", Fraction(n + 1), Fraction(-7)),
            ("zero", Fraction(0), Fraction(n + 1, n - 1)),
            ("one", Fraction(n + 1, n - 1), Fraction(1)),
            ("negative", Fraction(-n - 1, n - 1), Fraction(-n - 3, n - 1)),
            ("dyadic", Fraction(n + 1, 2 * n), Fraction(n - 1, 4 * n)),
            ("five", Fraction(-n - 1, 5**bits), Fraction(1, 3)),
            ("finite_mixed", Fraction(n + 1, 2**(bits+3) * 5**(bits-1)), Fraction(1, 7)),
        ):
            pairs.append((f"{shape}_{bits}", a, b))
    # Scalar 5^27 versus multi-limb 5^28, binary limb boundaries, and the
    # integer shortcut. Both orientations exercise the exact finite converter.
    for power in (0, 1, 26, 27, 28, 63, 64, 65, 127, 128, 129):
        a, b = Fraction(7, 2**power), Fraction(-11, 5**power)
        pairs.append((f"binary_boundary_{power}", a, b))
        pairs.append((f"five_boundary_{power}", b, a))
    for bits in (959, 960, 961, 1023, 1024, 1025, 8127, 8128, 8129, 8191, 8192, 8193):
        n = 2**bits
        pairs.append((f"equal_boundary_{bits}", Fraction(1-n, n), Fraction(n-3, n)))
    rng = random.Random(20261030)
    for i in range(random_count):
        bits = rng.choice((8, 64, 255, 1024))
        a = Fraction(rng.getrandbits(bits) * rng.choice((-1, 1)), rng.getrandbits(bits) or 1)
        b = Fraction(rng.getrandbits(bits) * rng.choice((-1, 1)), rng.getrandbits(bits) or 1)
        pairs.append((f"random_{i}", a, b))
    # Canonical equal denominators and integer translation in both orders.
    # Keep this seed separate so established random fixtures are unchanged.
    common_rng = random.Random(2026103017)
    for i in range(random_count // 4):
        bits = common_rng.choice((8, 64, 255, 1024))
        denominator = 2**bits
        a = Fraction((common_rng.getrandbits(bits) | 1) * common_rng.choice((-1, 1)), denominator)
        b = Fraction((common_rng.getrandbits(bits) | 1) * common_rng.choice((-1, 1)), denominator)
        integer = Fraction(common_rng.getrandbits(bits) * common_rng.choice((-1, 1)))
        pairs.extend(((f"random_equal_{i}", a, b), (f"random_translate_{i}", a, integer),
                      (f"random_translate_reverse_{i}", integer, a)))
    for name, a, b in pairs:
        operations = {"add": a + b, "sub": a - b, "mul": a * b, "square": a * a,
                      "compare": (a > b) - (a < b)}
        if a:
            operations["selfdiv"] = Fraction(1)
        if b:
            operations["div"] = a / b
        for mode, rounding in enumerate((ROUND_DOWN, ROUND_UP, ROUND_FLOOR, ROUND_CEILING, ROUND_HALF_UP, ROUND_HALF_EVEN)):
            denominator = a.denominator
            for prime in (2, 5):
                while denominator % prime == 0:
                    denominator //= prime
            with localcontext() as context:
                # Exact-first conversion intentionally retains all terminating
                # digits, even when more than 17 places are requested.
                context.prec = max(len(str(abs(a.numerator))) + 4 * a.denominator.bit_length(), 17) if denominator == 1 else 17
                context.rounding = rounding
                decimal = format(Decimal(a.numerator) / Decimal(a.denominator), 'f')
            if '.' in decimal:
                decimal = decimal.rstrip('0').rstrip('.')
            operations[f"convert{mode}"] = decimal
        for operation, expected in operations.items():
            yield (f"{name}_{operation}", operation, a.numerator, a.denominator,
                   b.numerator, b.denominator, str(expected))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--record", type=Path, help="New empty output directory; three serial runs")
    parser.add_argument("--random", type=int, default=40)
    args = parser.parse_args()
    if args.random < 0:
        parser.error("--random must be nonnegative")
    rows = list(fixtures(args.random))
    executable = args.executable.resolve()
    with tempfile.TemporaryDirectory(prefix="numforge-rational-", dir=executable.parent) as tmp:
        directory = Path(tmp)
        if args.record:
            args.record.mkdir(parents=True, exist_ok=False)
            directory = args.record
        inputs = directory / "fixtures.tsv"
        inputs.write_text("".join("\t".join(map(str, row)) + "\n" for row in rows), encoding="ascii")
        validated = subprocess.run([str(executable), str(inputs), "--validate"],
                                   capture_output=True, text=True, check=True, timeout=180)
        parsed = list(csv.DictReader(validated.stdout.splitlines()))
        assert len(parsed) == len(rows) and all(r["verification"] == "verified" for r in parsed)
        # The harness must reject an independent reference mismatch.
        corrupt = directory / "corrupt.tsv"
        bad = list(rows[0]); bad[-1] = "123456789"
        corrupt.write_text("\t".join(map(str, bad)) + "\n", encoding="ascii")
        rejected = subprocess.run([str(executable), str(corrupt), "--validate"], capture_output=True, timeout=10)
        assert rejected.returncode != 0, "Corrupt reference was accepted"
        memory = subprocess.run([str(executable), str(inputs), "--memory"],
                                capture_output=True, text=True, check=True, timeout=180)
        assert len(list(csv.DictReader(memory.stdout.splitlines()))) == len(rows)
        if args.record:
            # Speed recordings use the small fixed corpus; random properties only
            # validate correctness and must not silently change the timed suite.
            fixed = list(fixtures())
            timed = directory / "timed.tsv"
            timed.write_text("".join("\t".join(map(str, row)) + "\n" for row in fixed), encoding="ascii")
            for i in range(3):
                result = subprocess.run([str(executable), str(timed)], capture_output=True, check=True, timeout=180)
                (directory / f"run-{i+1}.csv").write_bytes(result.stdout)
            profile = subprocess.run([str(executable), str(timed), "--profile"], capture_output=True, check=True, timeout=180)
            (directory / "profile.csv").write_bytes(profile.stdout)
            (directory / "validation.csv").write_text(validated.stdout, encoding="ascii")
            cache = executable.parent / "CMakeCache.txt"
            compiler = [line for line in cache.read_text(encoding="utf8").splitlines()
                        if line.startswith(("CMAKE_C_COMPILER:", "CMAKE_BUILD_TYPE:", "CMAKE_C_FLAGS:", "CMAKE_C_FLAGS_RELEASE:",
                                            "BUILD_TESTING:", "NUMFORGE_ENABLE_SANITIZERS:", "NUMFORGE_ENABLE_COVERAGE:",
                                            "NUMFORGE_BUILD_BENCHMARKS:"))] if cache.exists() else []
            metadata = {"recorded_at": datetime.now(timezone.utc).isoformat(),
                        "revision": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
                        "platform": platform.platform(), "processor": platform.processor(),
                        "compiler_cache": compiler,
                        "executable": str(executable), "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
                        "fixture_sha256": hashlib.sha256(timed.read_bytes()).hexdigest(),
                        "note": "Five calibrated samples per row, three serial processes; profile is separate. No affinity/power lock."}
            (directory / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf8")
            (directory / "source.patch").write_bytes(subprocess.check_output(["git", "diff", "HEAD"]))
        print(f"Verified {len(rows)} Fraction/Decimal references, arithmetic aliases, zero live payload after teardown and corrupt-reference rejection.")


if __name__ == "__main__":
    main()
