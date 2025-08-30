"""
Analysis and processing utilities for memory profiler data.
"""

import pprint
from .malloc_stats import OutputRecord, ObjectTree, ObjectEnt, Loc, EventType
import typing
from typing import Annotated
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
    skip_inline=True,
    show_bin_addr=True,
):
    event = record.event_table[event_id]
    objects = event.expand_objects(record)

    frames = []

    frame_table = record.frame_table
    for pid, obj in zip(event.pc_id, objects):
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

    t = Tree(
        [
            f'{bold_white("id")}: {bb_green(event.id)}',
            f'{bold_white("type")}: {bb_green(event.type.value)}',
            f'{bold_white("addr")}: {bb_green(event.alloc_addr):x}',
            f'{bold_white("size")}: {bb_green(event.alloc_size)}',
        ],
        frames,
    )
    print_tree(t, file=file)


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
    count_tag: str = 'objects',
    count_tag_singular: str = 'object',
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
        int | None, typer.Option(help="Only show entries that take at least this many bytes")
    ] = None,
    exclude_self: Annotated[
        bool, typer.Option(help='Typically the size of an object is given by (dynamic allocations) + (sizeof(object) * num_objects). If --exclude-self is passed, only dynamic allocations are counted.')
    ] = False,
) -> None:
    """
    Print allocation statistics by type, sorted by total bytes allocated.

    Analyzes all FREE events in the memory profiler data and shows which types
    are responsible for the most memory allocations.
    """
    from .loader import load_from_file

    record = load_from_file(input_file)

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

            for type_data_idx, object_id in zip(object_info.type_data, object_info.object_id):
                entry= counts[type_data_idx]
                entry.byte_count += event.alloc_size
                entry.obj_ids.add(object_id)
        else:
            # Untyped allocation
            untyped_allocations_count += 1
            untyped_allocations += event.alloc_size

    if not exclude_self:
        for k, e in counts.items():
            e.byte_count += record.get_type_size(k) * e.num_objects()

    total_object_count = sum(
        len(e.obj_ids) for e in counts.values()
    )

    # Convert to (type_name, total_bytes) and sort by allocation size (descending)
    if min_bytes is None:
        type_items = [
            (record.get_type_name(idx), e.byte_count, e.num_objects())
            for idx, e in counts.items()
        ]
    else:
        type_items = [
            (record.get_type_name(idx), e.byte_count, e.num_objects())
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
    for type_name, total_bytes, object_count in sorted_types:
        _print_alloc_stat(type_name, object_count, total_bytes, max_tag_len=max_typename_len)

    if len(type_items) < len(counts):
        num_filtered = len(counts) - len(type_items)
        print(grey(f"                                 ..."))
        print(grey(f"{num_filtered:>19} entries filtered"))
        print()

    # Print untyped allocations if any
    if untyped_allocations > 0:
        _print_alloc_stat("<untyped>", untyped_allocations_count, untyped_allocations, count_tag='allocs', count_tag_singular='alloc', tag_style=BB_Y)

    # Print totals - sum of all FREE event alloc_sizes
    total_all_frees = sum(
        event.alloc_size for event in record.event_table if event.type == EventType.FREE
    )

    print()
    _print_alloc_stat("<total>", total_object_count, total_all_frees, tag_style=BB_W)
