#!/usr/bin/env python3
"""
HPC Scaling & Performance Analysis Tool
Supports:
  1. OpenMP Shared-Memory Thread Scaling (benchmark_thread_scaling.csv)
  2. MPI Distributed-Memory Rank Scaling (benchmark_mpi.csv)
  3. Direct Architectural Comparison: OpenMP vs MPI vs CUDA
"""

import sys
import os
import csv
import math

def detect_csv_type(filepath):
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith('Ranks'):
                return 'mpi'
            if line.startswith('Threads'):
                return 'omp'
    return 'omp'

def load_omp_csv(filepath):
    data = {
        'threads': [],
        'grid_w': [],
        'grid_h': [],
        'total_cells': [],
        'step_ms': [],
        'gen_per_sec': [],
        'speedup': [],
        'efficiency': [],
        'throughput_mcells': [],
        'amdahl_s': []
    }
    with open(filepath, 'r') as f:
        reader = csv.reader(f)
        for row in reader:
            if not row or row[0].startswith('#') or row[0] == 'Threads':
                continue
            data['threads'].append(int(row[0]))
            data['grid_w'].append(int(row[1]))
            data['grid_h'].append(int(row[2]))
            data['total_cells'].append(int(row[3]))
            data['step_ms'].append(float(row[4]))
            data['gen_per_sec'].append(float(row[5]))
            data['speedup'].append(float(row[6]))
            data['efficiency'].append(float(row[7]))
            data['throughput_mcells'].append(float(row[8]))
            data['amdahl_s'].append(float(row[9]))
    return data

def load_mpi_csv(filepath):
    data = {
        'ranks': [],
        'ranks_x': [],
        'ranks_y': [],
        'grid_w': [],
        'grid_h': [],
        'total_cells': [],
        'iterations': [],
        'step_ms': [],
        'gen_per_sec': [],
        'speedup': [],
        'efficiency': [],
        'throughput_mcells': [],
        'imbalance_pct': [],
        'population': []
    }
    with open(filepath, 'r') as f:
        reader = csv.reader(f)
        for row in reader:
            if not row or row[0].startswith('#') or row[0] == 'Ranks':
                continue
            ranks = int(row[0])
            step_ms = float(row[7])
            gen_sec = float(row[8])
            tp = float(row[9])
            imb = float(row[10])
            pop = int(row[11])

            data['ranks'].append(ranks)
            data['ranks_x'].append(int(row[1]))
            data['ranks_y'].append(int(row[2]))
            data['grid_w'].append(int(row[3]))
            data['grid_h'].append(int(row[4]))
            data['total_cells'].append(int(row[5]))
            data['iterations'].append(int(row[6]))
            data['step_ms'].append(step_ms)
            data['gen_per_sec'].append(gen_sec)
            data['throughput_mcells'].append(tp)
            data['imbalance_pct'].append(imb)
            data['population'].append(pop)

    # Compute Speedup and Efficiency relative to rank 1 baseline if available
    t1 = data['step_ms'][0] if data['step_ms'] else 1.0
    for i, r in enumerate(data['ranks']):
        sp = t1 / data['step_ms'][i] if data['step_ms'][i] > 0 else 1.0
        eff = (sp / r) * 100.0 if r > 0 else 100.0
        data['speedup'].append(sp)
        data['efficiency'].append(eff)

    return data

def print_mpi_analysis(data):
    print("\n" + "=" * 80)
    print("             DISTRIBUTED MPI SCALING REPORT SUMMARY")
    print("=" * 80)
    print(f" Grid Resolution  : {data['grid_w'][0]} x {data['grid_h'][0]} ({data['total_cells'][0]:,} cells)")
    print(f" Baseline (1 Rank): {data['step_ms'][0]:.2f} ms/step ({data['gen_per_sec'][0]:.1f} gen/s)")
    max_idx = data['speedup'].index(max(data['speedup']))
    print(f" Peak Speedup     : {data['speedup'][max_idx]:.2f}x at {data['ranks'][max_idx]} ranks ({data['step_ms'][max_idx]:.2f} ms/step)")
    print(f" Peak Throughput  : {max(data['throughput_mcells']):.1f} Megacells/sec")
    print(f" Avg Imbalance    : {sum(data['imbalance_pct'])/len(data['imbalance_pct']):.2f}%")
    print("=" * 80 + "\n")

