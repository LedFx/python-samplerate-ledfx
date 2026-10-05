# samplerate-ledfx

[![PyPI](https://img.shields.io/pypi/v/samplerate-ledfx.svg)](https://pypi.org/project/samplerate-ledfx/)
[![Python versions](https://img.shields.io/pypi/pyversions/samplerate-ledfx.svg)](https://pypi.org/project/samplerate-ledfx/)
[![License](https://img.shields.io/pypi/l/samplerate-ledfx.svg)](https://github.com/LedFx/samplerate-ledfx/blob/main/LICENSE.rst)
[![CI](https://github.com/LedFx/samplerate-ledfx/actions/workflows/ci.yml/badge.svg)](https://github.com/LedFx/samplerate-ledfx/actions/workflows/ci.yml)

Python bindings for [libsamplerate](https://libsndfile.github.io/libsamplerate/)
(Secret Rabbit Code), Erik de Castro Lopo's high-quality sample rate converter,
built with [pybind11](https://github.com/pybind/pybind11) and NumPy.

This is the [LedFx](https://github.com/LedFx) team's maintained fork of
[tuxu/python-samplerate](https://github.com/tuxu/python-samplerate). LedFx
depends on it, and upstream releases rarely, so the fork provides:

- Prebuilt wheels for CPython 3.11–3.15 on Linux (x86_64, aarch64), macOS
  (Intel, Apple Silicon) and Windows (x64).
- Optional GIL release during resampling, for multi-threaded use.
- Fixes for crashes and memory errors in the bindings and in libsamplerate
  itself (see [CHANGELOG.md](https://github.com/LedFx/samplerate-ledfx/blob/main/CHANGELOG.md)).

All credit for python-samplerate goes to its original authors.

## Installation

```sh
pip install samplerate-ledfx
```

The module is imported as `samplerate`, the same name as upstream's
[`samplerate`](https://pypi.org/project/samplerate/) package, so install one or
the other: `pip uninstall samplerate` first if you are switching.

Building from source (where no wheel fits) needs a C++14 compiler and network
access: CMake fetches libsamplerate and pybind11 at build time.

## Usage

The three [libsamplerate APIs](https://libsndfile.github.io/libsamplerate/api.html)
are all available:

- **Simple**: `resample()` converts a whole signal in one call.
- **Full**: `Resampler.process()` converts a stream chunk by chunk, and the
  ratio can change between chunks.
- **Callback**: `CallbackResampler.read()` pulls input from a function you
  provide.

```python
import numpy as np
import samplerate

# Synthesize data
fs = 1000.0
t = np.arange(fs * 2) / fs
input_data = np.sin(2 * np.pi * 5 * t)

# Simple API
ratio = 1.5
converter = "sinc_best"  # or "sinc_medium", "sinc_fastest", "zero_order_hold", "linear"
output_data_simple = samplerate.resample(input_data, ratio, converter)

# Full API
resampler = samplerate.Resampler(converter, channels=1)
output_data_full = resampler.process(input_data, ratio, end_of_input=True)

# The result is the same for both APIs.
assert np.allclose(output_data_simple, output_data_full)


# Callback API: the callback returns the next chunk of input, or None at the
# end of the stream. It is called again after that, so keep returning None.
def producer():
    for _ in range(10):
        yield np.random.uniform(-1, 1, 1024).astype(np.float32)


data_iter = producer()
resampler = samplerate.CallbackResampler(lambda: next(data_iter, None), ratio, converter)
output_chunks = []
while True:
    chunk = resampler.read(512)  # may return fewer frames than asked for
    if chunk.shape[0] == 0:
        break
    output_chunks.append(chunk)
```

[`examples/play_modulation.py`](https://github.com/LedFx/samplerate-ledfx/blob/main/examples/play_modulation.py) uses the callback
API to play a frequency-modulated tone. Type hints ship with the package.

### Input data

- Multi-channel data is a 2-D array of shape `(frames, channels)`; a single
  channel can also be a 1-D array.
- Data is converted to 32-bit float. Integer samples are cast, not scaled:
  divide 16-bit PCM by 32768 yourself to get the usual -1.0 to 1.0 range.
- The ratio (output rate / input rate) must be between 1/256 and 256;
  anything else raises `samplerate.ResamplingError`.
- The sinc converters handle at most 128 channels per `Resampler` or
  `CallbackResampler`. `resample()` takes any number: it converts wider input
  in groups of channels.

## Performance tips

1. **Pass `np.float32`.** libsamplerate works on 32-bit floats; `float64` (NumPy's
   default) or integer input is copied and cast first.
2. **Pass C-contiguous arrays.** Non-contiguous input, such as a column slice,
   is copied too.
3. **Tune the GIL threshold** if you process many small chunks from several
   threads (see below).

```python
data = np.zeros(1000, dtype=np.float32)  # no copy
samplerate.resample(data, 1.5)

data = np.zeros(1000)  # float64: copied and cast first
samplerate.resample(data, 1.5)
```

## Multi-threading and the GIL

`resample()`, `Resampler.process()` and `CallbackResampler.read()` take a
`release_gil` argument:

```python
# Default ("auto", or None): release the GIL only for inputs of at least
# 1000 frames, where the ~1-5 µs release/re-acquire cost is negligible.
output = samplerate.resample(input_data, ratio)

# Always release it: other Python threads run while this one resamples.
output = samplerate.resample(input_data, ratio, release_gil=True)

# Never release it: lowest overhead for single-threaded, small inputs.
output = samplerate.resample(input_data, ratio, release_gil=False)

# Change the "auto" threshold (in frames).
samplerate.set_gil_release_threshold(100)
```

Separate `Resampler` and `CallbackResampler` objects can run in parallel
threads. A single object holds stream state, so using it from two threads at
once raises `RuntimeError` instead of corrupting that state. While the GIL is
released the input array is read without it: don't modify it from another
thread during the call.

`samplerate.get_build_info()` reports the versions, compiler and settings the
module was built with, for bug reports.

## Development

```sh
uv sync --group test --group dev         # builds the extension
uv run pytest -m "not perf"              # what CI runs; drop -m for timing benchmarks
uv run prek run --all-files              # lint: ruff, actionlint, zizmor, ...
```

PR titles follow [Conventional Commits](https://www.conventionalcommits.org/);
releases are cut by release-please. See [MAINTAINING.md](https://github.com/LedFx/samplerate-ledfx/blob/main/MAINTAINING.md) for
releases, dependency updates, the libsamplerate patches and upstream syncs.

## See also

- [scikits.samplerate](https://pypi.org/project/scikits.samplerate/) implements
  only the Simple API, with Cython. Positional calls to its `resample`
  (`resample(input, ratio, "sinc_best")`) work unchanged here, but its keyword
  names differ (`r` and `type` rather than `ratio` and `converter_type`).
- [resampy](https://github.com/bmcfee/resampy): sample rate conversion in
  Python and Cython.

## License

This project is licensed under the [MIT license](https://github.com/LedFx/samplerate-ledfx/blob/main/LICENSE.rst).
[libsamplerate](https://libsndfile.github.io/libsamplerate/) is licensed under
the [2-clause BSD license](https://opensource.org/licenses/BSD-2-Clause).
