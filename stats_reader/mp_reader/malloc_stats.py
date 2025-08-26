"""
Python dataclasses for memory profiler output_record structure.

Replicates the C++ structures defined in mem_profile/output_record.h
"""

from dataclasses import dataclass
from enum import Enum

# Type aliases matching C++ types
addr_t = int  # uintptr_t
str_index_t = int  # size_t
u64 = int
u32 = int
u8 = int
size_t = int


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
    addr: addr_t
    size: size_t
    """Offset of this object, within it's parent"""
    offset: addr_t | None
    type: str

    def mem_range(self) -> range:
        return range(self.addr, self.addr + self.size)


@dataclass
class OutputObjectInfo:
    """
    Information about objects being destroyed during a FREE event.

    This is a sparse representation - only stack frames that involve
    destructor calls are recorded via trace_index.
    """

    # Indices into event.pc_id array - which stack frames have destructors
    trace_index: list[size_t]
    # Unique object identifiers (lifetime-unique)
    object_id: list[u64]
    # Object addresses (this pointers at destruction)
    addr: list[addr_t]
    # Object sizes in bytes
    size: list[size_t]
    # Type name indices into string table
    type: list[str_index_t]
    # Index into type data table for each entry
    type_data: list[size_t]


@dataclass
class OutputTypeData:
    """
    Type data table for the given OutputRecord.

    Holds information about object types, sizes, fields, and offsets
    """

    # Type size
    size: list[size_t]

    # Type name
    type: list[str_index_t]

    # Offsets into field table
    field_off: list[size_t]

    field_names: list[str_index_t]
    field_types: list[str_index_t]
    field_sizes: list[size_t]
    field_offsets: list[size_t]

    base_off: list[size_t]
    base_types: list[str_index_t]
    base_sizes: list[size_t]
    base_offsets: list[size_t]

    def field_slice(self, i: int) -> slice:
        """Return the fields corresponding to the given index into the type data table"""
        return slice(self.field_off[i], self.field_off[i+1])

    def base_slice(self, i: int) -> slice:
        """Return the bases corresponding to the given index into the type data table"""
        return slice(self.base_off[i], self.base_off[i+1])


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
        entries = [None] * len(self.pc_id)
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
                entries[i] = ObjectEnt(object_id, addr, size, get_offset(parent_range, addr), ctx.strtab[type])
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

    def get_object_symbol(self, i: int) -> int:
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



    def get_type_name(self, type_i: int) -> str:
        """Get the name of the given type, by the type index"""
        return self.strtab[self.type_data_table.type[type_i]]

    def get_type_size(self, type_i: int) -> size_t:
        """Get the size of the given type, by the type index"""
        return self.type_data_table.size[type_i]

    def get_field_slice(self, type_i: int) -> slice:
        """Get the slice into the field table for the given type, by the type index"""
        return self.type_data_table.field_slice(type_i)

    def get_base_slice(self, type_i: int) -> slice:
        """Get the slice into the base table for the given type, by the type index"""
        return self.type_data_table.base_slice(type_i)

    def get_field_names(self, type_i: int) -> list[str]:
        """Get the list of fields for a given type, by the type index"""
        return self.strs(self.type_data_table.field_names[self.get_field_slice(type_i)])

    def get_field_types(self, type_i: int) -> list[str]:
        """Get the list of field types for a given type, by the type index"""
        return self.strs(self.type_data_table.field_types[self.get_field_slice(type_i)])

    def get_field_sizes(self, type_i: int) -> list[size_t]:
        """Get the list of field sizes for a given type, by the type index"""
        return self.type_data_table.field_sizes[self.get_field_slice(type_i)]

    def get_field_offsets(self, type_i: int) -> list[size_t]:
        """Get the list of field offsets for a given type, by the type index"""
        return self.type_data_table.field_offsets[self.get_field_slice(type_i)]

    def get_base_types(self, type_i: int) -> list[str]:
        """Get the list of base class types for a given type, by the type index"""
        return self.strs(self.type_data_table.base_types[self.get_base_slice(type_i)])

    def get_base_sizes(self, type_i: int) -> list[size_t]:
        """Get the list of base class sizes for a given type, by the type index"""
        return self.type_data_table.base_sizes[self.get_base_slice(type_i)]

    def get_base_offsets(self, type_i: int) -> list[size_t]:
        """Get the list of base class offsets for a given type, by the type index"""
        return self.type_data_table.base_offsets[self.get_base_slice(type_i)]





@dataclass
class ObjectTree:
    type_name: str
    location: list[str]
    object_id: int
    direct_size: size_t
    allocated_bytes: size_t
    children: list["ObjectTree"]
