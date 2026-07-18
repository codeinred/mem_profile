#!/usr/bin/env python3
"""End-to-end test runner for mem_profile.

Each directory under tests/cases/ containing an expect.toml is a test case:
its .cpp files are compiled with the profiler plugin, executed under the
runtime library, and the resulting profile is checked against universal
invariants plus the case's expectations. See tests/CASES.md.

Usage:
    python3 tests/run.py                # run everything
    python3 tests/run.py trio lambda    # run cases matching these substrings
    python3 tests/run.py --list         # list cases
    python3 tests/run.py -v basic_new_delete   # verbose: show attribution

Requires Python >= 3.11 (tomllib). No third-party packages.
"""

from __future__ import annotations

import argparse
import shutil
import sys
import tomllib
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any, Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))

from harness.build import CaseCommandError, Toolchain, build_case, run_case
from harness.checks import check_expectations, check_invariants
from harness.profile import Profile

TESTS_DIR = Path(__file__).resolve().parent
CASES_DIR = TESTS_DIR / "cases"
WORK_DIR = TESTS_DIR / ".work"


def main() -> int:
    args = parse_args()
    cases = discover_cases(args.filters)
    if not cases:
        print("no cases matched", file=sys.stderr)
        return 2
    if args.list:
        for case in cases:
            print(case.name)
        return 0

    toolchain = Toolchain.detect(clangxx=args.clangxx, build_dir=args.build_dir)
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(lambda c: execute_case(toolchain, c, args.verbose), cases))

    return report(results)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("filters", nargs="*", help="substring filters on case names")
    parser.add_argument("--list", action="store_true", help="list matching cases and exit")
    parser.add_argument("-v", "--verbose", action="store_true", help="show per-case attribution")
    parser.add_argument("-j", "--jobs", type=int, default=8, help="parallel case builds")
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=None,
        help="build directory containing libmp_plugin / libmp_runtime (default: <repo>/build)",
    )
    parser.add_argument(
        "--clangxx",
        type=Path,
        default=None,
        help="LLVM clang++ to compile cases with (not AppleClang; default per platform)",
    )
    return parser.parse_args()


def discover_cases(filters: list[str]) -> list[Case]:
    cases = [
        Case(path)
        for path in sorted(CASES_DIR.iterdir())
        if (path / "expect.toml").is_file()
    ]
    if filters:
        cases = [c for c in cases if any(f in c.name for f in filters)]
    return cases


# ----------------------------------------------------------------------
# Case execution
# ----------------------------------------------------------------------


class Status(Enum):
    PASS = "PASS"
    FAIL = "FAIL"
    XFAIL = "XFAIL"  # failed, and the case is marked as a known issue
    XPASS = "XPASS"  # marked as a known issue but passed: the issue may be fixed!


@dataclass
class Case:
    path: Path

    @property
    def name(self) -> str:
        return self.path.name

    def spec(self) -> dict[str, Any]:
        with open(self.path / "expect.toml", "rb") as f:
            return tomllib.load(f)


@dataclass
class Result:
    case: Case
    status: Status
    errors: list[str] = field(default_factory=list)
    xfail_reason: Optional[str] = None
    detail: str = ""


def execute_case(toolchain: Toolchain, case: Case, verbose: bool) -> Result:
    spec = case.spec()
    errors, detail = collect_case_errors(toolchain, case, spec, verbose)
    return resolve_status(case, spec, errors, detail)


def collect_case_errors(
    toolchain: Toolchain, case: Case, spec: dict[str, Any], verbose: bool
) -> tuple[list[str], str]:
    """Build, run, and check one case; returns (violations, verbose detail)."""
    case_spec = spec.get("case", {})
    work_dir = WORK_DIR / case.name
    shutil.rmtree(work_dir, ignore_errors=True)
    work_dir.mkdir(parents=True)

    try:
        exe = build_case(toolchain, case.path, work_dir, spec.get("build", {}).get("flags", []))
        out_json = work_dir / "malloc_stats.json"
        proc = run_case(toolchain, exe, out_json)
    except CaseCommandError as e:
        return [str(e)], ""

    errors: list[str] = []
    expected_exit = case_spec.get("exit_code", 0)
    if proc.returncode != expected_exit:
        errors.append(
            f"exit code {proc.returncode} != expected {expected_exit}"
            + (f"\nstderr: {proc.stderr.strip()}" if proc.stderr.strip() else "")
        )
    if not out_json.is_file():
        errors.append(f"profiler produced no output file at {out_json}")
        return errors, ""

    profile = Profile(str(out_json))
    errors += [f"invariant: {e}" for e in check_invariants(profile)]
    errors += check_expectations(profile, spec)
    return errors, summarize(profile) if verbose else ""


def resolve_status(case: Case, spec: dict[str, Any], errors: list[str], detail: str) -> Result:
    case_spec = spec.get("case", {})
    xfail: Optional[str] = case_spec.get("xfail")
    platforms: list[str] = case_spec.get("xfail_platforms", [])
    xfail_applies = xfail is not None and (not platforms or sys.platform in platforms)

    if errors:
        status = Status.XFAIL if xfail_applies else Status.FAIL
    else:
        status = Status.XPASS if xfail_applies else Status.PASS
    return Result(case, status, errors, xfail if xfail_applies else None, detail)


def summarize(profile: Profile) -> str:
    lines = [f"typed: {profile.typed_free_bytes}  untyped: {profile.untyped_free_bytes}"]
    for name in sorted(profile.owned_bytes):
        lines.append(
            f"{profile.owned_bytes[name]:>10}b  {len(profile.objects[name]):>3} obj  {name}"
        )
    for (name, offset), n in sorted(profile.field_bytes.items()):
        lines.append(f"{n:>10}b  field {name}@{offset}")
    for (name, base), n in sorted(profile.base_bytes.items()):
        lines.append(f"{n:>10}b  base  {name} : {base}")
    return "\n".join(lines)


# ----------------------------------------------------------------------
# Reporting
# ----------------------------------------------------------------------

COLORS = {
    Status.PASS: "\x1b[32m",
    Status.FAIL: "\x1b[31m",
    Status.XFAIL: "\x1b[33m",
    Status.XPASS: "\x1b[35m",
}


def report(results: list[Result]) -> int:
    use_color = sys.stdout.isatty()
    for result in results:
        print(format_line(result, use_color))
        for error in result.errors if result.status is Status.FAIL else []:
            print(f"    - {error}")
        if result.status is Status.FAIL:
            print(f"    (artifacts: {WORK_DIR / result.case.name})")
        if result.detail:
            print("    " + result.detail.replace("\n", "\n    "))

    counts = {status: sum(r.status is status for r in results) for status in Status}
    summary = "  ".join(f"{status.value}={n}" for status, n in counts.items() if n)
    print(f"\n{len(results)} cases:  {summary}")

    for result in results:
        if result.status is Status.XPASS:
            print(
                f"note: {result.case.name} is marked xfail ({result.xfail_reason}) but PASSED —"
                f" if the issue is fixed, remove the xfail marker to lock in the fix."
            )
    return 1 if counts[Status.FAIL] else 0


def format_line(result: Result, use_color: bool) -> str:
    status = result.status
    tag = f"{status.value:<5}"
    if use_color:
        tag = f"{COLORS[status]}{tag}\x1b[0m"
    suffix = f"  (known issue: {result.xfail_reason})" if result.xfail_reason else ""
    return f"{tag}  {result.case.name}{suffix}"


if __name__ == "__main__":
    sys.exit(main())
