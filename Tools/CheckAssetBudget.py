#!/usr/bin/env python3
"""Content budget audit for the RoadToWisdom repository.

Reports the total size of git-tracked content, the largest tracked files and fails
with a non-zero exit code when the content budget or the large-file threshold is
exceeded. Standard library only; no files are modified or cleaned.

Usage:
  python Tools/CheckAssetBudget.py [--budget-mb 250] [--large-file-mb 50] [--top 15]
"""

import argparse
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


def tracked_files() -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=REPO_ROOT, capture_output=True, text=False, check=True,
    )
    names = result.stdout.split(b"\0")
    return [REPO_ROOT / n.decode("utf-8") for n in names if n]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--budget-mb", type=float, default=250.0,
                        help="tracked game content budget in MiB (default 250)")
    parser.add_argument("--large-file-mb", type=float, default=50.0,
                        help="single-file warning threshold in MiB (default 50)")
    parser.add_argument("--top", type=int, default=15, help="how many largest files to print")
    args = parser.parse_args()

    # Content dirs are game assets; Plugins/UT* are junctions into UnrealTemplates and
    # gitignored, so a plain git ls-files never lists them twice.
    content_prefixes = ("Content/", "Config/", "Source/", "Docs/", "Tools/", "Plugins/")
    files = [p for p in tracked_files()
             if p.suffix.lower() in {".uasset", ".umap", ".uplugin", ".ini", ".cs",
                                     ".h", ".cpp", ".md", ".py", ".json", ".uproject",
                                     ".gitignore", ".vsconfig", ""}]
    files = [p for p in files if any(p.relative_to(REPO_ROOT).as_posix().startswith(cp) for cp in content_prefixes)]

    sizes: list[tuple[int, Path]] = []
    total = 0
    missing = 0
    for path in files:
        try:
            size = path.stat().st_size
        except OSError:
            missing += 1
            continue
        total += size
        sizes.append((size, path))

    sizes.sort(reverse=True)

    mib = total / (1024 * 1024)
    print(f"Tracked content: {len(files)} files, {mib:.1f} MiB (budget {args.budget_mb:.0f} MiB)")
    if missing:
        print(f"Note: {missing} tracked files missing on disk (renamed/deleted but not committed?).")

    print(f"\nLargest tracked files (top {args.top}):")
    for size, path in sizes[: args.top]:
        rel = path.relative_to(REPO_ROOT).as_posix()
        print(f"  {size / (1024 * 1024):8.2f} MiB  {rel}")

    exit_code = 0
    if mib > args.budget_mib if hasattr(args, "budget_mib") else mib > args.budget_mb:
        print(f"\nFAIL: content {mib:.1f} MiB exceeds the {args.budget_mb:.0f} MiB budget.")
        exit_code = 1

    large = [(s, p) for s, p in sizes if s > args.large_file_mb * 1024 * 1024]
    if large:
        print(f"\nWARNING: {len(large)} file(s) above {args.large_file_mb:.0f} MiB:")
        for size, path in large:
            print(f"  {size / (1024 * 1024):8.2f} MiB  {path.relative_to(REPO_ROOT).as_posix()}")
        if exit_code == 0:
            exit_code = 1

    if exit_code == 0:
        print("\nBudget check passed.")
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
