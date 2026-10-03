import numpy as np
import pytest

import samplerate


@pytest.mark.parametrize("errnum", list(range(1, 25)))
def test_resampling_error(errnum):
    if 0 < errnum < 24:
        with pytest.raises(samplerate.ResamplingError):
            samplerate._internals.error_handler(errnum)
    elif errnum != 0:
        with pytest.raises(RuntimeError):
            samplerate._internals.error_handler(errnum)


def test_unknown_converter_type():
    with pytest.raises(ValueError):
        samplerate._internals.get_converter_type("super-downsampling")


def test_resample_zero_channel_input():
    data = np.zeros((16000, 0), dtype=np.float32)
    # does not produce an error, the output shape is (0, 0)
    # because the number of samples converted is zero
    with pytest.raises(ValueError):
        samplerate.resample(data, 0.5, "sinc_fastest")


def test_resampler_zero_channel_input():
    data = np.zeros((16000, 0), dtype=np.float32)
    # does not produce an error, the output shape is (0, 0)
    # because the number of samples converted is zero
    resampler = samplerate.Resampler("sinc_fastest", 1)
    with pytest.raises(ValueError):
        resampler.process(data, 0.5)


def test_resample_zero_len_input():
    data = np.zeros((0, 1), dtype=np.float32)
    # does not produce an error, the output shape is (0, 1)
    # same as the input
    samplerate.resample(data, 0.5, "sinc_fastest")


def test_resample_ndim_too_big():
    data = np.zeros((16000, 1, 1), dtype=np.float32)
    with pytest.raises(ValueError):
        # fails because the input has 3 dimensions
        samplerate.resample(data, 0.5, "sinc_fastest")


def test_resampler_ndim_too_big():
    data = np.zeros((16000, 1, 1), dtype=np.float32)
    resampler = samplerate.Resampler("sinc_fastest", 1)
    with pytest.raises(ValueError):
        # fails because the input has 3 dimensions
        resampler.process(data, 0.5)


def test_resampler_incorrect_channel_number():
    data = np.zeros((16000, 2), dtype=np.float32)
    resampler = samplerate.Resampler("sinc_fastest", 1)
    with pytest.raises(ValueError):
        # fails because we defined the converter for 1 channel
        resampler.process(data, 0.5)


def test_callback_resampler_ndim_too_big():
    data = np.zeros((16000, 1, 1), dtype=np.float32)

    def producer():
        yield data
        while True:
            yield None

    callback = lambda p=producer(): next(p)

    cb_resampler = samplerate.CallbackResampler(callback, 0.5, "sinc_fastest", 1)
    with pytest.raises(ValueError):
        # fails because the input has 3 dimensions
        cb_resampler.read(len(data))


def test_callback_resampler_incorrect_channel_number():
    data = np.zeros((16000, 2), dtype=np.float32)

    def producer():
        yield data
        while True:
            yield None

    callback = lambda p=producer(): next(p)

    cb_resampler = samplerate.CallbackResampler(callback, 0.5, "sinc_fastest", 1)
    with pytest.raises(ValueError):
        # fails because we defined the converter for 1 channel
        cb_resampler.read(len(data))


def test_callback_resampler_zero_channels():
    data = np.zeros((16000, 0), dtype=np.float32)

    def producer():
        yield data
        while True:
            yield None

    callback = lambda p=producer(): next(p)

    cb_resampler = samplerate.CallbackResampler(callback, 0.5, "sinc_fastest", 1)
    with pytest.raises(ValueError):
        # fails because we defined the converter for 1 channel
        cb_resampler.read(len(data))


def test_callback_resampler_callback_raises():
    def callback():
        raise KeyError("raised in callback")

    cb_resampler = samplerate.CallbackResampler(callback, 0.5, "sinc_fastest", 1)
    with pytest.raises(KeyError, match="raised in callback"):
        cb_resampler.read(100)


BAD_RATIOS = [0.0, -1.0, 1e-9, 1e9, float("inf"), float("nan")]


@pytest.mark.parametrize("ratio", BAD_RATIOS)
def test_resample_bad_ratio(ratio):
    # Checked before the output buffer is sized from the ratio: 1e9 used to
    # ask for terabytes (MemoryError), NaN for a negative size (ValueError).
    data = np.zeros(1000, dtype=np.float32)
    with pytest.raises(samplerate.ResamplingError):
        samplerate.resample(data, ratio, "sinc_fastest")


@pytest.mark.parametrize("ratio", BAD_RATIOS)
def test_resampler_bad_ratio(ratio):
    resampler = samplerate.Resampler("sinc_fastest", 1)
    with pytest.raises(samplerate.ResamplingError):
        resampler.process(np.zeros(1000, dtype=np.float32), ratio)
    with pytest.raises(samplerate.ResamplingError):
        resampler.set_ratio(ratio)


@pytest.mark.parametrize("ratio", BAD_RATIOS)
def test_callback_resampler_bad_ratio(ratio):
    # libsamplerate accepts NaN and returns NaN samples
    data = np.zeros(1000, dtype=np.float32)
    cb_resampler = samplerate.CallbackResampler(lambda: data, ratio, "sinc_fastest")
    with pytest.raises(samplerate.ResamplingError):
        cb_resampler.read(100)
    cb_resampler.ratio = 0.5
    with pytest.raises(samplerate.ResamplingError):
        cb_resampler.set_starting_ratio(ratio)


@pytest.mark.parametrize("converter_type", ["sinc_best", "sinc_medium", "sinc_fastest"])
def test_sinc_too_many_channels(converter_type):
    # libsamplerate's own message for this is "Channel count must be >= 1."
    with pytest.raises(samplerate.ResamplingError, match="at most 128 channels"):
        samplerate.Resampler(converter_type, 129)
    with pytest.raises(samplerate.ResamplingError, match="at most 128 channels"):
        samplerate.CallbackResampler(lambda: None, 0.5, converter_type, 129)


@pytest.mark.parametrize("channels", [0, -1])
@pytest.mark.parametrize("converter_type", ["sinc_best", "linear"])
def test_resampler_bad_channel_count(converter_type, channels):
    # libsamplerate asserts channels > 0 (libsamplerate#223): with NDEBUG it
    # accepted 0, and it reported -1 as a failed malloc.
    with pytest.raises(samplerate.ResamplingError, match="Channel count"):
        samplerate.Resampler(converter_type, channels)


def test_callback_resampler_zero_channel_count():
    with pytest.raises(samplerate.ResamplingError, match="Channel count"):
        samplerate.CallbackResampler(lambda: None, 0.5, "sinc_fastest", 0)
