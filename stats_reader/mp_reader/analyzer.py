"""
Analysis and processing utilities for memory profiler data.
"""

import pprint
from .malloc_stats import OutputRecord, ObjectTree, ObjectEnt, Loc
import typing
from .tree import Tree, print_tree
from .color import *
from itertools import starmap


def _loc_lines(loc: Loc) -> list[str]:
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
    record: OutputRecord, event_id: int, file: typing.IO | None = None,
    skip_inline = True,
    show_bin_addr = False
):
    event = record.event_table[event_id]
    objects = event.expand_objects(record)

    frames = []

    frame_table = record.frame_table
    for pid, obj in zip(event.pc_id, objects):
        pc = frame_table.pc[pid]
        object_path = record.get_object_path(pid)
        object_addr = record.get_object_address(pid)

        object_str = f"{object_path}+0x{object_addr:x}"

        locs: list[Loc] = record.get_locs(pid)

        if skip_inline:
            locs = [loc for loc in locs if not loc.is_inline]

        ent = [_loc_lines(loc) for loc in locs]

        last_ent = ent[-1]

        if obj is not None:
            last_ent.append(
                f"OBJECT this={bb_yellow(ptr(obj.addr))} size={bb_green(obj.size)} type={bb_magenta(obj.type)}",
            )

        if show_bin_addr:
            last_ent.append(f"{bb_yellow(object_str)}")

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
        children = []
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
