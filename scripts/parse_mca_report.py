#!/usr/bin/env python3
import re
import sys
import json
regions = []
def parse_mca_report(filename):

    current = None

    with open(filename, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()

            # === Start of a new region ===
            m = re.search(r'\[\d+\]\s+Code Region - (.+)', line)
            if m:
                if current:
                    regions.append(current)
                current = {
                    'name': m.group(1).strip(),
                    'instructions': 0,
                    'cycles': 0,
                    'ipc': 0.0,
                    'uops': 0
                }
                continue

            if not current:
                continue

            # Extract metrics
            if 'Instructions:' in line:
                m = re.search(r'Instructions:\s*(\d+)', line)
                if m:
                    current['instructions'] = int(m.group(1))

            elif 'Total Cycles:' in line:
                m = re.search(r'Total Cycles:\s*(\d+)', line)
                if m:
                    current['cycles'] = int(m.group(1))

            elif 'IPC:' in line:
                m = re.search(r'IPC:\s*([\d.]+)', line)
                if m:
                    current['ipc'] = float(m.group(1))

            elif 'Total uOps:' in line:
                m = re.search(r'Total uOps:\s*(\d+)', line)
                if m:
                    current['uops'] = int(m.group(1))

    if current:
        regions.append(current)

    if not regions:
        print("❌ No regions found in the report.")
        return

    # === Print nice table ===
    print(f"{'Function':<40} {'Instructions':>12} {'Cycles':>10} {'IPC':>8} {'uOps':>8}")
    print("-" * 85)

    for r in regions:
        print(f"{r['name']:<40} {r['instructions']:12} {r['cycles']:10} "
              f"{r['ipc']:8.3f} {r['uops']:8}")

    print("\n=== Sorted by IPC (best → worst) ===")
    for r in sorted(regions, key=lambda x: x['ipc'], reverse=True):
        print(f"{r['name']:<40}  IPC = {r['ipc']:.3f}  "
              f"({r['instructions']} instr, {r['cycles']} cycles)")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 parse_mca_report.py <mca_report.txt>")
        sys.exit(1)
    parse_mca_report(sys.argv[1])

    regions_parse = {}
    for region in regions:
        regions_parse[region['name']] = region

    with open("output_artifacts/mca_report.json", "w") as out:
        json.dump(regions_parse, out, indent=2)