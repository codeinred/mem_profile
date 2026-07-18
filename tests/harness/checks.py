"""Checks run against a profile: universal invariants and per-case expectations.

Both entry points return a list of human-readable violation messages; an empty
list means the profile passed.

- `check_invariants(profile)` needs no configuration and is run on every case.
- `check_expectations(profile, spec)` checks the case's parsed `expect.toml`.

Expectation bounds are written either as a bare integer (exact) or as a table
with any of `eq` / `min` / `max`, e.g. `bytes = 4000` or `bytes = { min = 1 }`.
Type names are matched with glob patterns (`fnmatch`); a name without wildcard
characters is an exact match.
"""

from __future__ import annotations

from typing import Any, Optional, Union

from .profile import FieldInfo, Profile, TypeInfo

Bound = Union[int, dict[str, int]]


# ----------------------------------------------------------------------
# Expectations
# ----------------------------------------------------------------------


def check_expectations(profile: Profile, spec: dict[str, Any]) -> list[str]:
    """Check a case's expect.toml `spec` against `profile`."""
    errors: list[str] = []
    _check_totals(profile, spec.get("totals", {}), errors)
    for type_spec in spec.get("types", []):
        _check_type(profile, type_spec, errors)
    return errors


def _check_totals(profile: Profile, totals: dict[str, Bound], errors: list[str]) -> None:
    actuals = {
        "typed_free_bytes": profile.typed_free_bytes,
        "untyped_free_bytes": profile.untyped_free_bytes,
    }
    for key, bound in totals.items():
        if key not in actuals:
            errors.append(f"totals: unknown key {key!r}")
            continue
        _check_bound(f"totals.{key}", actuals[key], bound, errors)


def _check_type(profile: Profile, type_spec: dict[str, Any], errors: list[str]) -> None:
    pattern: str = type_spec["type"]
    entries = profile.type_entries(pattern)
    if not entries:
        known = ", ".join(sorted(t.name for t in profile.types)) or "<none>"
        errors.append(f"type {pattern!r}: not present in type table (present: {known})")
        return

    label = f"type {pattern!r}"
    names = {e.name for e in entries}

    # Metadata assertions apply to every matching table entry.
    for entry in entries:
        if "size" in type_spec:
            _check_bound(f"{label}: size", entry.size, type_spec["size"], errors)

    # Attribution assertions aggregate over all matching names.
    if "bytes" in type_spec:
        actual = sum(profile.owned_bytes.get(n, 0) for n in names)
        _check_bound(f"{label}: bytes", actual, type_spec["bytes"], errors)
    if "self_bytes" in type_spec:
        actual = sum(profile.self_bytes.get(n, 0) for n in names)
        _check_bound(f"{label}: self_bytes", actual, type_spec["self_bytes"], errors)
    if "objects" in type_spec:
        object_ids: set[int] = set()
        for n in names:
            object_ids |= profile.objects.get(n, set())
        _check_bound(f"{label}: objects", len(object_ids), type_spec["objects"], errors)

    for field_spec in type_spec.get("fields", []):
        _check_field(profile, entries, label, field_spec, errors)
    for base_spec in type_spec.get("bases", []):
        _check_base(profile, entries, label, base_spec, errors)


def _check_field(
    profile: Profile,
    entries: list[TypeInfo],
    label: str,
    field_spec: dict[str, Any],
    errors: list[str],
) -> None:
    """Check one `[[types.fields]]` entry.

    The field is selected by `name`, or by `offset` for unnamed fields (lambda
    captures). Metadata bounds (`offset`, `size`) are checked on each matching
    type entry; attribution (`bytes`) is summed across distinct (type, offset)
    credits.
    """
    selector = field_spec.get("name")
    field_label = f"{label} field {selector or ('@' + str(field_spec.get('offset')))!r}"

    credited: set[tuple[str, int]] = set()
    found = False
    for entry in entries:
        field = _select_field(entry, field_spec)
        if field is None:
            continue
        found = True
        if "offset" in field_spec:
            _check_bound(f"{field_label}: offset", field.offset, field_spec["offset"], errors)
        if "size" in field_spec:
            _check_bound(f"{field_label}: size", field.size, field_spec["size"], errors)
        credited.add((entry.name, field.offset))

    if not found:
        available = ", ".join(f"{f.name}@{f.offset}" for e in entries for f in e.fields)
        errors.append(f"{field_label}: no such field (available: {available or '<none>'})")
        return

    if "bytes" in field_spec:
        actual = sum(profile.field_bytes.get(key, 0) for key in credited)
        _check_bound(f"{field_label}: bytes", actual, field_spec["bytes"], errors)


