"""
Python dataclasses for memory profiler output_record structure.

Replicates the C++ structures defined in mem_profile/output_record.h
"""

from dataclasses import dataclass
from enum import Enum
import typing
import numpy as np
import numpy.typing as npt
import copy
from collections import defaultdict

# Type aliases matching C++ types
addr_t = int  # uintptr_t
str_index_t = int  # size_t

u64 = int

u32 = int

u8 = int

size_t = int

type_index_t = int
"""Index into type data table"""

offset_t = int
"""Pointer offset - difference of subtracting two pointers"""


class EventType(Enum):
    """Event type enum matching C++ event_type"""

    FREE = "FREE"
    ALLOC = "ALLOC"
    REALLOC = "REALLOC"


@dataclass
class Loc:
    func: str
    file: str
    line: int
    col: int
    is_inline: bool


@dataclass
class ObjectEnt:
    """Represents a specific object, with fields expanded/dereferenced"""

    object_id: u64
    """Unique ID of the object. This is unique over the lifetime of the program,
    even if the address is not."""

    addr: addr_t
    """Address of the object"""

    size: size_t
    """Size of this object"""

    offset: addr_t | None
    """Offset of this object, within it's parent"""

    type: str
    """Type of the object, expressed as a string"""

    def mem_range(self) -> range:
        return range(self.addr, self.addr + self.size)


@dataclass
class OutputObjectInfo:
    """
    Information about objects being destroyed during a FREE event.

    This is a sparse representation - only stack frames that involve
    destructor calls are recorded via trace_index.
    """

    trace_index: list[size_t]
    """Indices into event.pc_id array - which stack frames have destructors"""

    object_id: list[u64]
    """Unique object identifiers (lifetime-unique)"""

    addr: list[addr_t]
    """Object addresses (this pointers at destruction)"""

    size: list[size_t]
    """Object sizes in bytes"""

    type: list[str_index_t]
    """Type name indices into string table"""

    type_data: list[type_index_t]
    """Index into type data table for each entry"""

    def depth(self) -> int:
        return len(self.type_data)

    def reverse(self) -> "OutputObjectInfo":
        """Return a new OutputObjectInfo which reverses the order of all entries
        in the OutputObjectInfo.

        This puts the 'largest' or 'most senior' object first."""
        return OutputObjectInfo(
            list(reversed(self.trace_index)),
            list(reversed(self.object_id)),
            list(reversed(self.addr)),
            list(reversed(self.size)),
            list(reversed(self.type)),
            list(reversed(self.type_data)),
        )

    def clean(self, selected: list[int]):
        self.trace_index = [self.trace_index[i] for i in selected]
        self.object_id = [self.object_id[i] for i in selected]
        self.addr = [self.addr[i] for i in selected]
        self.size = [self.size[i] for i in selected]
        self.type = [self.type[i] for i in selected]
        self.type_data = [self.type_data[i] for i in selected]


@dataclass
class OutputTypeData:
    """
    Type data table for the given OutputRecord.

    Holds information about object types, sizes, fields, and offsets
    """

    size: list[size_t]
    """size[i] is the size of the i-th type entry"""

    type: list[str_index_t]
    """type[i] is the name of the i-th type entry, as a string"""

    field_off: list[size_t]
    """`slice(field_off[i], field_off[i+1])` corresponts to the fields for the i-th type entry"""

    field_names: list[str_index_t]
    """Field name table. Field names for the i-th type entry are given by `field_names[field_slice(i)]`"""

    field_types: list[str_index_t]
    """Field type table. Field typenames for the i-th type entry are given by `field_types[field_slice(i)]`"""

    field_sizes: list[size_t]
    """Field size table. Field sizes for the i-th type entry are given by `field_sizes[field_slice(i)]`"""

    field_offsets: list[size_t]
    """Field offsets table. Field offsets (relative to the start of the object, in memory) for the i-th type entry are given by `field_offsets[field_slice(i)]`"""

    base_off: list[size_t]
    """`slice(base_off[i], base_off[i+1])` corresponds to the bases for the i-th type entry"""

    base_types: list[str_index_t]
    """Base type table. Base typenames for the i-th type entry are given by `base_types[base_slice(i)]`"""

    base_sizes: list[size_t]
    """Base size table. Base sizes for the i-th type entry are given by `base_sizes[base_slice(i)]`"""

    base_offsets: list[size_t]
    """Base offsets table. Base offsets (relative to the start of the object, in memory) for the i-th type entry are given by `base_offsets[base_slice(i)]`"""

    def field_slice(self, i: int) -> slice:
        """Return the fields corresponding to the given index into the type data table"""
        return slice(self.field_off[i], self.field_off[i + 1])

    def base_slice(self, i: int) -> slice:
        """Return the bases corresponding to the given index into the type data table"""
        return slice(self.base_off[i], self.base_off[i + 1])

    def num_entries(self) -> int:
        return len(self.size)


