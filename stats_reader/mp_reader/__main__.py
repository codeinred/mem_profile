"""
Main script for analyzing memory profiler output files.
"""

import typer
from .analyzer import stats

app = typer.Typer(help="Analyze memory profiler output")

app.command("stats")(stats)
app.command("stats2")(stats)

if __name__ == "__main__":
    app()
