"""
Analysis and processing utilities for memory profiler data.
"""

import pprint
from .malloc_stats import (
    OutputRecord,
    ObjectTree,
    ObjectEnt,
    OutputEvent,
    OutputObjectInfo,
    EventType,
    Loc,
    EventType,
    size_t,
    offset_t,
    type_index_t,
)
import typing
from typing import Annotated, Literal
from pathlib import Path
from .tree import Tree, print_tree
from .color import *
from itertools import starmap
from collections import defaultdict
import typer
from dataclasses import dataclass, field


def _loc_lines(loc: Loc) -> list[str | Styled]:
    s = f"{bb_green(loc.file)}"
    if loc.col != 0:
        s += f":{bb_blue(loc.line)}:{bb_blue(loc.col)}"
    elif loc.line != 0:
        s += f":{bb_blue(loc.line)}"

    if loc.is_inline:
        s += " (inlined)"
    if loc.func:
        return [s, bb_cyan(loc.func)]
    return [s]


def ptr(x: int) -> str:
    return f"0x{x:x}"


def print_event_trace(
    record: OutputRecord,
    event_id: int,
    file: typing.IO | None = None,
    skip_frames: int = 0,
    skip_inline: bool = True,
    show_bin_addr: bool = True,
    show_event_type: bool = True,
    show_event_id: bool = True,
    show_event_addr: bool = True,
    show_event_size: bool = True,
):
    event = record.event_table[event_id]
    objects = event.expand_objects(record)

    frames = []

    frame_table = record.frame_table
    for pid, obj in zip(event.pc_id[skip_frames:], objects[skip_frames:]):
        pc = frame_table.pc[pid]
        object_path = record.get_object_path(pid)
        object_addr = record.get_object_address(pid)
        object_sym = record.get_object_symbol(pid)

        object_str = f"{object_path}+0x{object_addr:x}"

        locs: list[Loc] = record.get_locs(pid)

        if skip_inline:
            locs = [loc for loc in locs if not loc.is_inline]

        ent = [_loc_lines(loc) for loc in locs]

        last_ent = ent[-1]

        if obj is not None:
            s = f"OBJECT this={bb_yellow(ptr(obj.addr))}"
            if obj.offset is not None:
                s += f" offset={bb_blue(obj.offset)}"
            s += f" size={bb_green(obj.size)} type={bb_magenta(obj.type)}"
            last_ent.append(s)

        if show_bin_addr:
            last_ent.append(f"{bb_yellow(object_str)}")
            last_ent.append(f"{grey(object_sym)}")

        frames.extend(ent)

    header = []
    if show_event_id:
        header.append(f'{bold_white("id")}: {bb_green(event.id)}')
    if show_event_type:
        header.append(f'{bold_white("type")}: {bb_green(event.type.value)}')
    if show_event_addr:
        header.append(f'{bold_white("addr")}: {bb_green(event.alloc_addr):x}')
    if show_event_size:
        header.append(f'{bold_white("size")}: {bb_green(event.alloc_size)}')
    if len(header) == 0:
        header = ["<event>"]
    print_tree(Tree(header, frames), file=file)


def dump_events(
    input_file: Annotated[Path, typer.Argument(help="Path to malloc_stats.json file")],
    skip_inline: Annotated[
        bool, typer.Option(help="Filter out inline frames from the trace")
    ] = False,
    show_bin_addr: Annotated[
        bool, typer.Option(help="Show addresses within the executable")
    ] = False,
    event_type: Annotated[
        EventType | None, typer.Option(help="Filter by event type")
    ] = None,
    skip_frames: Annotated[
        int, typer.Option(help="Skip the top N frames at the start of the trace")
    ] = 0,
    show_event_type: Annotated[
        bool, typer.Option(help="show event type in trace")
    ] = True,
    show_event_id: Annotated[bool, typer.Option(help="show event id in trace")] = True,
    show_event_addr: Annotated[
        bool, typer.Option(help="show event addr in trace")
    ] = True,
    show_event_size: Annotated[
        bool, typer.Option(help="show event size in trace")
    ] = True,
) -> None:
    from .loader import load_from_file

    record: OutputRecord = load_from_file(input_file)

    for i in range(len(record.event_table)):
        current_event_type = record.event_table[i].type
        if event_type is None or event_type == current_event_type:
            print_event_trace(
                record,
                i,
                skip_inline=skip_inline,
                show_bin_addr=show_bin_addr,
                skip_frames=skip_frames,
                show_event_type=show_event_type,
                show_event_id=show_event_id,
                show_event_addr=show_event_addr,
                show_event_size=show_event_size,
            )