@dataclass
class TypeData:
    size: int
    """Size of the type, in bytes"""
    name: str
    """Typename of the type. Derived from OutputTypeData.type"""

    field_names: list[str]
    """List of field names"""

    field_types: list[str]
    """List of field types"""

    field_sizes: list[int]
    """List of field sizes"""
    field_offsets: list[int]
    """List of field offsets, relative to the start of the object in memory"""

    base_types: list[str]
    """List of base types"""

    base_sizes: list[int]
    """List of base sizes"""

    base_offsets: list[int]
    """List of base offsets, relative to the start of the object in memory"""


def get_offset(parent_range: range, addr: int) -> addr_t | None:
    if addr in parent_range:
        return addr - parent_range.start
    else:
        return None


@dataclass
class OutputEvent:
    """
    A single memory allocation or deallocation event.
    """

    # Unique chronological event identifier
    id: u64
    # Type of memory operation
    type: EventType
    # Size in bytes (allocation size or freed size)
    alloc_size: size_t
    # Memory address being allocated or freed
    alloc_addr: u64
    # Input pointer (used for operations like realloc)
    alloc_hint: u64
    # Stack trace as indices into frame_table.pc
    pc_id: list[size_t]
    # Object destruction details (FREE events only)
    object_info: OutputObjectInfo | None

    def trace_size(self) -> int:
        return len(self.pc_id)

    def expand_objects(self, ctx: "OutputRecord") -> list[ObjectEnt | None]:
        """Returns a list of entries the same length as the pc_ids, with all objects filled in"""
        entries: list[ObjectEnt | None] = [None] * len(self.pc_id)
        object_info = self.object_info
        if object_info is not None:
            parent_range = range(0, 0)
            # Iterate in reverse, so that we can compute the parent_range as we go along
            for i, object_id, addr, size, type in zip(
                reversed(object_info.trace_index),
                reversed(object_info.object_id),
                reversed(object_info.addr),
                reversed(object_info.size),
                reversed(object_info.type),
            ):
                entries[i] = ObjectEnt(
                    object_id,
                    addr,
                    size,
                    get_offset(parent_range, addr),
                    ctx.strtab[type],
                )
                parent_range = range(addr, addr + size)
        return entries


@dataclass
class OutputFrameTable:
    """
    Stack frame information that maps program counters to source code locations.
    Supports inlined functions where a single PC may have multiple frames.
    """

    # Program counter addresses
    pc: list[addr_t]
    # Object path indices into string table
    object_path: list[str_index_t]
    # Address within the object
    object_address: list[addr_t]
    # Symbol corresponding to the function associated tith this program counter
    # (usually the mangled name of a function, etc)
    object_symbol: list[str_index_t]

    # Frame boundary offsets (length = pc.length + 1)
    offsets: list[size_t]
    # Source file indices into string table
    file: list[str_index_t]
    # Function name indices into string table
    func: list[str_index_t]
    # Source line numbers (0 if unavailable)
    line: list[u32]
    # Source column numbers (0 if unavailable)
    column: list[u32]
    # Inline flags (False=not inlined, True=inlined)
    is_inline: list[bool]

    def frame_count(self, i: int) -> int:
        """
        Get the number of frames for the i-th program counter.
        If inlining occurs, a PC may have more than one associated frame.
        """
        return self.offsets[i + 1] - self.offsets[i]

    def get_frames(self, pc_index: int) -> slice:
        """Get the frame range for a given PC index"""
        return slice(self.offsets[pc_index], self.offsets[pc_index + 1])


