"""
Main script for analyzing memory profiler output files.
"""

import argparse
from .loader import load_from_file
from .analyzer import print_objects, print_event_trace



def main():
    parser = argparse.ArgumentParser(description="Analyze memory profiler output")
    parser.add_argument("input_file", help="Path to malloc_stats.json file")
    args = parser.parse_args()

    record = load_from_file(args.input_file)
    for i in range(len(record.event_table)):
        print_event_trace(record, i)
    # print_objects(record)


if __name__ == "__main__":
    main()