def get_objects(record: OutputRecord) -> dict[int, ObjectTree]:
    """
    Extract object tree information from memory profiler data.

    Args:
        record: The memory profiler data

    Returns:
        Dictionary mapping object IDs to ObjectTree instances
    """
    objects: dict[int, ObjectTree] = {}

    for event in record.event_table:
        object_info = event.object_info
        children: list[ObjectTree] = []
        if object_info is not None:
            children = []
            for object_id, addr, size, trace_index, type in zip(
                object_info.object_id,
                object_info.addr,
                object_info.size,
                object_info.trace_index,
                object_info.type,
            ):
                obj: ObjectTree

                if object_id not in objects:
                    location = record.get_loc(event.pc_id[trace_index])

                    obj = ObjectTree(
                        record.strtab[type],
                        location,
                        object_id,
                        size,
                        event.alloc_size,
                        children,
                    )
                    objects[object_id] = obj
                else:
                    obj = objects[object_id]
                    obj.allocated_bytes += event.alloc_size
                    obj.children.extend(children)
                children = [obj]
    return objects


def print_objects(record: OutputRecord) -> None:
    """
    Print object information from an OutputRecord, sorted by direct size.

    Args:
        record: The memory profiler data to analyze
    """
    objects = get_objects(record)
    object_list = list(objects.values())
    object_list.sort(key=lambda x: x.direct_size, reverse=True)

    for obj in object_list:
        pprint.pprint(obj, compact=True)


def _print_alloc_stat(
    tag: str,
    count: int,
    total_bytes: int,
    count_tag: str = "objects",
    count_tag_singular: str = "object",
    max_tag_len: int | None = None,
    count_style: str = Grey,
    byte_style: str = BB_G,
    tag_style: str = BB_C,
):
    if max_tag_len is not None:
        if len(tag) > max_tag_len:
            tag = tag[:max_tag_len]
            tag += "..."

    if count == 1:
        count_tag = count_tag_singular
    print(
        f"{st(count_style, f'{count:>8,} {count_tag:8}')} {st(byte_style, f'{total_bytes:>12,} bytes')}  {st(tag_style, tag)}"
    )


@dataclass(slots=True)
class AllocCount:
    alloc_count: int = 0
    alloc_bytes: int = 0

    def add(self, alloc_bytes: int):
        self.alloc_count += 1
        self.alloc_bytes += alloc_bytes


@dataclass(slots=True, order=True)
class Base:
    offset: offset_t
    size: size_t
    type_name: str


@dataclass(slots=True, order=True)
class Field:
    offset: offset_t
    size: size_t
    type_name: str
    field_name: str | None


@dataclass(slots=True, order=True)
class ChildAllocStats:
    offset: offset_t
    size: size_t
    type_name: str
    tid: type_index_t
    alloc_count: AllocCount


