#!/usr/bin/env python
"""
Detailed performance comparison between pybind11 and nanobind implementations.
Focus on real-time audio analysis scenarios with callback-based resampling.
"""

import sys
import time
import numpy as np
from pathlib import Path
import statistics

# Import pybind11 version (installed)
import samplerate as sr_pb

# Import nanobind version
repo_root = Path(__file__).parent
sys.path.insert(0, str(repo_root / 'build/lib.linux-x86_64-cpython-312'))
import samplerate as sr_nb

print("=" * 80)
print("DETAILED PERFORMANCE COMPARISON: pybind11 vs nanobind")
print("Focus: Real-time audio analysis with callback-based resampling")
print("=" * 80)

# ============================================================================
# Test 1: Simple API - Bulk Resampling (baseline)
# ============================================================================
print("\n" + "=" * 80)
print("TEST 1: Simple API - Bulk Resampling (Non-Real-Time)")
print("=" * 80)

test_configs = [
    # (samples, ratio, converter, description)
    (1024, 2.0, 'sinc_fastest', 'Tiny buffer (1024 samples, 2x upsampling, fastest)'),
    (4096, 2.0, 'sinc_medium', 'Small buffer (4096 samples, 2x upsampling, medium)'),
    (44100, 0.5, 'sinc_best', 'Large buffer (1 sec @ 44.1kHz, 0.5x downsampling, best)'),
    (512, 1.5, 'sinc_fastest', 'Real-time sized buffer (512 samples, 1.5x, fastest)'),
]

simple_api_results = []

for samples, ratio, converter, desc in test_configs:
    print(f"\n{desc}")
    print("-" * 80)
    
    # Generate test data
    np.random.seed(42)
    input_data = np.sin(2 * np.pi * 5 * np.arange(samples) / samples).astype(np.float32)
    
    # Warmup
    _ = sr_pb.resample(input_data, ratio, converter)
    _ = sr_nb.resample(input_data, ratio, converter)
    
    # Test pybind11
    iterations = 1000 if samples < 10000 else 100
    pb_times = []
    for _ in range(iterations):
        start = time.perf_counter()
        _ = sr_pb.resample(input_data, ratio, converter)
        pb_times.append(time.perf_counter() - start)
    
    # Test nanobind
    nb_times = []
    for _ in range(iterations):
        start = time.perf_counter()
        _ = sr_nb.resample(input_data, ratio, converter)
        nb_times.append(time.perf_counter() - start)
    
    pb_mean = statistics.mean(pb_times) * 1000  # ms
    pb_std = statistics.stdev(pb_times) * 1000  # ms
    nb_mean = statistics.mean(nb_times) * 1000  # ms
    nb_std = statistics.stdev(nb_times) * 1000  # ms
    speedup = pb_mean / nb_mean if nb_mean > 0 else 1.0
    
    print(f"  pybind11: {pb_mean:.4f} ± {pb_std:.4f} ms")
    print(f"  nanobind: {nb_mean:.4f} ± {nb_std:.4f} ms")
    print(f"  Speedup:  {speedup:.3f}x {'✓ FASTER' if speedup > 1.0 else '✗ SLOWER'}")
    
    simple_api_results.append({
        'desc': desc,
        'samples': samples,
        'pb_mean': pb_mean,
        'nb_mean': nb_mean,
        'speedup': speedup
    })

# ============================================================================
# Test 2: Full API - Streaming Resampling (Real-time simulation)
# ============================================================================
print("\n" + "=" * 80)
print("TEST 2: Full API - Streaming Resampling (Real-time Simulation)")
print("=" * 80)

streaming_configs = [
    # (chunk_size, chunks, ratio, converter, description)
    (512, 100, 2.0, 'sinc_fastest', 'Real-time audio chunks (512 samples/chunk, 100 chunks)'),
    (1024, 50, 1.5, 'sinc_medium', 'Medium chunks (1024 samples/chunk, 50 chunks)'),
    (256, 200, 0.5, 'sinc_fastest', 'Tiny chunks (256 samples/chunk, 200 chunks)'),
]