def _check_base(
    profile: Profile,
    entries: list[TypeInfo],
    label: str,
    base_spec: dict[str, Any],
    errors: list[str],
) -> None:
    """Check one `[[types.bases]]` entry, selected by base type name."""
    base_type: str = base_spec["type"]
    base_label = f"{label} base {base_type!r}"

    credited: set[tuple[str, str]] = set()
    found = False
    for entry in entries:
        for base in entry.bases:
            if base.type != base_type:
                continue
            found = True
            if "offset" in base_spec:
                _check_bound(f"{base_label}: offset", base.offset, base_spec["offset"], errors)
            if "size" in base_spec:
                _check_bound(f"{base_label}: size", base.size, base_spec["size"], errors)
            credited.add((entry.name, base.type))

    if not found:
        available = ", ".join(b.type for e in entries for b in e.bases)
        errors.append(f"{base_label}: no such base (available: {available or '<none>'})")
        return

    if "bytes" in base_spec:
        actual = sum(profile.base_bytes.get(key, 0) for key in credited)
        _check_bound(f"{base_label}: bytes", actual, base_spec["bytes"], errors)


def _select_field(entry: TypeInfo, field_spec: dict[str, Any]) -> Optional[FieldInfo]:
    if "name" in field_spec:
        for field in entry.fields:
            if field.name == field_spec["name"]:
                return field
        return None
    # No name: select by exact offset (which is then trivially satisfied as a
    # metadata bound, but stays useful as documentation in the .toml).
    for field in entry.fields:
        if field.offset == field_spec.get("offset"):
            return field
    return None


def _check_bound(label: str, actual: int, bound: Bound, errors: list[str]) -> None:
    if isinstance(bound, int):
        bound = {"eq": bound}
    if "eq" in bound and actual != bound["eq"]:
        errors.append(f"{label}: expected {bound['eq']}, got {actual}")
    if "min" in bound and actual < bound["min"]:
        errors.append(f"{label}: expected >= {bound['min']}, got {actual}")
    if "max" in bound and actual > bound["max"]:
        errors.append(f"{label}: expected <= {bound['max']}, got {actual}")


# ----------------------------------------------------------------------
# Universal invariants
# ----------------------------------------------------------------------


def check_invariants(profile: Profile) -> list[str]:
    """Structural checks that hold for any well-formed profile."""
    errors: list[str] = []
    _check_frame_table(profile, errors)
    _check_type_table(profile, errors)
    _check_events(profile, errors)
    _check_alloc_free_pairing(profile, errors)
    return errors


def _check_frame_table(profile: Profile, errors: list[str]) -> None:
    ft: dict[str, list[int]] = profile.raw["frame_table"]
    n_pc = len(ft["pc"])
    n_str = len(profile.strtab)

    for key in ("object_path", "object_address", "object_symbol"):
        if len(ft[key]) != n_pc:
            errors.append(f"frame_table.{key}: length {len(ft[key])} != pc count {n_pc}")
    if len(ft["offsets"]) != n_pc + 1:
        errors.append(f"frame_table.offsets: length {len(ft['offsets'])} != pc count + 1")
    elif _non_monotonic(ft["offsets"]):
        errors.append("frame_table.offsets: not monotonically non-decreasing")
    else:
        n_frames = ft["offsets"][-1]
        for key in ("file", "func", "line", "column", "is_inline"):
            if len(ft[key]) != n_frames:
                errors.append(f"frame_table.{key}: length {len(ft[key])} != frame count {n_frames}")
    for key in ("object_path", "object_symbol", "file", "func"):
        bad = [i for i in ft[key] if not 0 <= i < n_str]
        if bad:
            errors.append(f"frame_table.{key}: string index out of range (e.g. {bad[0]})")


