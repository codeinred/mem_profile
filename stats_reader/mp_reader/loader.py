"""
Deserialization utilities for memory profiler output files.
"""

import json
from pathlib import Path
import cattrs

from .malloc_stats import OutputRecord, OutputFrameTable
from .cli_tools import _time


# Create converter with custom hooks
_converter = cattrs.Converter()


# Custom converter for OutputFrameTable to handle is_inline conversion
def _structure_frame_table(data: dict, _) -> OutputFrameTable:
    """Convert JSON dict to OutputFrameTable, converting is_inline from int to bool"""
    data = data.copy()
    data["is_inline"] = [bool(x) for x in data["is_inline"]]
    return cattrs.structure(data, OutputFrameTable)


_converter.register_structure_hook(OutputFrameTable, _structure_frame_table)


def load_from_file(filepath: str | Path) -> OutputRecord:
    """
    Load memory profiler data from a malloc_stats.json file.

    Args:
        filepath: Path to the malloc_stats.json file

    Returns:
        OutputRecord containing the parsed profiler data

    Raises:
        FileNotFoundError: If the file doesn't exist
        json.JSONDecodeError: If the file contains invalid JSON
        cattrs.StructureError: If the JSON structure doesn't match expected format
    """
    with _time("load json"):
        with open(filepath) as f:
            data = json.load(f)
    with _time("create output record"):
        return _converter.structure(data, OutputRecord)


def load_from_dict(data: dict) -> OutputRecord:
    """
    Load memory profiler data from a dictionary.

    Args:
        data: Dictionary containing the profiler data

    Returns:
        OutputRecord containing the parsed profiler data

    Raises:
        cattrs.StructureError: If the data structure doesn't match expected format
    """
    return _converter.structure(data, OutputRecord)