def get_stats_for_type(
    tid: int,
    record: OutputRecord,
    select_events: typing.Iterable[int] | None = None,
    show_offsets: bool = False,
):
    if select_events is None:
        select_events = range(len(record.event_table))

    # events: list[OutputEvent] = [record.event_table[e] for e in select_events]

    # Get all free events that contain the given type
    events: list[OutputEvent] = [
        e
        for e in map(record.event_table.__getitem__, select_events)
        if (
            e.type == EventType.FREE
            and e.object_info is not None
            and tid in e.object_info.type_data
        )
    ]

    type_size = record.get_type_size(tid)

    object_ids: set[int] = set()

    alloc_count = len(events)
    alloc_bytes = sum(e.alloc_size for e in events)

    direct_alloc_count = 0
    direct_alloc_bytes = 0

    child_data: dict[tuple[offset_t, type_index_t], AllocCount] = defaultdict(
        AllocCount
    )
    indirect_child_data: dict[type_index_t, AllocCount] = defaultdict(AllocCount)

    for e in events:
        # if 't_lookup' in record.get_type_name(tid):
        #     print_event_trace(record, e.id, show_bin_addr=True, skip_inline=False)
        object_info = e.object_info.reverse()

        # Find the index of the type within the object.
        # We can assume that `tid` must be in the type_data, because that was
        # one of the conditions when we created the list of events up above.
        i = object_info.type_data.index(tid)

        object_ids.add(object_info.object_id[i])

        is_leaf = object_info.depth() == i + 1

        # If it's a leaf, there are no child objects
        if is_leaf:
            direct_alloc_count += 1
            direct_alloc_bytes += e.alloc_size
            continue

        this_ptr = object_info.addr[i]

        child_ptr = object_info.addr[i + 1]
        child_tid = object_info.type_data[i + 1]

        # Offset of child within parent
        offset = child_ptr - this_ptr

        is_member_or_base: bool = offset in range(type_size)

        if is_member_or_base:
            child_data[(offset, child_tid)].add(e.alloc_size)
        else:
            indirect_child_data[child_tid].add(e.alloc_size)

    total_alloc_count = len(events)
    total_allocated_bytes = sum(e.alloc_size for e in events)

    type_data = record.get_type_data_at(tid)

    results: list[Field | Base | ChildAllocStats] = []

    for offset, size, type_name in zip(
        type_data.base_offsets, type_data.base_sizes, type_data.base_types
    ):
        results.append(
            Base(
                offset,
                size,
                type_name,
            )
        )

    for offset, size, type_name, field_name in zip(
        type_data.field_offsets,
        type_data.field_sizes,
        type_data.field_types,
        type_data.field_names,
    ):
        results.append(
            Field(
                offset,
                size,
                type_name,
                field_name,
            )
        )

    for (offset, tid), allocs in child_data.items():
        results.append(
            ChildAllocStats(
                offset,
                record.get_type_size(tid),
                record.get_type_name(tid),
                tid,
                allocs,
            )
        )

    def _key(x: Field | Base | ChildAllocStats):
        match x:
            case Base():
                return (x.offset, 0, x.size)
            case Field():
                return (x.offset, 1, x.size)
            case ChildAllocStats():
                return (x.offset, 2, x.size)

    max_field_type_len = 0
    max_base_type_len = 0
    for e in results:
        match e:
            case Field():
                max_field_type_len = max(len(e.type_name), max_field_type_len)
            case Base():
                max_base_type_len = max(len(e.type_name), max_base_type_len)
            case _:
                continue

    results.sort(key=_key)

    base_prefix = ": "
    needs_open_bracket = True
    needs_newline = False
    last_offset = 0
    last_child_size = 0
    last_child_str: str = ""
    last_print_was_stats = False

    print(f"{Grey}// Totals for {type_data.name}{RE}")
    print(
        f"{Grey}// └── {BB_G}{total_allocated_bytes:,} bytes{RE}{Grey} across {BB_B}{total_alloc_count} allocs{RE}"
    )
    print(f"{bb_yellow('struct')} {bb_cyan(type_data.name)}")

    for e in results:
        match e:
            case Base():
                last_offset = e.offset
                last_child_size = e.size
                if needs_newline:
                    needs_newline = False
                    print()
                start, end = e.offset, e.offset + e.size
                _range = f"bytes {start:<4}..{end:<4} in object"
                last_child_str = (
                    f"  {base_prefix}{bb_cyan(e.type_name):<{max_base_type_len}}"
                )
                print(last_child_str, end="")
                last_print_was_stats = False
                needs_newline = True
                base_prefix = ", "
            case Field():
                last_offset = e.offset
                last_child_size = e.size
                if needs_open_bracket:
                    if needs_newline:
                        print(" {")
                    else:
                        print("{")
                    needs_open_bracket = False
                    needs_newline = False
                if needs_newline:
                    print()
                    needs_newline = False
                start, end = e.offset, e.offset + e.size
                tag = e.field_name
                if tag == "":
                    tag = "(unnamed)"
                last_child_str = (
                    f"  {bb_cyan(e.type_name):<{max_field_type_len}} {bb_green(tag)};"
                )
                print(last_child_str, end="")
                last_print_was_stats = False
                needs_newline = True
            case ChildAllocStats():
                # Ensure that this stat has the same indent of the previous one
                if last_print_was_stats:
                    print(" " * len_without_color(last_child_str), end="")
                inner_offset_str = ""
                if e.size < last_child_size:
                    # We're inside a c-style array or we were allocated in a buffer...
                    inner_offset = e.offset - last_offset
                    if inner_offset % e.size == 0:
                        array_index = inner_offset // e.size
                        inner_offset_str = f"[{array_index}] "
                    else:
                        inner_offset_str = f"+{inner_offset} "
                alloc_bytes = f"{e.alloc_count.alloc_bytes:,}" + " bytes"
                alloc_count = f"{e.alloc_count.alloc_count:,}" + " allocs"
                start, end = e.offset, e.offset + e.size
                if show_offsets:
                    _range = f"{start}..{end:>3}"
                    _range = f"{bb_yellow(_range):>8}  "
                else:
                    _range = ""
                print(
                    f" {Grey}// {inner_offset_str}{_range}{RE}{bb_green(alloc_bytes)}{Grey} across {RE}{bb_blue(alloc_count)}{Grey} : {e.type_name}{RE}"
                )
                last_print_was_stats = True
                needs_newline = False
    if needs_open_bracket:
        print("{")
    if needs_newline:
        print()
        needs_newline = False

    has_other_allocs = direct_alloc_count > 0 or len(indirect_child_data) > 0

    if has_other_allocs:
        print()
        print(f"  {BB_C}~{type_data.name}();{RE}")

        if direct_alloc_count > 0:
            alloc_bytes = f"{direct_alloc_bytes:,} bytes"
            alloc_count = f"{direct_alloc_count:,} allocs"
            print(f"  {Grey}// directly owned (or unannotated):")
            print(
                f"  {Grey}// └── {BB_G}{alloc_bytes}{RE}{Grey} across {BB_B}{alloc_count}{RE}"
            )
        if len(indirect_child_data) > 0:
            print(f"  {Grey}// children on heap:")
            items: list[tuple[type_index_t, AllocCount]] = sorted(
                list(indirect_child_data.items()),
                key=lambda ent: record.get_type_name(ent[0]),
            )
            last_i = len(items) - 1
            for i, (tid, alloc_count) in enumerate(items):
                alloc_bytes = f"{alloc_count.alloc_bytes:,}" + " bytes"
                alloc_count = f"{alloc_count.alloc_count:,}" + " allocs"
                prefix = "├── " if i < last_i else "└── "
                print(
                    f"  {Grey}// {BOLD}{prefix}{bb_green(alloc_bytes)}{Grey} across {bb_blue(alloc_count)}{Grey} : {record.get_type_name(tid)}"
                )

    print("};")
    print()


