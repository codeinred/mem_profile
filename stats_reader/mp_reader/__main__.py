"""
Main script for analyzing memory profiler output files.
"""

import argparse
from .loader import load_from_file
from .analyzer import print_objects, print_event_trace, print_allocation_stats



def main():
    parser = argparse.ArgumentParser(description="Analyze memory profiler output")
    parser.add_argument("input_file", help="Path to malloc_stats.json file")
    
    subparsers = parser.add_subparsers(dest="command", help="Available commands")
    
    # Stats command
    stats_parser = subparsers.add_parser("stats", help="Print allocation statistics by type")
    stats_parser.add_argument("-n", "--count", type=int, help="Limit output to top N entries")
    
    args = parser.parse_args()

    record = load_from_file(args.input_file)
    
    if args.command == "stats":
        print_allocation_stats(record, args.count)
    else:
        # Default behavior
        for i in range(len(record.event_table)):
            print_event_trace(record, i, skip_inline=False)
        # print_objects(record)


if __name__ == "__main__":
    main()
