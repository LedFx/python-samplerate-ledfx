import numpy as np
import pytest

import samplerate


def test_resize():
    np.random.seed(0)
    ratio = 0.9
    x = np.random.randn(167)
    # internally, the bindings will first prepare a buffer of size
    # ceil(167 * 0.9) = 151, which will be resized to 150
    y = samplerate.resample(x, ratio)
    assert y.shape[0] == 150


def _assert_owns_or_views_base(y):
    # A truncated output is a view; its data must live inside its base,
    # otherwise it points at freed memory.
    assert y.base is None or np.shares_memory(y, y.base)


def test_truncated_outputs_are_valid_views():
    x = np.random.randn(167).astype(np.float32)
    ref = samplerate.resample(x, 0.9, "linear")
    _assert_owns_or_views_base(ref)

    r = samplerate.Resampler("linear")
    y = r.process(x, 0.9, end_of_input=True)
    _assert_owns_or_views_base(y)
    np.testing.assert_array_equal(y, ref)

    for data in (x, x[:, None]):  # 1-D mono takes a separate code path
        chunks = iter([data, None])
        with samplerate.CallbackResampler(chunks.__next__, 0.9, "linear") as cb:
            y = cb.read(1000)  # more than available -> truncated
        _assert_owns_or_views_base(y)
        assert y.shape[0] < 1000
        np.testing.assert_array_equal(y.reshape(ref.shape), ref)


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