streaming_results = []

for chunk_size, chunks, ratio, converter, desc in streaming_configs:
    print(f"\n{desc}")
    print("-" * 80)
    
    # Generate streaming data
    np.random.seed(42)
    
    # Test pybind11 streaming
    resampler_pb = sr_pb.Resampler(converter, channels=1)
    pb_times = []
    for i in range(chunks):
        chunk = np.sin(2 * np.pi * 5 * np.arange(chunk_size) / chunk_size).astype(np.float32)
        start = time.perf_counter()
        _ = resampler_pb.process(chunk, ratio, end_of_input=(i == chunks-1))
        pb_times.append(time.perf_counter() - start)
    
    # Test nanobind streaming
    resampler_nb = sr_nb.Resampler(converter, channels=1)
    nb_times = []
    for i in range(chunks):
        chunk = np.sin(2 * np.pi * 5 * np.arange(chunk_size) / chunk_size).astype(np.float32)
        start = time.perf_counter()
        _ = resampler_nb.process(chunk, ratio, end_of_input=(i == chunks-1))
        nb_times.append(time.perf_counter() - start)
    
    pb_mean = statistics.mean(pb_times) * 1000  # ms
    pb_std = statistics.stdev(pb_times) * 1000  # ms
    nb_mean = statistics.mean(nb_times) * 1000  # ms
    nb_std = statistics.stdev(nb_times) * 1000  # ms
    speedup = pb_mean / nb_mean if nb_mean > 0 else 1.0
    
    # Calculate latency (time per chunk)
    print(f"  pybind11: {pb_mean:.4f} ± {pb_std:.4f} ms/chunk")
    print(f"  nanobind: {nb_mean:.4f} ± {nb_std:.4f} ms/chunk")
    print(f"  Speedup:  {speedup:.3f}x {'✓ FASTER' if speedup > 1.0 else '✗ SLOWER'}")
    
    # Calculate if real-time processing is possible
    # For 44.1kHz audio, 512 samples = ~11.6ms
    sample_duration_ms = (chunk_size / 44100) * 1000
    pb_realtime = "✓ Real-time capable" if pb_mean < sample_duration_ms else "✗ NOT real-time"
    nb_realtime = "✓ Real-time capable" if nb_mean < sample_duration_ms else "✗ NOT real-time"
    
    print(f"  Real-time @44.1kHz (need < {sample_duration_ms:.2f}ms):")
    print(f"    pybind11: {pb_realtime}")
    print(f"    nanobind: {nb_realtime}")
    
    streaming_results.append({
        'desc': desc,
        'chunk_size': chunk_size,
        'pb_mean': pb_mean,
        'nb_mean': nb_mean,
        'speedup': speedup
    })

# ============================================================================
# Test 3: Callback API - Real-time Audio Processing
# ============================================================================
print("\n" + "=" * 80)
print("TEST 3: Callback API - Real-time Audio Processing (CRITICAL TEST)")
print("=" * 80)

callback_configs = [
    # (callback_chunk, num_calls, ratio, converter, description)
    (512, 100, 2.0, 'sinc_fastest', 'Fast callback (512 samples, fastest converter)'),
    (1024, 50, 1.5, 'sinc_medium', 'Medium callback (1024 samples, medium converter)'),
    (256, 200, 0.5, 'sinc_fastest', 'Tiny callback (256 samples, fastest converter)'),
]

callback_results = []