@dataclass
class _counts:
    obj_ids: set[int] = field(default_factory=set)
    byte_count: int = 0

    def num_objects(self) -> int:
        return len(self.obj_ids)


def stats(
    input_file: Annotated[Path, typer.Argument(help="Path to malloc_stats.json file")],
    count: Annotated[
        int | None, typer.Option(help="Limit output to top N entries")
    ] = None,
    max_typename_len: Annotated[
        int | None, typer.Option(help="Maximum length for type names")
    ] = None,
    min_bytes: Annotated[
        int | None,
        typer.Option(help="Only show entries that take at least this many bytes"),
    ] = None,
    include_self: Annotated[
        bool,
        typer.Option(
            help="Typically only dynamic allocations for an object will be reported. If --inlude-self is passed, then the size of the object itself will be included."
        ),
    ] = False,
    top_n_layouts: Annotated[
        int, typer.Option(help="Print the layout stats for the top n largest types")
    ] = 0,
    show_offsets: Annotated[
        bool, typer.Option(help="Show offsets when printing allocations in members and bases")
    ] = False,
) -> None:
    """
    Print allocation statistics by type, sorted by total bytes allocated.

    Analyzes all FREE events in the memory profiler data and shows which types
    are responsible for the most memory allocations.
    """
    from .loader import load_from_file

    record: OutputRecord = load_from_file(input_file)

    record.clean()

    # Dictionary to track total bytes allocated by type_data index
    counts: dict[int, _counts] = defaultdict(_counts)

    untyped_allocations = 0
    untyped_allocations_count = 0

    # Process all FREE events
    for event in record.event_table:
        if event.type != EventType.FREE:
            continue

        object_info = event.object_info
        if object_info is not None:
            # Process typed allocations from object info
            # Each type gets attributed the full event.alloc_size

            for tid, object_id in zip(object_info.type_data, object_info.object_id):
                counts[tid].obj_ids.add(object_id)

            # Constructing a set from the types in the trace ensures that
            # we don't double-count allocations for recursive data structures
            for tid in set(object_info.type_data):
                counts[tid].byte_count += event.alloc_size

        else:
            # Untyped allocation
            untyped_allocations_count += 1
            untyped_allocations += event.alloc_size

    if include_self:
        for k, e in counts.items():
            e.byte_count += record.get_type_size(k) * e.num_objects()

    total_object_count = sum(len(e.obj_ids) for e in counts.values())

    # Convert to (type_name, total_bytes) and sort by allocation size (descending)
    if min_bytes is None:
        type_items = [
            (record.get_type_name(idx), e.byte_count, e.num_objects(), idx)
            for idx, e in counts.items()
        ]
    else:
        type_items = [
            (record.get_type_name(idx), e.byte_count, e.num_objects(), idx)
            for idx, e in counts.items()
            if e.byte_count >= min_bytes
        ]

    sorted_types = sorted(type_items, key=lambda x: x[1], reverse=True)

    # Apply count limit if specified
    if count is not None:
        sorted_types = sorted_types[:count]

    # Print results
    print(f"{bold_white('Allocation Statistics by Type:')}\n")

    # Print typed allocations
    for type_name, total_bytes, object_count, _ in sorted_types:
        _print_alloc_stat(
            type_name, object_count, total_bytes, max_tag_len=max_typename_len
        )

    if len(type_items) < len(counts):
        num_filtered = len(counts) - len(type_items)
        print(grey(f"                                 ..."))
        print(grey(f"{num_filtered:>19} entries filtered"))
        print()

    # Print untyped allocations if any
    if untyped_allocations > 0:
        _print_alloc_stat(
            "<untyped>",
            untyped_allocations_count,
            untyped_allocations,
            count_tag="allocs",
            count_tag_singular="alloc",
            tag_style=BB_Y,
        )

    # Print totals - sum of all FREE event alloc_sizes
    total_all_frees = sum(
        event.alloc_size for event in record.event_table if event.type == EventType.FREE
    )

    print()
    _print_alloc_stat("<total>", total_object_count, total_all_frees, tag_style=BB_W)

    if top_n_layouts > 0:
        print()
        entries = sorted_types[:top_n_layouts]
        for _, _, _, tid in entries:
            get_stats_for_type(tid, record, show_offsets=show_offsets)