@dataclass
class OutputRecord:
    """
    Complete memory profiling data containing frame table, events, and strings.
    """

    # Stack frame information
    frame_table: OutputFrameTable
    # Type data for each recorded type
    type_data_table: OutputTypeData
    # Chronological list of memory events
    event_table: list[OutputEvent]
    # Centralized string storage
    strtab: list[str]

    def strs(self, ii: list[str_index_t]) -> list[str]:
        """Get the list of strings associated with a list of indices into the string table"""
        return [self.strtab[i] for i in ii]

    def get_loc(self, pc_id: int) -> list[str]:
        offsets = self.frame_table.offsets
        sl = slice(offsets[pc_id], offsets[pc_id + 1])
        paths: list[str] = []
        for file, lineno in zip(self.frame_table.file[sl], self.frame_table.line[sl]):
            if lineno != 0:
                paths.append(f"{self.strtab[file]}:{lineno}")
            else:
                paths.append(self.strtab[file])
        return paths

    def get_pc(self, i: int) -> addr_t:
        """Get the program counter for the given entry"""
        return self.frame_table.pc[i]

    def get_object_path(self, i: int) -> str:
        """Path into executable or library where function was found, during the trace"""
        return self.strtab[self.frame_table.object_path[i]]

    def get_object_address(self, i: int) -> int:
        """Address within executable or library where function was found during trace"""
        return self.frame_table.object_address[i]

    def get_object_symbol(self, i: int) -> str:
        return self.strtab[self.frame_table.object_symbol[i]]

    def get_frames(self, i: int) -> slice:
        return slice(self.frame_table.offsets[i], self.frame_table.offsets[i + 1])

    def get_files(self, i: int) -> list[str]:
        """List of source files associated with an index into the frame table"""
        return self.strs(self.frame_table.file[self.get_frames(i)])

    def get_funcs(self, i: int) -> list[str]:
        return self.strs(self.frame_table.func[self.get_frames(i)])

    def get_lines(self, i: int) -> list[int]:
        return self.frame_table.line[self.get_frames(i)]

    def get_columns(self, i: int) -> list[int]:
        return self.frame_table.column[self.get_frames(i)]

    def get_is_inline(self, i: int) -> list[bool]:
        return self.frame_table.is_inline[self.get_frames(i)]

    def get_locs(self, i: int) -> list[Loc]:
        return list(
            map(
                Loc,
                self.get_funcs(i),
                self.get_files(i),
                self.get_lines(i),
                self.get_columns(i),
                self.get_is_inline(i),
            )
        )

    def get_events(self, ii: typing.Iterable[int]) -> list[OutputEvent]:
        return [self.event_table[i] for i in ii]

    def get_type_name(self, type_i: type_index_t) -> str:
        """Get the name of the given type, by the type index"""
        return self.strtab[self.type_data_table.type[type_i]]

    def get_type_size(self, type_i: type_index_t) -> size_t:
        """Get the size of the given type, by the type index"""
        return self.type_data_table.size[type_i]

    def get_field_slice(self, type_i: type_index_t) -> slice:
        """Get the slice into the field table for the given type, by the type index"""
        return self.type_data_table.field_slice(type_i)

    def get_base_slice(self, type_i: type_index_t) -> slice:
        """Get the slice into the base table for the given type, by the type index"""
        return self.type_data_table.base_slice(type_i)

    def get_field_names(self, type_i: type_index_t) -> list[str]:
        """Get the list of fields for a given type, by the type index"""
        return self.strs(self.type_data_table.field_names[self.get_field_slice(type_i)])

    def get_field_types(self, type_i: type_index_t) -> list[str]:
        """Get the list of field types for a given type, by the type index"""
        return self.strs(self.type_data_table.field_types[self.get_field_slice(type_i)])

    def get_field_sizes(self, type_i: type_index_t) -> list[size_t]:
        """Get the list of field sizes for a given type, by the type index"""
        return self.type_data_table.field_sizes[self.get_field_slice(type_i)]

    def get_field_offsets(self, type_i: type_index_t) -> list[size_t]:
        """Get the list of field offsets for a given type, by the type index"""
        return self.type_data_table.field_offsets[self.get_field_slice(type_i)]

    def get_base_types(self, type_i: type_index_t) -> list[str]:
        """Get the list of base class types for a given type, by the type index"""
        return self.strs(self.type_data_table.base_types[self.get_base_slice(type_i)])

    def get_base_sizes(self, type_i: type_index_t) -> list[size_t]:
        """Get the list of base class sizes for a given type, by the type index"""
        return self.type_data_table.base_sizes[self.get_base_slice(type_i)]

    def get_base_offsets(self, type_i: type_index_t) -> list[size_t]:
        """Get the list of base class offsets for a given type, by the type index"""
        return self.type_data_table.base_offsets[self.get_base_slice(type_i)]

    def get_type_data_at(self, i: type_index_t) -> TypeData:
        return TypeData(
            size=self.get_type_size(i),
            name=self.get_type_name(i),
            field_names=self.get_field_names(i),
            field_types=self.get_field_types(i),
            field_sizes=self.get_field_sizes(i),
            field_offsets=self.get_field_offsets(i),
            base_types=self.get_base_types(i),
            base_sizes=self.get_base_sizes(i),
            base_offsets=self.get_base_offsets(i),
        )

    def get_type_data(self) -> list[TypeData]:
        return list(
            map(self.get_type_data_at, range(self.type_data_table.num_entries()))
        )

    def clean(self):
        for e in self.event_table:
            if e.object_info is None:
                continue
            if e.type != EventType.FREE:
                continue
            object_info: OutputObjectInfo = e.object_info
            trace_pc_ids = [
                e.pc_id[trace_index] for trace_index in object_info.trace_index
            ]

            good = [
                i
                for i in range(len(trace_pc_ids))
                if trace_pc_ids
                if "::~"
                in self.strtab[
                    self.frame_table.func[
                        self.frame_table.offsets[trace_pc_ids[i] + 1] - 1
                    ]
                ]
            ]
            e.object_info.clean(good)

    def get_memory_over_time(self) -> tuple[
        npt.NDArray[np.int64],
        npt.NDArray[np.int64],
    ]:
        """
        Returns two arrays.

        - The first array is the total number of allocations
        - The second array is the total number of allocated bytes

        These are measurments 'over time', where each entry corresponds to an event.
        """
        living_allocs = 0
        living_bytes = 0

        # Stores the last event at a given address
        prev_event: dict[addr_t, OutputEvent] = {}

        event_count: int = len(self.event_table)

        alloc_counts = np.zeros(event_count, dtype=np.int64)
        byte_counts = np.zeros(event_count, dtype=np.int64)

        for i, e in enumerate(self.event_table):
            match e.type:
                case EventType.FREE:
                    living_allocs -= 1
                    living_bytes -= e.alloc_size
                case EventType.ALLOC:
                    living_allocs += 1
                    living_bytes += e.alloc_size
                case EventType.REALLOC:
                    e_prior = prev_event[e.alloc_hint]
                    living_bytes -= e_prior.alloc_size
                    living_bytes += e.alloc_size
            prev_event[e.alloc_addr] = e

            alloc_counts[i] = living_allocs
            byte_counts[i] = living_bytes
        return (alloc_counts, byte_counts)

    def still_reachable(self) -> tuple[int, int]:
        """Return the number of allocations and bytes for which a free event was not recorded"""

        alloc_counts, byte_counts = self.get_memory_over_time()
        return int(alloc_counts[-1]), int(byte_counts[-1])

    def event_chains(self) -> list[list[int]]:
        """Returns each chain of events: the allocation, followed by any reallocs, followed by any free (if one exists)"""
        complete: list[list[int]] = []

        open_chains: dict[addr_t, list[int]] = defaultdict(list)

        for i, e in enumerate(self.event_table):
            match e.type:
                case EventType.ALLOC:
                    chain = open_chains[e.alloc_addr]
                    chain.append(i)

                case EventType.FREE:
                    chain = open_chains[e.alloc_addr]
                    chain.append(i)

                    # Clean up the chain - it's no longer open
                    del open_chains[e.alloc_addr]
                    complete.append(chain)

                case EventType.REALLOC:
                    if e.alloc_hint == 0 or e.alloc_hint == e.alloc_addr:
                        # treat like a malloc
                        chain = open_chains[e.alloc_addr]
                        chain.append(i)
                    else:
                        chain = open_chains[e.alloc_hint]
                        chain.append(i)
                        # Move the entry to the new location
                        del open_chains[e.alloc_hint]
                        open_chains[e.alloc_addr] = chain

        complete.extend(open_chains.values())
        return complete

@dataclass
class ObjectTree:
    type_name: str
    location: list[str]
    object_id: int
    direct_size: size_t
    allocated_bytes: size_t
    children: list["ObjectTree"]
