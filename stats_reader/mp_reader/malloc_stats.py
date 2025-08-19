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

    def get_pc(self, i: int) -> addr_t:
        """Get the program counter for the given entry"""
        return self.pc[i]

    def frame_count(self, i: int) -> int:
        """
        Get the number of frames for the i-th program counter.
        If inlining occurs, a PC may have more than one associated frame.
        """
        return self.offsets[i + 1] - self.offsets[i]

    def get_frames(self, pc_index: int) -> range:
        """Get the frame range for a given PC index"""
        return range(self.offsets[pc_index], self.offsets[pc_index + 1])


@dataclass
class OutputRecord:
    """
    Complete memory profiling data containing frame table, events, and strings.
    """

    # Stack frame information
    frame_table: OutputFrameTable
    # Chronological list of memory events
    event_table: list[OutputEvent]
    # Centralized string storage
    strtab: list[str]

    def get_loc(self, pc_id: int) -> list[str]:
        offsets = self.frame_table.offsets
        sl = slice(offsets[pc_id], offsets[pc_id+1])
        paths: list[str] = []
        for file, lineno in zip(self.frame_table.file[sl], self.frame_table.line[sl]):
            if lineno != 0:
                paths.append(f"{self.strtab[file]}:{lineno}")
            else:
                paths.append(self.strtab[file])
        return paths


@dataclass
class ObjectTree:
    type_name: str
    location: list[str]
    object_id: int
    direct_size: size_t
    allocated_bytes: size_t
    children: list["ObjectTree"]