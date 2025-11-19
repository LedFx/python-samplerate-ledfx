#!/usr/bin/env python
"""
Performance comparison between pybind11 and nanobind implementations.
"""

import sys
import time
import numpy as np
from pathlib import Path

# Import pybind11 version (installed)
import samplerate as sr_pb

# Import nanobind version
repo_root = Path(__file__).parent
sys.path.insert(0, str(repo_root / 'build/lib.linux-x86_64-cpython-312'))
import samplerate as sr_nb

print("=" * 70)
print("Performance Comparison: pybind11 vs nanobind")
print("=" * 70)

# Test data of various sizes
test_sizes = [1000, 10000, 100000]
ratios = [1.5, 2.0, 0.5]
converters = ['sinc_fastest', 'sinc_medium', 'sinc_best']

results = []

for size in test_sizes:
    for ratio in ratios:
        for converter in converters:
            # Generate test data
            np.random.seed(42)
            input_1d = np.sin(2 * np.pi * 5 * np.arange(size) / size).astype(np.float32)
            
            # Test pybind11
            start = time.perf_counter()
            for _ in range(10):
                _ = sr_pb.resample(input_1d, ratio, converter)
            pb_time = (time.perf_counter() - start) / 10
            
            # Test nanobind
            start = time.perf_counter()
            for _ in range(10):
                _ = sr_nb.resample(input_1d, ratio, converter)
            nb_time = (time.perf_counter() - start) / 10
            
            speedup = pb_time / nb_time if nb_time > 0 else 1.0
            
            results.append({
                'size': size,
                'ratio': ratio,
                'converter': converter,
                'pybind11': pb_time * 1000,  # ms
                'nanobind': nb_time * 1000,  # ms
                'speedup': speedup
            })

print(f"\n{'Size':<10} {'Ratio':<7} {'Converter':<15} {'pybind11':<12} {'nanobind':<12} {'Speedup':<10}")
print("-" * 70)

for r in results:
    print(f"{r['size']:<10} {r['ratio']:<7.1f} {r['converter']:<15} "
          f"{r['pybind11']:<12.3f} {r['nanobind']:<12.3f} {r['speedup']:<10.2f}x")

# Calculate averages
avg_pb = np.mean([r['pybind11'] for r in results])
avg_nb = np.mean([r['nanobind'] for r in results])
avg_speedup = np.mean([r['speedup'] for r in results])

print("-" * 70)
print(f"{'AVERAGE':<33} {avg_pb:<12.3f} {avg_nb:<12.3f} {avg_speedup:<10.2f}x")

print("\n" + "=" * 70)
print(f"Average runtime speedup: {avg_speedup:.2f}x")
print("=" * 70)

# Check file sizes
import os
pb_so = next(Path('/home/runner/.local/lib/python3.12/site-packages').glob('**/samplerate*.so'))
nb_so = repo_root / 'build/lib.linux-x86_64-cpython-312/samplerate.cpython-312-x86_64-linux-gnu.so'

pb_size = os.path.getsize(pb_so)
nb_size = os.path.getsize(nb_so)

print(f"\nBinary sizes:")
print(f"  pybind11: {pb_size:,} bytes ({pb_size/1024:.1f} KB)")
print(f"  nanobind: {nb_size:,} bytes ({nb_size/1024:.1f} KB)")
print(f"  Size reduction: {(1 - nb_size/pb_size)*100:.1f}%")
print("=" * 70)
