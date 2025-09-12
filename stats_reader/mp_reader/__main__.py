"""
Main script for analyzing memory profiler output files.
"""

import typer
from .analyzer import stats, dump_events

app = typer.Typer(help="Analyze memory profiler output")

app.command("stats")(stats)
app.command("dump_events")(dump_events)

if __name__ == "__main__":
    app()