def plot_mpi(data, out_dir="scripts"):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("[NOTICE] 'matplotlib' is not installed.")
        return

    os.makedirs(out_dir, exist_ok=True)
    ranks = data['ranks']
    speedup = data['speedup']
    efficiency = data['efficiency']
    throughput = data['throughput_mcells']
    grid_dim = f"{data['grid_w'][0]}x{data['grid_h'][0]}"

    # Figure 1: MPI Speedup
    plt.figure(figsize=(8, 5.5), dpi=300)
    plt.plot(ranks, ranks, 'k--', label='Ideal Linear Speedup ($S = P$)', linewidth=1.5)
    plt.plot(ranks, speedup, 'mo-', label='Measured 2D Cartesian MPI Speedup', linewidth=2.2, markersize=7)
    plt.title(f"MPI Distributed Strong Scaling Speedup ({grid_dim} Grid)", fontsize=13, fontweight='bold')
    plt.xlabel("Number of MPI Ranks ($P$)", fontsize=11)
    plt.ylabel("Parallel Speedup ($S_P = T_1 / T_P$)", fontsize=11)
    plt.xticks(ranks)
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.legend(loc='upper left', fontsize=10)
    plt.tight_layout()
    f1 = os.path.join(out_dir, "mpi_scaling_speedup.png")
    plt.savefig(f1)
    plt.close()
    print(f"[OK] Saved: {f1}")

    # Figure 2: MPI Efficiency
    plt.figure(figsize=(8, 5.5), dpi=300)
    colors = ['#2ca02c' if eff >= 80.0 else '#ff7f0e' if eff >= 50.0 else '#d62728' for eff in efficiency]
    bars = plt.bar([str(r) for r in ranks], efficiency, color=colors, width=0.55, edgecolor='black', alpha=0.85)
    plt.axhline(80.0, color='green', linestyle='--', linewidth=1.2, label='80% High Efficiency Threshold')
    plt.axhline(50.0, color='orange', linestyle=':', linewidth=1.2, label='50% Threshold')
    for bar, eff in zip(bars, efficiency):
        yval = bar.get_height()
        plt.text(bar.get_x() + bar.get_width()/2.0, yval + 1.5, f"{eff:.1f}%", ha='center', va='bottom', fontsize=9, fontweight='bold')
    plt.title(f"MPI Parallel Efficiency ({grid_dim} Grid)", fontsize=13, fontweight='bold')
    plt.xlabel("Number of MPI Ranks", fontsize=11)
    plt.ylabel("Parallel Efficiency $E(P) = \\frac{S(P)}{P} \\times 100\\%$", fontsize=11)
    plt.ylim(0, max(120, max(efficiency) + 15))
    plt.grid(axis='y', linestyle='--', alpha=0.6)
    plt.legend(loc='upper right', fontsize=10)
    plt.tight_layout()
    f2 = os.path.join(out_dir, "mpi_parallel_efficiency.png")
    plt.savefig(f2)
    plt.close()
    print(f"[OK] Saved: {f2}")

    # Figure 3: MPI Throughput
    plt.figure(figsize=(8, 5.5), dpi=300)
    plt.plot(ranks, throughput, 'd-', color='#e377c2', linewidth=2.2, markersize=7, label='MPI Update Throughput')
    for r, tp in zip(ranks, throughput):
        plt.annotate(f"{tp:.1f} MC/s", (r, tp), textcoords="offset points", xytext=(0, 8), ha='center', fontsize=8.5)
    plt.title(f"MPI Computational Throughput ({grid_dim} Grid)", fontsize=13, fontweight='bold')
    plt.xlabel("Number of MPI Ranks", fontsize=11)
    plt.ylabel("Throughput (Million Cells / Second)", fontsize=11)
    plt.xticks(ranks)
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.legend(loc='lower right', fontsize=10)
    plt.tight_layout()
    f3 = os.path.join(out_dir, "mpi_throughput_scaling.png")
    plt.savefig(f3)
    plt.close()
    print(f"[OK] Saved: {f3}")

def print_omp_analysis(data):
    print("\n" + "=" * 80)
    print("                OPENMP THREAD SCALING REPORT SUMMARY")
    print("=" * 80)
    print(f" Grid Resolution  : {data['grid_w'][0]} x {data['grid_h'][0]} ({data['total_cells'][0]:,} cells)")
    print(f" Baseline (1 Th.) : {data['step_ms'][0]:.2f} ms/step ({data['gen_per_sec'][0]:.1f} gen/s)")
    max_idx = data['speedup'].index(max(data['speedup']))
    print(f" Peak Speedup     : {data['speedup'][max_idx]:.2f}x at {data['threads'][max_idx]} threads ({data['step_ms'][max_idx]:.2f} ms/step)")
    print(f" Peak Throughput  : {max(data['throughput_mcells']):.1f} Megacells/sec")

    s_vals = [s for s in data['amdahl_s'] if s > 0.0]
    avg_s = sum(s_vals) / len(s_vals) if s_vals else 0.0
    print(f" Empirical Amdahl Serial Fraction (s): {avg_s * 100.0:.2f}% (Parallel fraction f = {(1 - avg_s) * 100.0:.2f}%)")
    print("=" * 80)

    print("\n Speedup Curve (ASCII preview):")
    max_sp = max(data['speedup'])
    for t, sp in zip(data['threads'], data['speedup']):
        bar_len = int((sp / max(max_sp, max(data['threads']))) * 45)
        bar = "█" * bar_len
        print(f"  {t:2d} threads | {bar} {sp:.2f}x")
    print("=" * 80 + "\n")

