import gc

import numpy as np
import pytest

import samplerate


def test_callback_resampler_clone_outlives_original():
    x = np.arange(4096, dtype=np.float32)
    original = samplerate.CallbackResampler(lambda: x.copy(), 1.0, "linear")
    original.read(100)  # libsamplerate now points into original's buffer
    clone = original.clone()
    del original
    gc.collect()
    junk = [np.full(4096, -1.0, np.float32) for _ in range(200)]  # reuse freed memory
    y = clone.read(1000)
    assert y.min() >= 0


def test_reentrant_use_raises():
    # The same guard stops two threads driving one resampler while the GIL is
    # released; re-entering from the callback triggers it deterministically.
    def callback():
        resampler.read(10)
        return np.ones(256, np.float32)

    resampler = samplerate.CallbackResampler(callback, 1.0, "linear")
    with pytest.raises(RuntimeError, match="already in use"):
        resampler.read(10)