for callback_chunk, num_calls, ratio, converter, desc in callback_configs:
    print(f"\n{desc}")
    print("-" * 80)
    
    # Create callback function
    call_count = [0]
    chunk_data = np.sin(2 * np.pi * 5 * np.arange(callback_chunk) / callback_chunk).astype(np.float32)
    
    def callback_pb():
        call_count[0] += 1
        return chunk_data.copy()
    
    def callback_nb():
        call_count[0] += 1
        return chunk_data.copy()
    
    # Test pybind11 callback
    call_count[0] = 0
    pb_times = []
    for i in range(10):  # Fewer iterations due to overhead
        resampler_pb = sr_pb.CallbackResampler(callback_pb, ratio, converter, channels=1)
        start = time.perf_counter()
        total_output = 0
        for _ in range(num_calls // 10):  # Process in batches
            output = resampler_pb.read(callback_chunk)
            total_output += len(output)
        pb_times.append(time.perf_counter() - start)
    
    # Test nanobind callback
    call_count[0] = 0
    nb_times = []
    for i in range(10):
        resampler_nb = sr_nb.CallbackResampler(callback_nb, ratio, converter, channels=1)
        start = time.perf_counter()
        total_output = 0
        for _ in range(num_calls // 10):
            output = resampler_nb.read(callback_chunk)
            total_output += len(output)
        nb_times.append(time.perf_counter() - start)
    
    pb_mean = statistics.mean(pb_times) * 1000  # ms
    pb_std = statistics.stdev(pb_times) * 1000  # ms
    nb_mean = statistics.mean(nb_times) * 1000  # ms
    nb_std = statistics.stdev(nb_times) * 1000  # ms
    speedup = pb_mean / nb_mean if nb_mean > 0 else 1.0
    
    print(f"  pybind11: {pb_mean:.4f} ± {pb_std:.4f} ms/batch")
    print(f"  nanobind: {nb_mean:.4f} ± {nb_std:.4f} ms/batch")
    print(f"  Speedup:  {speedup:.3f}x {'✓ FASTER' if speedup > 1.0 else '✗ SLOWER'}")
    
    # Callback overhead analysis
    pb_per_call = pb_mean / (num_calls // 10)
    nb_per_call = nb_mean / (num_calls // 10)
    print(f"  Latency per callback:")
    print(f"    pybind11: {pb_per_call:.4f} ms")
    print(f"    nanobind: {nb_per_call:.4f} ms")
    print(f"    Improvement: {pb_per_call - nb_per_call:.4f} ms ({speedup:.2f}x faster)")
    
    callback_results.append({
        'desc': desc,
        'callback_chunk': callback_chunk,
        'pb_mean': pb_mean,
        'nb_mean': nb_mean,
        'speedup': speedup,
        'pb_per_call': pb_per_call,
        'nb_per_call': nb_per_call
    })

# ============================================================================
# Test 4: Dtype Conversion Overhead
# ============================================================================
print("\n" + "=" * 80)
print("TEST 4: Dtype Conversion Overhead (float64 → float32)")
print("=" * 80)

dtype_test_size = 10000
iterations = 500

for dtype_name, dtype in [('float32', np.float32), ('float64', np.float64)]:
    print(f"\nInput dtype: {dtype_name}")
    print("-" * 80)
    
    np.random.seed(42)
    input_data = np.sin(2 * np.pi * 5 * np.arange(dtype_test_size) / dtype_test_size).astype(dtype)
    
    # Test pybind11
    pb_times = []
    for _ in range(iterations):
        start = time.perf_counter()
        _ = sr_pb.resample(input_data, 2.0, 'sinc_fastest')
        pb_times.append(time.perf_counter() - start)
    
    # Test nanobind
    nb_times = []
    for _ in range(iterations):
        start = time.perf_counter()
        _ = sr_nb.resample(input_data, 2.0, 'sinc_fastest')
        nb_times.append(time.perf_counter() - start)
    
    pb_mean = statistics.mean(pb_times) * 1000
    nb_mean = statistics.mean(nb_times) * 1000
    speedup = pb_mean / nb_mean if nb_mean > 0 else 1.0
    
    print(f"  pybind11: {pb_mean:.4f} ms")
    print(f"  nanobind: {nb_mean:.4f} ms")
    print(f"  Speedup:  {speedup:.3f}x")

# ============================================================================
# Test 5: Multi-channel Performance
# ============================================================================
print("\n" + "=" * 80)
print("TEST 5: Multi-channel Performance (Stereo Audio)")
print("=" * 80)

for channels in [1, 2]:
    print(f"\nChannels: {channels}")
    print("-" * 80)
    
    np.random.seed(42)
    if channels == 1:
        input_data = np.sin(2 * np.pi * 5 * np.arange(4096) / 4096).astype(np.float32)
    else:
        input_data = np.column_stack([
            np.sin(2 * np.pi * 5 * np.arange(4096) / 4096),
            np.sin(2 * np.pi * 7 * np.arange(4096) / 4096)
        ]).astype(np.float32)
    
    iterations = 500
    
    # Test pybind11
    pb_times = []
    for _ in range(iterations):
        start = time.perf_counter()
        _ = sr_pb.resample(input_data, 2.0, 'sinc_medium')
        pb_times.append(time.perf_counter() - start)
    
    # Test nanobind
    nb_times = []
    for _ in range(iterations):
        start = time.perf_counter()
        _ = sr_nb.resample(input_data, 2.0, 'sinc_medium')
        nb_times.append(time.perf_counter() - start)
    
    pb_mean = statistics.mean(pb_times) * 1000
    nb_mean = statistics.mean(nb_times) * 1000
    speedup = pb_mean / nb_mean if nb_mean > 0 else 1.0
    
    print(f"  pybind11: {pb_mean:.4f} ms")
    print(f"  nanobind: {nb_mean:.4f} ms")
    print(f"  Speedup:  {speedup:.3f}x")

# ============================================================================
# Summary
# ============================================================================
print("\n" + "=" * 80)
print("SUMMARY")
print("=" * 80)

print("\nSimple API (Bulk Resampling):")
for r in simple_api_results:
    print(f"  {r['samples']:>6} samples: {r['speedup']:.3f}x")
avg_simple = statistics.mean([r['speedup'] for r in simple_api_results])
print(f"  Average: {avg_simple:.3f}x")

print("\nStreaming API (Real-time Simulation):")
for r in streaming_results:
    print(f"  {r['chunk_size']:>6} samples: {r['speedup']:.3f}x")
avg_streaming = statistics.mean([r['speedup'] for r in streaming_results])
print(f"  Average: {avg_streaming:.3f}x")

print("\nCallback API (CRITICAL for real-time):")
for r in callback_results:
    print(f"  {r['callback_chunk']:>6} samples: {r['speedup']:.3f}x (latency reduction: {r['pb_per_call'] - r['nb_per_call']:.4f} ms/call)")
avg_callback = statistics.mean([r['speedup'] for r in callback_results])
print(f"  Average: {avg_callback:.3f}x")

print("\n" + "=" * 80)
print("OVERALL PERFORMANCE")
print("=" * 80)
overall_speedup = statistics.mean([avg_simple, avg_streaming, avg_callback])
print(f"Overall average speedup: {overall_speedup:.3f}x")

if overall_speedup >= 1.10:
    print(f"✓ TARGET MET: {overall_speedup:.1f}x speedup (>10% improvement)")
elif overall_speedup >= 1.05:
    print(f"~ CLOSE: {overall_speedup:.1f}x speedup (5-10% improvement)")
else:
    print(f"✗ TARGET NOT MET: {overall_speedup:.1f}x speedup (<5% improvement)")

print("\n" + "=" * 80)
print("CONCLUSION FOR REAL-TIME AUDIO")
print("=" * 80)

if avg_callback >= 1.05:
    print(f"✓ Nanobind provides {avg_callback:.2f}x speedup for callback-based real-time audio")
    print(f"  This translates to {((avg_callback - 1) * 100):.1f}% lower latency per callback")
    print(f"  RECOMMENDATION: Migrate to nanobind for improved real-time performance")
else:
    print(f"~ Nanobind provides {avg_callback:.2f}x performance (minimal difference)")
    print(f"  RECOMMENDATION: Both implementations suitable for real-time use")

print("=" * 80)
