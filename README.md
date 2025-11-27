# python-samplerate-ledfx
> **Note:** This is a fork of the original [python-samplerate](https://github.com/tuxu/python-samplerate) maintained by the [LedFx](https://github.com/LedFx) team.
>
> **Why this fork exists:**
> - The original python-samplerate project is sporadically active
> - We need Python 3.14 support with pre-built wheels on PyPI
> - We require the latest fixes and improvements from the main branch of python-samplerate
> - LedFx depends on python-samplerate and needs a reliable, up-to-date release
>
> All credit for python-samplerate goes to the original authors. This fork exists solely to provide maintained releases for projects that depend on python-samplerate.
>
> **Original project:** https://github.com/tuxu/python-samplerate  
> **This fork:** https://github.com/LedFx/python-samplerate-ledfx
[![image](https://img.shields.io/pypi/v/samplerate-ledfx.svg)](https://pypi.python.org/pypi/samplerate-ledfx)[![image](https://img.shields.io/pypi/l/samplerate-ledfx.svg)](https://pypi.python.org/pypi/samplerate)[![image](https://img.shields.io/pypi/wheel/samplerate-ledfx.svg)](https://pypi.python.org/pypi/samplerate-ledfx)[![image](https://img.shields.io/pypi/pyversions/samplerate-ledfx.svg)](https://pypi.python.org/pypi/samplerate-ledfx)[![Documentation Status](https://readthedocs.org/projects/python-samplerate/badge/?version=latest)](http://python-samplerate.readthedocs.io/en/latest/?badge=latest)

This is a wrapper around Erik de Castro Lopo's [libsamplerate](http://www.mega-nerd.com/libsamplerate/) (aka Secret Rabbit Code) for high-quality sample rate conversion.

It implements all three [APIs](http://www.mega-nerd.com/libsamplerate/api.html) available in [libsamplerate](http://www.mega-nerd.com/libsamplerate/):

-   **Simple API**: for resampling a large chunk of data with a single library call
-   **Full API**: for obtaining the resampled signal from successive chunks of data
-   **Callback API**: like Full API, but input samples are provided by a callback function

The [libsamplerate](http://www.mega-nerd.com/libsamplerate/) library is statically built together with the python bindings using [pybind11](https://github.com/pybind/pybind11/).

## Installation

> \$ pip install samplerate-ledfx

Binary wheels of [samplerate-ledfx](https://pypi.org/p/samplerate-ledfx) are available. A C++ 14 or above compiler is required to build the package.

## Usage

``` python
import numpy as np
import samplerate

# Synthesize data
fs = 1000.
t = np.arange(fs * 2) / fs
input_data = np.sin(2 * np.pi * 5 * t)

# Simple API
ratio = 1.5
converter = 'sinc_best'  # or 'sinc_fastest', ...
output_data_simple = samplerate.resample(input_data, ratio, converter)

# Full API
resampler = samplerate.Resampler(converter, channels=1)
output_data_full = resampler.process(input_data, ratio, end_of_input=True)

# The result is the same for both APIs.
assert np.allclose(output_data_simple, output_data_full)

# See `samplerate.CallbackResampler` for the Callback API, or
# `examples/play_modulation.py` for an example.
```

See `samplerate.resample`, `samplerate.Resampler`, and `samplerate.CallbackResampler` in the API documentation for details.

## Multi-threading and GIL Control

All resampling methods support a `release_gil` parameter that controls Python's Global Interpreter Lock (GIL) during resampling operations. This is useful for optimizing performance in different scenarios:

``` python
import samplerate

# Default: "auto" mode - releases GIL only for large data (>= 1000 frames)
# Balances single-threaded performance with multi-threading capability
output = samplerate.resample(input_data, ratio)

# Force GIL release - best for multi-threaded applications
# Allows other Python threads to run during resampling
output = samplerate.resample(input_data, ratio, release_gil=True)

# Disable GIL release - best for single-threaded applications with small data
# Avoids the ~1-5µs overhead of GIL release/acquire
output = samplerate.resample(input_data, ratio, release_gil=False)
```

The same parameter is available on `Resampler.process()` and `CallbackResampler.read()`:

``` python
resampler = samplerate.Resampler('sinc_best', channels=1)
output = resampler.process(input_data, ratio, release_gil=True)
```

## See also

-   [scikits.samplerate](https://pypi.python.org/pypi/scikits.samplerate) implements only the Simple API and uses [Cython](http://cython.org/) for extern calls. The resample function of scikits.samplerate and this package share the same function signature for compatiblity.
-   [resampy](https://github.com/bmcfee/resampy): sample rate conversion in Python + Cython.

## License

This project is licensed under the [MIT license](https://opensource.org/licenses/MIT).

As of version 0.1.9,
[libsamplerate](http://www.mega-nerd.com/libsamplerate/) is licensed under the [2-clause BSD license](https://opensource.org/licenses/BSD-2-Clause).
