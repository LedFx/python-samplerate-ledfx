import numpy as np
import pytest
import samplerate


def test_resize():
    np.random.seed(0)
    ratio = 0.9
    x = np.random.randn(167)
    # internally, the bindings will first prepare a buffer of size
    # ceil(167 * 0.9) = 151, which will be resized to 150
    y = samplerate.resample(x, 0.9)
    assert y.shape[0] == 150


@pytest.mark.parametrize("converter", ["sinc_best", "sinc_fastest", "linear"])
def test_process_flush_at_high_ratio(converter):
    # end_of_input flushes the input the converter held back, about
    # ratio x its filter half-length frames: more than any fixed headroom.
    ratio = 256
    x = np.random.rand(512).astype(np.float32)
    resampler = samplerate.Resampler(converter)
    total = sum(len(resampler.process(x, ratio)) for _ in range(5))
    total += len(resampler.process(x[:0], ratio, end_of_input=True))
    assert total == pytest.approx(5 * len(x) * ratio, abs=ratio)


def test_process_after_ratio_change():
    # After a ratio change libsamplerate ramps from the previous ratio, so a
    # call can produce far more than ceil(len(x) * new_ratio).
    resampler = samplerate.Resampler("linear")
    resampler.process(np.zeros(1000, np.float32), 4.0)
    x = np.random.rand(200_000).astype(np.float32)
    y = resampler.process(x, 0.25, end_of_input=True)
    assert len(y) > len(x) * 0.25