def plot_omp(data, out_dir="scripts"):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("[NOTICE] 'matplotlib' is not installed.")
        return

    os.makedirs(out_dir, exist_ok=True)
    threads = data['threads']
    speedup = data['speedup']
    efficiency = data['efficiency']
    throughput = data['throughput_mcells']
    grid_dim = f"{data['grid_w'][0]}x{data['grid_h'][0]}"

    # Figure 1: Speedup vs Threads
    plt.figure(figsize=(8, 5.5), dpi=300)
    plt.plot(threads, threads, 'k--', label='Ideal Linear Speedup ($S = p$)', linewidth=1.5)
    plt.plot(threads, speedup, 'bo-', label='Measured OpenMP Speedup', linewidth=2.2, markersize=7)

    s_vals = [s for s in data['amdahl_s'] if s > 0.0]
    s_fit = sum(s_vals) / len(s_vals) if s_vals else 0.03
    p_fine = [1.0 + i * 0.1 for i in range(int((max(threads) - 1) * 10) + 1)]
    amdahl_fine = [1.0 / (s_fit + (1.0 - s_fit) / p) for p in p_fine]
    plt.plot(p_fine, amdahl_fine, 'r:', label=f"Amdahl's Law Model ($s={s_fit*100:.1f}\\%$)", linewidth=1.8)

    plt.title(f"OpenMP Strong Scaling Speedup ({grid_dim} Grid)", fontsize=13, fontweight='bold')
    plt.xlabel("Number of OpenMP Threads ($p$)", fontsize=11)
    plt.ylabel("Parallel Speedup ($S_p = T_1 / T_p$)", fontsize=11)
    plt.xticks(threads)
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.legend(loc='upper left', fontsize=10)
    plt.tight_layout()

    fig1_path = os.path.join(out_dir, "strong_scaling_speedup.png")
    plt.savefig(fig1_path)
    plt.close()
    print(f"[OK] Saved: {fig1_path}")

    # Figure 2: Efficiency vs Threads
    plt.figure(figsize=(8, 5.5), dpi=300)
    colors = ['#2ca02c' if eff >= 80.0 else '#ff7f0e' if eff >= 50.0 else '#1f77b4' for eff in efficiency]
    bars = plt.bar([str(t) for t in threads], efficiency, color=colors, width=0.55, edgecolor='black', alpha=0.85)

    plt.axhline(80.0, color='green', linestyle='--', linewidth=1.2, label='80% High Efficiency Threshold')
    plt.axhline(50.0, color='orange', linestyle=':', linewidth=1.2, label='50% Scalability Threshold')

    for bar, eff in zip(bars, efficiency):
        yval = bar.get_height()
        plt.text(bar.get_x() + bar.get_width()/2.0, yval + 1.5, f"{eff:.1f}%", ha='center', va='bottom', fontsize=9, fontweight='bold')

    plt.title(f"OpenMP Parallel Efficiency ({grid_dim} Grid)", fontsize=13, fontweight='bold')
    plt.xlabel("Number of OpenMP Threads", fontsize=11)
    plt.ylabel("Parallel Efficiency $E(p) = \\frac{S(p)}{p} \\times 100\\%$", fontsize=11)
    plt.ylim(0, max(120, max(efficiency) + 15))
    plt.grid(axis='y', linestyle='--', alpha=0.6)
    plt.legend(loc='upper right', fontsize=10)
    plt.tight_layout()

    fig2_path = os.path.join(out_dir, "parallel_efficiency.png")
    plt.savefig(fig2_path)
    plt.close()
    print(f"[OK] Saved: {fig2_path}")

    # Figure 3: Throughput
    plt.figure(figsize=(8, 5.5), dpi=300)
    plt.plot(threads, throughput, 's-', color='#9467bd', linewidth=2.2, markersize=7, label='Cell Update Throughput')
    for t, tp in zip(threads, throughput):
        plt.annotate(f"{tp:.1f} MCells/s", (t, tp), textcoords="offset points", xytext=(0, 8), ha='center', fontsize=8.5)

    plt.title(f"Computational Throughput vs Thread Count ({grid_dim} Grid)", fontsize=13, fontweight='bold')
    plt.xlabel("Number of OpenMP Threads", fontsize=11)
    plt.ylabel("Throughput (Million Cells / Second)", fontsize=11)
    plt.xticks(threads)
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.legend(loc='lower right', fontsize=10)
    plt.tight_layout()

    fig3_path = os.path.join(out_dir, "throughput_scaling.png")
    plt.savefig(fig3_path)
    plt.close()
    print(f"[OK] Saved: {fig3_path}")

def main():
    filepath = sys.argv[1] if len(sys.argv) > 1 else "benchmark_thread_scaling.csv"
    if not os.path.exists(filepath):
        print(f"[ERROR] CSV file not found: {filepath}")
        sys.exit(1)

    csv_type = detect_csv_type(filepath)
    if csv_type == 'mpi':
        data = load_mpi_csv(filepath)
        print_mpi_analysis(data)
        plot_mpi(data)
    else:
        data = load_omp_csv(filepath)
        print_omp_analysis(data)
        plot_omp(data)

if __name__ == "__main__":
    main()
