"""Compile and execute test-case programs against the built profiler.

A case is compiled from all `*.cpp` files in its directory with the mem_profile
Clang plugin and hook prelude (the same canonical flags the README documents),
then executed with the runtime library preloaded and MEM_PROFILE_OUT pointed
into the case's work directory.

The compiler and profiler build directory default per-platform and can be
overridden with the runner's --clangxx / --build-dir flags.
"""

from __future__ import annotations

import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parents[2]

COMPILE_TIMEOUT_S = 180
RUN_TIMEOUT_S = 60


@dataclass
class Toolchain:
    clangxx: Path
    plugin: Path
    runtime: Path
    prelude: Path
    preload_var: str

    @staticmethod
    def detect(clangxx: Optional[Path] = None, build_dir: Optional[Path] = None) -> Toolchain:
        """Locate clang++ and the built profiler libraries, or fail loudly.

        `clangxx` must be LLVM clang, not AppleClang (which silently ignores
        the plugin).
        """
        if sys.platform == "darwin":
            default_clangxx = Path("/opt/homebrew/opt/llvm/bin/clang++")
            lib_ext = "dylib"
            preload_var = "DYLD_INSERT_LIBRARIES"
        else:
            default_clangxx = Path("clang++")
            lib_ext = "so"
            preload_var = "LD_PRELOAD"

        clangxx = clangxx or default_clangxx
        build_dir = (build_dir or REPO_ROOT / "build").resolve()
        toolchain = Toolchain(
            clangxx=clangxx,
            plugin=build_dir / f"libmp_plugin.{lib_ext}",
            runtime=build_dir / f"libmp_runtime.{lib_ext}",
            prelude=REPO_ROOT / "mp" / "hook_prelude" / "include" / "mp_hook_prelude.h",
            preload_var=preload_var,
        )
        missing = [p for p in (toolchain.plugin, toolchain.runtime) if not p.exists()]
        if missing:
            raise SystemExit(
                f"profiler libraries not found: {', '.join(map(str, missing))}\n"
                f"Build the project first (`just build` or `cmake --build build`),"
                f" or set MP_BUILD_DIR."
            )
        return toolchain


def build_case(
    toolchain: Toolchain, case_dir: Path, work_dir: Path, extra_flags: list[str]
) -> Path:
    """Compile all .cpp files of `case_dir` into `work_dir`; return the binary path.

    Raises CaseCommandError on failure. The full compiler invocation and output
    are saved to `work_dir/build.log` either way.
    """
    sources = case_sources(case_dir)
    if not sources:
        raise CaseCommandError(f"no .cpp files in {case_dir}")
    exe = work_dir / case_dir.name
    cmd = plugin_compile_command(toolchain, sources, exe, extra_flags)
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=COMPILE_TIMEOUT_S)
    (work_dir / "build.log").write_text(
        f"$ {' '.join(cmd)}\n{result.stdout}{result.stderr}"
    )
    if result.returncode != 0:
        raise CaseCommandError(
            f"compilation failed (exit {result.returncode}), see {work_dir / 'build.log'}:\n"
            + _tail(result.stderr)
        )
    return exe


def run_case(toolchain: Toolchain, exe: Path, out_json: Path) -> subprocess.CompletedProcess[str]:
    """Run a case binary under the profiler runtime, writing its profile to `out_json`."""
    env = os.environ.copy()
    env[toolchain.preload_var] = str(toolchain.runtime)
    env["MEM_PROFILE_OUT"] = str(out_json)
    return subprocess.run(
        [str(exe)],
        capture_output=True,
        text=True,
        timeout=RUN_TIMEOUT_S,
        env=env,
        cwd=exe.parent,
    )


def case_sources(case_dir: Path) -> list[Path]:
    """The .cpp files making up a case, in stable order."""
    return sorted(case_dir.glob("*.cpp"))


def plugin_compile_command(
    toolchain: Toolchain, sources: list[Path], output: Path, extra_flags: list[str]
) -> list[str]:
    """The canonical plugin-enabled compile command for test-case sources."""
    return [
        str(toolchain.clangxx),
        "-std=c++20",
        "-Og",
        "-g",
        "-Wall",
        f"--include={toolchain.prelude}",
        f"-fplugin={toolchain.plugin}",
        *extra_flags,
        *map(str, sources),
        "-o",
        str(output),
    ]


class CaseCommandError(Exception):
    """A case's build or run step failed in a way that has no profile to check."""


def _tail(text: str, lines: int = 15) -> str:
    return "\n".join(text.splitlines()[-lines:])
