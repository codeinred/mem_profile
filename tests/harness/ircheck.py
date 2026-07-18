"""IR-level verification that emitted destructors carry the instrumentation.

This is the authoritative check for rewrite-timing bugs (gh-2): the plugin's
log and an -ast-dump both show the injected payload even when CodeGen has
already emitted the *old* body, so only the IR can be trusted.

Each source file of a case is compiled a second time with `-S -emit-llvm`
(plugin enabled, same flags). The payload declares a function-local static
`__MP_TYPE_DATA` inside every rewritten destructor; in IR this surfaces as a
global named `_ZZ<dtor-encoding>E<len>__MP_TYPE_DATA`. The check asserts, for
every emitted base/complete destructor definition (`...D1Ev` / `...D2Ev`,
including abi-tagged variants):

- exactly one such global exists for that destructor — zero means the rewrite
  did not land (the gh-2 failure mode), two or more means the payload was
  double-injected;
- `available_externally` definitions are exempt (they mirror out-of-image
  code, e.g. extern-template members compiled into libc++).

Deleting destructors (`D0Ev`) are exempt: CodeGen emits them as a call to the
complete-object destructor plus `operator delete`, without the body.

A case opts in with an `[ir]` table in expect.toml; `instrumented = [...]`
additionally asserts that a destructor definition for each named class was
emitted at all (guarding against "no symbol, nothing to check" passing
vacuously).
"""

from __future__ import annotations

import re
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .build import COMPILE_TIMEOUT_S, Toolchain, case_sources, plugin_compile_command


def check_ir(
    toolchain: Toolchain,
    case_dir: Path,
    work_dir: Path,
    extra_flags: list[str],
    ir_spec: dict[str, Any],
) -> list[str]:
    """Emit LLVM IR for each source of the case and check instrumentation."""
    dtors: list[DtorDefinition] = []
    for source in case_sources(case_dir):
        ll_path = work_dir / f"{source.stem}.ll"
        errors = emit_ir(toolchain, source, ll_path, extra_flags)
        if errors:
            return errors
        dtors += scan_ir(ll_path.read_text())

    errors = []
    for dtor in dtors:
        if dtor.payload_count == 0:
            errors.append(f"ir: destructor {dtor.symbol} emitted without instrumentation")
        elif dtor.payload_count > 1:
            errors.append(
                f"ir: destructor {dtor.symbol} instrumented {dtor.payload_count} times"
                f" (payload double-injected)"
            )

    for class_name in ir_spec.get("instrumented", []):
        token = f"{len(class_name)}{class_name}"
        if not any(token in d.symbol for d in dtors):
            emitted = ", ".join(d.symbol for d in dtors) or "<none>"
            errors.append(
                f"ir: no destructor definition emitted for {class_name!r} (emitted: {emitted})"
            )
    return errors


@dataclass
class DtorDefinition:
    """An emitted D1/D2 destructor definition and its payload-static count."""

    symbol: str
    payload_count: int


def emit_ir(
    toolchain: Toolchain, source: Path, ll_path: Path, extra_flags: list[str]
) -> list[str]:
    cmd = plugin_compile_command(toolchain, [source], ll_path, extra_flags)
    cmd += ["-S", "-emit-llvm"]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=COMPILE_TIMEOUT_S)
    if result.returncode != 0:
        return [f"ir: -emit-llvm compile of {source.name} failed:\n{result.stderr.strip()}"]
    return []


# A base/complete destructor symbol, allowing an abi-tag between the
# discriminator and Ev (e.g. libc++'s ...D2B8ne200100Ev).
DTOR_SYMBOL = re.compile(r"D[12](?:B[0-9A-Za-z]+)?Ev$")

# `define` lines, capturing attributes and the (possibly quoted) symbol.
DEFINE_LINE = re.compile(r'^define\s+(?P<attrs>[^@]*)@(?P<q>"?)(?P<sym>[^"(]+)(?P=q)\(')

# Global definitions, e.g. `@_ZZN3FooD2EvE14__MP_TYPE_DATA = ...`.
GLOBAL_LINE = re.compile(r'^@(?P<q>"?)(?P<sym>[^"=\s]+)(?P=q)\s*=')


def scan_ir(ir_text: str) -> list[DtorDefinition]:
    """Collect instrumentable destructor definitions and their payload counts."""
    defines: list[str] = []
    globals_: list[str] = []
    for line in ir_text.splitlines():
        if match := DEFINE_LINE.match(line):
            if "available_externally" not in match["attrs"]:
                defines.append(match["sym"])
        elif match := GLOBAL_LINE.match(line):
            globals_.append(match["sym"])

    dtors = []
    for symbol in defines:
        match = DTOR_SYMBOL.search(symbol)
        if match is None or not symbol.startswith("_ZN"):
            continue
        count = sum(
            any(g.startswith(p) for p in _static_prefixes(symbol, match.start()))
            and "__MP_TYPE_DATA" in g
            for g in globals_
        )
        dtors.append(DtorDefinition(symbol, count))
    return dtors


def _static_prefixes(symbol: str, dtor_pos: int) -> list[str]:
    """Mangled-name prefixes of `symbol`'s function-local statics.

    The payload static is mangled against one destructor variant (typically
    D1) while the emitted definition may be another (typically D2, with D1 an
    alias to it), so all variants of the discriminator are accepted. The
    function-local static of _ZN<enc> is mangled _ZZ<enc>E<...>.
    """
    return [
        "_ZZ" + symbol[2:dtor_pos] + f"D{variant}" + symbol[dtor_pos + 2 :] + "E"
        for variant in (0, 1, 2)
    ]
