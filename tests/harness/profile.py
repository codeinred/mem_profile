"""Load a malloc_stats.json profile and derive attribution metrics.

Attribution model (see tests/CASES.md for the full description):

For every FREE event the runtime records the chain of instrumented destructor
frames that were live on the stack, innermost first. From those chains we
compute:

- owned_bytes[type]        bytes freed while `type`'s destructor was live
                           (each FREE counted once per distinct type in chain)
- self_bytes[type]         bytes whose *innermost* destructor frame was `type`
- objects[type]            distinct destructor invocations (object ids) seen
- field_bytes[(type, field_offset)]
                           bytes credited to a field of `type`: for adjacent
                           chain entries (child, parent), `child.addr -
                           parent.addr` locates the owning field in parent's
                           layout
- base_bytes[(type, base_type)]
                           same, when the offset locates a base subobject
- typed_free_bytes / untyped_free_bytes
                           freed bytes with a non-empty / empty chain
"""

from __future__ import annotations

import fnmatch
import json
from collections import defaultdict
from dataclasses import dataclass
from typing import Any, Literal, Optional


class Profile:
    """A parsed profiler output file plus derived attribution metrics."""

    strtab: list[str]
    types: list[TypeInfo]
    events: list[dict[str, Any]]

    owned_bytes: dict[str, int]
    self_bytes: dict[str, int]
    objects: dict[str, set[int]]
    field_bytes: dict[tuple[str, int], int]
    base_bytes: dict[tuple[str, str], int]
    typed_free_bytes: int
    untyped_free_bytes: int

    def __init__(self, path: str) -> None:
        with open(path) as f:
            self.raw: dict[str, Any] = json.load(f)
        self.strtab = self.raw["strtab"]
        self.types = self._load_types()
        self.events = self.raw["event_table"]
        self._attribute()

    # ------------------------------------------------------------------
    # Public queries
    # ------------------------------------------------------------------

    def type_entries(self, pattern: str) -> list[TypeInfo]:
        """All type-table entries whose name matches the glob `pattern`."""
        return [t for t in self.types if fnmatch.fnmatchcase(t.name, pattern)]

    def matching_names(self, pattern: str) -> set[str]:
        """Distinct type names matching the glob `pattern`."""
        return {t.name for t in self.type_entries(pattern)}

    # ------------------------------------------------------------------
    # Loading
    # ------------------------------------------------------------------

    def _load_types(self) -> list[TypeInfo]:
        tt: dict[str, list[int]] = self.raw["type_data_table"]
        st = self.strtab
        types: list[TypeInfo] = []
        for i in range(len(tt["size"])):
            fields = [
                FieldInfo(
                    name=st[tt["field_names"][j]],
                    type=st[tt["field_types"][j]],
                    size=tt["field_sizes"][j],
                    offset=tt["field_offsets"][j],
                )
                for j in range(tt["field_off"][i], tt["field_off"][i + 1])
            ]
            bases = [
                BaseInfo(
                    type=st[tt["base_types"][j]],
                    size=tt["base_sizes"][j],
                    offset=tt["base_offsets"][j],
                )
                for j in range(tt["base_off"][i], tt["base_off"][i + 1])
            ]
            types.append(TypeInfo(i, st[tt["type"][i]], tt["size"][i], fields, bases))
        return types

    # ------------------------------------------------------------------
    # Attribution
    # ------------------------------------------------------------------

    def _attribute(self) -> None:
        self.owned_bytes = defaultdict(int)
        self.self_bytes = defaultdict(int)
        self.objects = defaultdict(set)
        self.field_bytes = defaultdict(int)
        self.base_bytes = defaultdict(int)
        self.typed_free_bytes = 0
        self.untyped_free_bytes = 0

        for event in self.events:
            if event["type"] == "FREE":
                self._attribute_free(event)

    def _attribute_free(self, event: dict[str, Any]) -> None:
        size: int = event["alloc_size"]
        oi: Optional[dict[str, list[int]]] = event.get("object_info")
        if not oi or not oi["trace_index"]:
            self.untyped_free_bytes += size
            return
        self.typed_free_bytes += size

        chain = self._chain(oi)
        seen: set[str] = set()
        for entry in chain:
            self.objects[entry.type_name].add(entry.object_id)
            if entry.type_name not in seen:
                seen.add(entry.type_name)
                self.owned_bytes[entry.type_name] += size
        self.self_bytes[chain[0].type_name] += size

        for child, parent in zip(chain, chain[1:]):
            parent_type = self.types[parent.type_data]
            located = self._locate(parent_type, child.addr - parent.addr, child.type_name)
            if located is None:
                continue
            kind, key = located
            if kind == "field":
                assert isinstance(key, int)
                self.field_bytes[(parent_type.name, key)] += size
            else:
                assert isinstance(key, str)
                self.base_bytes[(parent_type.name, key)] += size

    def _chain(self, oi: dict[str, list[int]]) -> list[ChainEntry]:
        st = self.strtab
        return [
            ChainEntry(
                trace_index=oi["trace_index"][k],
                object_id=oi["object_id"][k],
                addr=oi["addr"][k],
                size=oi["size"][k],
                type_name=st[oi["type"][k]],
                type_data=oi["type_data"][k],
            )
            for k in range(len(oi["trace_index"]))
        ]

    @staticmethod
    def _locate(
        parent: TypeInfo, offset: int, child_type: str
    ) -> Optional[tuple[Literal["field", "base"], int | str]]:
        """Locate `offset` within parent's layout.

        An exact-offset base of the child's own type wins (so a base subobject
        at offset 0 is not mistaken for a first field at offset 0); otherwise
        the containing field, then the containing base.
        """
        for b in parent.bases:
            if b.offset == offset and b.type == child_type:
                return ("base", b.type)
        for f in parent.fields:
            if f.offset <= offset < f.offset + max(f.size, 1):
                return ("field", f.offset)
        for b in parent.bases:
            if b.offset <= offset < b.offset + max(b.size, 1):
                return ("base", b.type)
        return None


@dataclass
class TypeInfo:
    """One entry of the type metadata table."""

    index: int
    name: str
    size: int
    fields: list[FieldInfo]
    bases: list[BaseInfo]


@dataclass
class FieldInfo:
    name: str
    type: str
    size: int
    offset: int


@dataclass
class BaseInfo:
    type: str
    size: int
    offset: int


@dataclass
class ChainEntry:
    """One destructor frame in a FREE event's object chain (innermost first)."""

    trace_index: int
    object_id: int
    addr: int
    size: int
    type_name: str
    type_data: int
