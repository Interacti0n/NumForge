"""Scan local image archives without granting the scanner Docker socket access."""
import argparse
from collections import Counter
import json
from pathlib import Path
import subprocess
import tempfile

SCANNER = "aquasec/trivy:0.76.1"


def fixable_findings(report, all_severities=False):
    return [v for result in report.get("Results", []) for v in result.get("Vulnerabilities", [])
            if v.get("FixedVersion") and (all_severities or v["Severity"] in {"HIGH", "CRITICAL"})]


def scan(image, output, all_severities=False):
    output.mkdir(parents=True, exist_ok=True)
    name = image.replace("/", "_").replace(":", "_") + ".json"
    with tempfile.TemporaryDirectory(prefix="trivy-", dir=output) as temporary:
        archive = Path(temporary).resolve()
        # Binary stdout avoids PowerShell's text-redirection conversion.
        with (archive / "image.tar").open("wb") as stream:
            subprocess.run(["docker", "image", "save", image], stdout=stream, check=True)
        subprocess.run(["docker", "run", "--rm", "--security-opt=no-new-privileges:true",
            "--mount", f"type=bind,source={archive},target=/input,readonly",
            "--mount", f"type=bind,source={output.resolve()},target=/reports",
            "--mount", "type=volume,source=numforge-trivy-cache,target=/root/.cache/trivy",
            SCANNER, "image", "--input", "/input/image.tar", "--scanners", "vuln",
            "--format", "json", "--output", "/reports/" + name, "--timeout", "15m"], check=True)
    report = json.loads((output / name).read_text(encoding="utf-8"))
    vulnerabilities = [v for result in report.get("Results", []) for v in result.get("Vulnerabilities", [])]
    counts = Counter(v["Severity"] for v in vulnerabilities)
    fixable = fixable_findings(report, all_severities)
    print(image, dict(counts), "fixable all severities:" if all_severities else "fixable HIGH/CRITICAL:", len(fixable), flush=True)
    return bool(fixable)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("images", nargs="+")
    parser.add_argument("--output", type=Path, default=Path("build/security"))
    parser.add_argument("--fail-on-fixable", action="store_true", help="Legacy gate: fail on listed HIGH/CRITICAL fixes")
    parser.add_argument("--fail-on-any-fix", action="store_true", help="Fail on a listed fix at any severity, including UNKNOWN")
    args = parser.parse_args()
    failed = False
    for image in args.images:
        failed |= scan(image, args.output, all_severities=args.fail_on_any_fix)
    raise SystemExit(1 if failed and (args.fail_on_fixable or args.fail_on_any_fix) else 0)
