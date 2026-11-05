"""Inventory every severity from saved Trivy reports without suppressing findings."""
import argparse
from collections import Counter
import csv
import json
from pathlib import Path
import re
import subprocess


def inventory(paths, verify_images=False):
    rows, images = [], []
    for path in paths:
        report = json.loads(path.read_text(encoding="utf-8"))
        # Trivy's archive config ID can differ from Docker's OCI manifest ID.
        # Never present them as the same identifier.
        match = re.fullmatch(r"sha256_([0-9a-f]{64})", path.stem)
        image = "sha256:" + match[1] if match else report["Metadata"]["ImageID"]
        if verify_images:
            if not match:
                raise ValueError("Layer verification requires an immutable sha256 report filename.")
            actual = json.loads(subprocess.check_output(["docker", "image", "inspect", image]))[0]
            if actual["RootFS"]["Layers"] != report["Metadata"]["DiffIDs"]:
                raise ValueError("Scanner report does not match image layers: " + image)
        findings = []
        for result in report.get("Results", []):
            for finding in result.get("Vulnerabilities", []):
                row = {"image": image, "target": result["Target"], "type": result["Type"],
                       "package": finding["PkgName"], "source_package": finding.get("SrcName", finding["PkgName"]),
                       "version": finding["InstalledVersion"], "advisory": finding["VulnerabilityID"],
                       "severity": finding["Severity"], "fixed_version": finding.get("FixedVersion", ""),
                       "vendor_status": finding.get("Status", "unknown"), "url": finding.get("PrimaryURL", "")}
                rows.append(row)
                findings.append(row)
        images.append({"image": image, "scanner_image_id": report["Metadata"]["ImageID"],
                       "layers_verified": verify_images, "created_at": report.get("CreatedAt"), "report": str(path),
                       "counts": dict(Counter(r["severity"] for r in findings)),
                       "listed_fixes": dict(Counter(r["severity"] for r in findings if r["fixed_version"])),
                       "vendor_status": dict(Counter(r["vendor_status"] for r in findings))})
    return {"images": images, "total_records": len(rows),
            "unique_package_advisories": len({(r["source_package"], r["advisory"]) for r in rows}),
            "limits": "Saved scanner records; no reachability conclusion or private-backport exemption is inferred."}, rows


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reports", nargs="+", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--verify-images", action="store_true")
    args = parser.parse_args()
    summary, rows = inventory(args.reports, args.verify_images)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    columns = ("image", "target", "type", "package", "source_package", "version", "advisory",
               "severity", "fixed_version", "vendor_status", "url")
    with (args.output / "findings.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=columns)
        writer.writeheader()
        writer.writerows(rows)
    print("Inventory written:", len(rows), "records; no findings suppressed.")