def _check_type_table(profile: Profile, errors: list[str]) -> None:
    for t in profile.types:
        prefix = f"type_data_table[{t.index}] ({t.name!r})"
        if t.size <= 0:
            errors.append(f"{prefix}: non-positive size {t.size}")
        for f in t.fields:
            if f.offset + f.size > t.size:
                errors.append(
                    f"{prefix}: field {f.name!r} [{f.offset}, {f.offset + f.size})"
                    f" exceeds type size {t.size}"
                )
        for b in t.bases:
            if b.offset + b.size > t.size:
                errors.append(
                    f"{prefix}: base {b.type!r} [{b.offset}, {b.offset + b.size})"
                    f" exceeds type size {t.size}"
                )


def _check_events(profile: Profile, errors: list[str]) -> None:
    n_pc = len(profile.raw["frame_table"]["pc"])
    n_types = len(profile.types)
    seen_ids: set[int] = set()
    prev_id = -1

    for event in profile.events:
        eid: int = event["id"]
        prefix = f"event {eid}"
        if eid in seen_ids:
            errors.append(f"{prefix}: duplicate id")
        seen_ids.add(eid)
        if eid < prev_id:
            errors.append(f"{prefix}: events not sorted by id")
        prev_id = eid

        if any(not 0 <= pc < n_pc for pc in event["pc_id"]):
            errors.append(f"{prefix}: pc_id out of range")

        oi: Optional[dict[str, list[int]]] = event.get("object_info")
        if oi is None:
            continue
        lengths = {key: len(values) for key, values in oi.items()}
        if len(set(lengths.values())) > 1:
            errors.append(f"{prefix}: object_info arrays have differing lengths {lengths}")
            continue
        if _non_monotonic(oi["trace_index"], strict=True):
            errors.append(f"{prefix}: object_info.trace_index not strictly increasing")
        for k in range(len(oi["trace_index"])):
            if not 0 <= oi["type_data"][k] < n_types:
                errors.append(f"{prefix}: object_info.type_data out of range")
                continue
            entry = profile.types[oi["type_data"][k]]
            name = profile.strtab[oi["type"][k]]
            if name != entry.name:
                errors.append(
                    f"{prefix}: object type {name!r} disagrees with type table entry {entry.name!r}"
                )
            if oi["size"][k] != entry.size:
                errors.append(
                    f"{prefix}: object size {oi['size'][k]} disagrees with"
                    f" type table size {entry.size} for {entry.name!r}"
                )


def _check_alloc_free_pairing(profile: Profile, errors: list[str]) -> None:
    """Track the live-allocation set through the event stream.

    Frees of unknown addresses are allowed (allocations made before tracing or
    while recording was suppressed), but double-frees and re-allocations of a
    live address are violations, and a FREE's filled-in size must match its
    allocation.
    """
    live: dict[int, int] = {}
    for event in profile.events:
        addr: int = event["alloc_addr"]
        if addr == 0:
            continue
        if event["type"] in ("ALLOC", "REALLOC"):
            hint: int = event["alloc_hint"]
            if hint:
                live.pop(hint, None)
            if addr in live:
                errors.append(f"event {event['id']}: ALLOC at live address {addr:#x}")
            live[addr] = event["alloc_size"]
        elif event["type"] == "FREE":
            if addr not in live:
                continue
            expected = live.pop(addr)
            if event["alloc_size"] not in (0, expected):
                errors.append(
                    f"event {event['id']}: FREE size {event['alloc_size']}"
                    f" != allocation size {expected} for {addr:#x}"
                )


def _non_monotonic(values: list[int], strict: bool = False) -> bool:
    if strict:
        return any(b <= a for a, b in zip(values, values[1:]))
    return any(b < a for a, b in zip(values, values[1:]))
