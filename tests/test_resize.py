import numpy as np
import samplerate


def test_resize():
    np.random.seed(0)
    ratio = 0.9
    x = np.random.randn(167)
    # internally, the bindings will first prepare a buffer of size
    # ceil(167 * 0.9) = 151, which will be resized to 150
    y = samplerate.resample(x, 0.9)
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
        with samplerate.CallbackResampler(lambda: next(chunks), 0.9, "linear") as cb:
            y = cb.read(1000)  # more than available -> truncated
        _assert_owns_or_views_base(y)
        assert y.shape[0] < 1000
        np.testing.assert_array_equal(y.reshape(ref.shape), ref)
