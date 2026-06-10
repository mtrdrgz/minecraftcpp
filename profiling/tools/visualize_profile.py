#!/usr/bin/env python3
"""
Profiling data visualization.
Generates graphs and heatmaps to visualize performance bottlenecks.
Requires: matplotlib, pandas
Usage: python visualize_profile.py <profile.csv> [output_dir]
"""

import sys
import csv
from collections import defaultdict
from pathlib import Path

try:
    import matplotlib.pyplot as plt
    import matplotlib
    matplotlib.use('Agg')  # Use non-interactive backend
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False

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

def plot_event_pie(entries, output_dir):
    """Create pie chart of time spent per event type."""
    if not HAS_MATPLOTLIB:
        print("Warning: matplotlib not available, skipping pie chart")
        return
    
    event_totals = defaultdict(float)
    for entry in entries:
        event_totals[entry['event']] += entry['duration']
    
    fig, ax = plt.subplots(figsize=(12, 8))
    events = list(event_totals.keys())
    durations = list(event_totals.values())
    
    # Sort by duration
    sorted_data = sorted(zip(events, durations), key=lambda x: x[1], reverse=True)
    events, durations = zip(*sorted_data)
    
    colors = plt.cm.Set3(range(len(events)))
    ax.pie(durations, labels=events, autopct='%1.1f%%', colors=colors, startangle=90)
    ax.set_title('Time Distribution by Event Type', fontsize=14, fontweight='bold')
    
    plt.tight_layout()
    output_path = Path(output_dir) / 'event_distribution.png'
    plt.savefig(str(output_path), dpi=150)
    print(f"Saved: {output_path}")
    plt.close()

def plot_event_bars(entries, output_dir):
    """Create bar chart of average time per event type."""
    if not HAS_MATPLOTLIB:
        return
    
    event_stats = defaultdict(lambda: {'total': 0.0, 'count': 0})
    for entry in entries:
        event_stats[entry['event']]['total'] += entry['duration']
        event_stats[entry['event']]['count'] += 1
    
    for event in event_stats:
        event_stats[event]['avg'] = event_stats[event]['total'] / event_stats[event]['count']
    
    fig, ax = plt.subplots(figsize=(14, 8))
    events = sorted(event_stats.keys(), key=lambda x: event_stats[x]['avg'], reverse=True)
    avgs = [event_stats[e]['avg'] for e in events]
    totals = [event_stats[e]['total'] for e in events]
    
    x_pos = range(len(events))
    bars1 = ax.bar([i - 0.2 for i in x_pos], avgs, 0.4, label='Avg Time (ms)', color='steelblue')
    ax2 = ax.twinx()
    bars2 = ax2.bar([i + 0.2 for i in x_pos], totals, 0.4, label='Total Time (ms)', color='coral')
    
    ax.set_xlabel('Event Type', fontsize=12)
    ax.set_ylabel('Avg Time (ms)', fontsize=12, color='steelblue')
    ax2.set_ylabel('Total Time (ms)', fontsize=12, color='coral')
    ax.set_title('Average vs Total Time per Event', fontsize=14, fontweight='bold')
    ax.set_xticks(x_pos)
    ax.set_xticklabels(events, rotation=45, ha='right')
    
    ax.tick_params(axis='y', labelcolor='steelblue')
    ax2.tick_params(axis='y', labelcolor='coral')
    
    ax.grid(True, alpha=0.3, axis='y')
    
    lines1, labels1 = ax.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax.legend(lines1 + lines2, labels1 + labels2, loc='upper right')
    
    plt.tight_layout()
    output_path = Path(output_dir) / 'event_timings.png'
    plt.savefig(str(output_path), dpi=150)
    print(f"Saved: {output_path}")
    plt.close()

def plot_timeline(entries, output_dir):
    """Create timeline plot of events over time."""
    if not HAS_MATPLOTLIB:
        return
    
    # Group events by type
    event_groups = defaultdict(list)
    for entry in entries:
        event_groups[entry['event']].append((entry['timestamp'], entry['duration']))
    
    fig, ax = plt.subplots(figsize=(14, 8))
    
    colors = plt.cm.Set3(range(len(event_groups)))
    for idx, (event, data) in enumerate(sorted(event_groups.items())):
        times = [x[0] for x in data]
        durations = [x[1] for x in data]
        ax.scatter(times, durations, label=event, color=colors[idx], s=50, alpha=0.7)
    
    ax.set_xlabel('Time (seconds)', fontsize=12)
    ax.set_ylabel('Duration (ms)', fontsize=12)
    ax.set_title('Event Timeline', fontsize=14, fontweight='bold')
    ax.legend(loc='best')
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    output_path = Path(output_dir) / 'timeline.png'
    plt.savefig(str(output_path), dpi=150)
    print(f"Saved: {output_path}")
    plt.close()

def plot_cumulative(entries, output_dir):
    """Create cumulative time plot."""
    if not HAS_MATPLOTLIB:
        return
    
    event_cumulative = defaultdict(float)
    events = sorted(set(e['event'] for e in entries))
    
    # Sort entries by timestamp
    sorted_entries = sorted(entries, key=lambda x: x['timestamp'])
    
    timestamps = []
    cumulative_lines = {event: [] for event in events}
    
    for entry in sorted_entries:
        event_cumulative[entry['event']] += entry['duration']
        timestamps.append(entry['timestamp'])
        for event in events:
            cumulative_lines[event].append(event_cumulative[event])
    
    fig, ax = plt.subplots(figsize=(14, 8))
    
    colors = plt.cm.Set3(range(len(events)))
    for idx, event in enumerate(events):
        ax.plot(timestamps, cumulative_lines[event], label=event, color=colors[idx], linewidth=2)
    
    ax.set_xlabel('Time (seconds)', fontsize=12)
    ax.set_ylabel('Cumulative Time (ms)', fontsize=12)
    ax.set_title('Cumulative Time by Event Type', fontsize=14, fontweight='bold')
    ax.legend(loc='best')
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    output_path = Path(output_dir) / 'cumulative.png'
    plt.savefig(str(output_path), dpi=150)
    print(f"Saved: {output_path}")
    plt.close()

def main():
    if len(sys.argv) < 2:
        print("Usage: python visualize_profile.py <profile.csv> [output_dir]")
        sys.exit(1)
    
    filename = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else Path(filename).parent / "viz"
    
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
    
    Path(output_dir).mkdir(parents=True, exist_ok=True)
    
    print(f"Loaded {len(entries)} profiling entries")
    print(f"Generating visualizations to {output_dir}...")
    
    if not HAS_MATPLOTLIB:
        print("WARNING: matplotlib not installed. Install with: pip install matplotlib")
        return
    
    plot_event_pie(entries, output_dir)
    plot_event_bars(entries, output_dir)
    plot_timeline(entries, output_dir)
    plot_cumulative(entries, output_dir)
    
    print("Done!")

if __name__ == "__main__":
    main()
