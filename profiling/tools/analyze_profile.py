#!/usr/bin/env python3
"""
Profiling data analyzer and report generator.
Usage: python analyze_profile.py <profile.csv>
"""

import sys
import csv
from collections import defaultdict
from pathlib import Path

def load_profile(filename):
    """Load profiling CSV data."""
    entries = []
    with open(filename, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            entries.append({
                'timestamp': float(row['Timestamp(s)']),
                'event': row['Event'],
                'chunk': row['ChunkKey'],
                'duration': float(row['Duration(ms)']),
                'thread': row['ThreadID']
            })
    return entries

def analyze_events(entries):
    """Analyze timing per event type."""
    stats = defaultdict(lambda: {'count': 0, 'total': 0.0, 'min': float('inf'), 'max': 0.0, 'durations': []})
    
    for entry in entries:
        event = entry['event']
        stats[event]['count'] += 1
        stats[event]['total'] += entry['duration']
        stats[event]['min'] = min(stats[event]['min'], entry['duration'])
        stats[event]['max'] = max(stats[event]['max'], entry['duration'])
        stats[event]['durations'].append(entry['duration'])
    
    # Compute averages
    for event in stats:
        stats[event]['avg'] = stats[event]['total'] / stats[event]['count']
    
    return stats

def analyze_chunks(entries):
    """Analyze timing per chunk position."""
    stats = defaultdict(lambda: {'count': 0, 'total': 0.0, 'min': float('inf'), 'max': 0.0})
    
    for entry in entries:
        chunk = entry['chunk']
        stats[chunk]['count'] += 1
        stats[chunk]['total'] += entry['duration']
        stats[chunk]['min'] = min(stats[chunk]['min'], entry['duration'])
        stats[chunk]['max'] = max(stats[chunk]['max'], entry['duration'])
    
    # Compute averages
    for chunk in stats:
        stats[chunk]['avg'] = stats[chunk]['total'] / stats[chunk]['count']
    
    return stats

def analyze_threads(entries):
    """Analyze timing per thread."""
    stats = defaultdict(lambda: {'count': 0, 'total': 0.0})
    
    for entry in entries:
        thread = entry['thread']
        stats[thread]['count'] += 1
        stats[thread]['total'] += entry['duration']
    
    return stats

def print_event_analysis(stats):
    """Print event timing breakdown."""
    print("\n" + "="*90)
    print("EVENT TIMING ANALYSIS (Sorted by Total Time)")
    print("="*90)
    print(f"{'Event':<30} {'Count':>8} {'Total(ms)':>12} {'Avg(ms)':>12} {'Min(ms)':>12} {'Max(ms)':>12}")
    print("-"*90)
    
    sorted_stats = sorted(stats.items(), key=lambda x: x[1]['total'], reverse=True)
    
    total_all = sum(s['total'] for _, s in sorted_stats)
    
    for event, stat in sorted_stats:
        pct = (stat['total'] / total_all * 100) if total_all > 0 else 0
        print(f"{event:<30} {stat['count']:>8} {stat['total']:>12.2f} {stat['avg']:>12.2f} {stat['min']:>12.2f} {stat['max']:>12.2f} [{pct:>5.1f}%]")
    
    print("-"*90)
    print(f"{'TOTAL':<30} {sum(s['count'] for _, s in sorted_stats):>8} {total_all:>12.2f}")

def print_chunk_analysis(stats, limit=20):
    """Print per-chunk timing breakdown."""
    print("\n" + "="*90)
    print(f"CHUNK TIMING ANALYSIS (Top {limit} by Total Time)")
    print("="*90)
    print(f"{'Chunk':<20} {'Count':>8} {'Total(ms)':>12} {'Avg(ms)':>12} {'Min(ms)':>12} {'Max(ms)':>12}")
    print("-"*90)
    
    sorted_stats = sorted(stats.items(), key=lambda x: x[1]['total'], reverse=True)
    
    for i, (chunk, stat) in enumerate(sorted_stats[:limit]):
        if chunk:  # Skip empty chunks (no chunk key)
            print(f"{chunk:<20} {stat['count']:>8} {stat['total']:>12.2f} {stat['avg']:>12.2f} {stat['min']:>12.2f} {stat['max']:>12.2f}")

def print_thread_analysis(stats):
    """Print per-thread timing breakdown."""
    print("\n" + "="*90)
    print("THREAD UTILIZATION ANALYSIS")
    print("="*90)
    print(f"{'Thread ID':<30} {'Events':>8} {'Total(ms)':>12} {'Avg Event(ms)':>15}")
    print("-"*90)
    
    sorted_stats = sorted(stats.items(), key=lambda x: x[1]['total'], reverse=True)
    
    for thread, stat in sorted_stats:
        avg_per_event = stat['total'] / stat['count'] if stat['count'] > 0 else 0
        print(f"{thread:<30} {stat['count']:>8} {stat['total']:>12.2f} {avg_per_event:>15.2f}")

def main():
    if len(sys.argv) < 2:
        print("Usage: python analyze_profile.py <profile.csv>")
        sys.exit(1)
    
    filename = sys.argv[1]
    
    try:
        entries = load_profile(filename)
    except FileNotFoundError:
        print(f"Error: File not found: {filename}")
        sys.exit(1)
    except Exception as e:
        print(f"Error reading profile: {e}")
        sys.exit(1)
    
    if not entries:
        print("Error: No profiling data in file")
        sys.exit(1)
    
    print(f"\nLoaded {len(entries)} profiling entries from {filename}")
    print(f"Profiling duration: {entries[-1]['timestamp'] - entries[0]['timestamp']:.2f} seconds")
    
    event_stats = analyze_events(entries)
    chunk_stats = analyze_chunks(entries)
    thread_stats = analyze_threads(entries)
    
    print_event_analysis(event_stats)
    print_chunk_analysis(chunk_stats)
    print_thread_analysis(thread_stats)
    
    # Summary
    print("\n" + "="*90)
    print("SUMMARY")
    print("="*90)
    
    # Find bottleneck
    slowest_event = max(event_stats.items(), key=lambda x: x[1]['total'])
    print(f"\nSlowest operation: {slowest_event[0]}")
    print(f"  Total time: {slowest_event[1]['total']:.2f}ms ({slowest_event[1]['total']/sum(s['total'] for _, s in event_stats.items())*100:.1f}%)")
    print(f"  Avg per call: {slowest_event[1]['avg']:.2f}ms")
    print(f"  Call count: {slowest_event[1]['count']}")

if __name__ == "__main__":
    main()
