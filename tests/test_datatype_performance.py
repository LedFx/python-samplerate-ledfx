import time
import numpy as np
import samplerate

def benchmark_resample(input_data, ratio=1.5, converter='sinc_fastest'):
    start_time = time.perf_counter()
    samplerate.resample(input_data, ratio, converter)
    end_time = time.perf_counter()
    return end_time - start_time

def test_datatype_performance():
    # Generate 1 second of audio at 44.1kHz
    fs = 44100
    duration = 15.0
    t = np.arange(fs * duration) / fs
    
    # Create float64 (default) and float32 arrays
    data_float64 = np.sin(2 * np.pi * 440 * t)
    data_float32 = data_float64.astype(np.float32)
    
    # Warmup
    benchmark_resample(data_float32)
    
    # Benchmark float32 (native)
    times_f32 = []
    for _ in range(10):
        times_f32.append(benchmark_resample(data_float32))
    avg_f32 = np.mean(times_f32)
    
    # Benchmark float64 (requires conversion)
    times_f64 = []
    for _ in range(10):
        times_f64.append(benchmark_resample(data_float64))
    avg_f64 = np.mean(times_f64)
    
    print(f"\nPerformance Comparison (1s audio, sinc_fastest):")
    print(f"float32 (native): {avg_f32*1000:.3f} ms")
    print(f"float64 (copy):   {avg_f64*1000:.3f} ms")
    print(f"Overhead:         {(avg_f64 - avg_f32)*1000:.3f} ms ({(avg_f64/avg_f32 - 1)*100:.1f}%)")
    
    # We expect float32 to be faster, but we won't fail the test if it isn't 
    # (machine noise can affect small benchmarks), just report it.

if __name__ == "__main__":
    test_datatype_performance()