"""
CLI utilities and helpers for the memory profiler reader.
"""

import time
from dataclasses import dataclass
from .color import bb_yellow


@dataclass
class Timer:
    """Context manager for timing operations and optionally displaying results."""

    description: str
    threshold_seconds: float = 1.0
    start_time: float = 0.0
    end_time: float = 0.0

    def __enter__(self):
        self.start_time = time.time()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.end_time = time.time()
        elapsed = self.end_time - self.start_time

        if elapsed >= self.threshold_seconds:
            formatted_time = self._format_duration(elapsed)
            print(f"  {bb_yellow(formatted_time)} to {self.description}...")

    @staticmethod
    def _format_duration(seconds: float) -> str:
        """Format duration in a human-readable format."""
        if seconds < 60:
            return f"{seconds:.1f}s"

        minutes = int(seconds // 60)
        remaining_seconds = seconds % 60

        if minutes < 60:
            return f"{minutes}m {remaining_seconds:.1f}s"

        hours = int(minutes // 60)
        remaining_minutes = minutes % 60
        return f"{hours}h {remaining_minutes}m {remaining_seconds:.1f}s"


def _time(description: str, threshold_seconds: float = 1.0) -> Timer:
    """
    Create a timing context manager.

    Usage:
        with _time("load json"):
            # code here

    Args:
        description: Description of the operation being timed
        threshold_seconds: Minimum duration to display timing info (default: 1.0)

    Returns:
        Timer context manager
    """
    return Timer(description, threshold_seconds)
