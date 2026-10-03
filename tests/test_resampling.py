import numpy as np
import pytest

import samplerate


def make_tone(freq, sr, duration):
    t = np.arange(int(sr * duration), dtype=np.float64) / sr
    return np.sin(2 * np.pi * freq * t), t


def window(signal, n_win):
    """window the signal at beginning and end with window of size n_win/2"""

    win = np.hanning(2 * n_win)

    sig_copy = signal.copy()

    sig_copy[:n_win] *= win[:n_win]
    sig_copy[-n_win:] *= win[-n_win:]

    return sig_copy


def make_sweep(T, fs, f_lo=0.0, f_hi=None, fade=None, ascending=False):
    """
    Exponential sine sweep

    Parameters
    ----------
    T: float
        length in seconds
    fs:
        sampling frequency
    f_lo: float
        lowest frequency in fraction of fs (default 0)
    f_hi: float
        lowest frequency in fraction of fs (default 1)
    fade: float, optional
        length of fade in and out in seconds (default 0)
    ascending: bool, optional
    """

    if f_hi is None:
        f_hi = fs / 2
    elif f_hi < 0.0:
        f_hi = fs / 2 + f_hi
    elif f_hi > fs / 2:
        f_hi = fs / 2

    if f_lo < 1.0:
        f_lo = 1.0

    if f_lo > f_hi:
        raise ValueError("Error: need 0. <= f_lo < f_hi <= fs/2")

    Ts = 1.0 / fs  # Sampling period in [s]
    N = np.floor(T / Ts)  # number of samples
    n = np.arange(0, N, dtype="float64")  # Sample index

    om1 = 2 * np.pi * f_lo
    om2 = 2 * np.pi * f_hi

    sweep = np.sin(
        om1 * N * Ts / np.log(om2 / om1) * (np.exp(n / N * np.log(om2 / om1)) - 1)
    )

    if not ascending:
        sweep = sweep[::-1]

    if fade is not None and fade > 0.0:
        sweep = window(sweep, int(fs * fade))

    return sweep, np.arange(sweep.shape[0]) / fs


@pytest.mark.parametrize("sr_orig,sr_new", [(44100, 22050), (22050, 44100)])
@pytest.mark.parametrize(
    "fil,rms",
    [
        (samplerate.ConverterType.sinc_best, 1e-6),
        (samplerate.ConverterType.sinc_medium, 1e-5),
        (samplerate.ConverterType.sinc_fastest, 1e-4),
    ],
)
def test_quality_sine(sr_orig, sr_new, fil, rms):
    FREQ = 512.0
    DURATION = 2.0

    x, _ = make_tone(FREQ, sr_orig, DURATION)
    y, _ = make_tone(FREQ, sr_new, DURATION)
    y_pred = samplerate.resample(x, sr_new / sr_orig, fil)

    idx = slice(sr_new // 2, -sr_new // 2)

    err = np.mean(np.abs(y[idx] - y_pred[idx]))
    assert err <= rms, "{:g} > {:g}".format(err, rms)


@pytest.mark.parametrize("sr_orig,sr_new", [(44100, 22050), (22050, 44100)])
@pytest.mark.parametrize(
    "fil,rms",
    [
        (samplerate.ConverterType.sinc_best, 1e-6),
        (samplerate.ConverterType.sinc_medium, 1e-5),
        (samplerate.ConverterType.sinc_fastest, 1e-4),
    ],
)
def test_quality_sweep(sr_orig, sr_new, fil, rms):
    FREQ = 8000
    DURATION = 5.0

    x, _ = make_sweep(DURATION, sr_orig, 0.0, FREQ, fade=0.1)
    y, _ = make_sweep(DURATION, sr_new, 0.0, FREQ, fade=0.1)

    x = x[::-1]
    y = y[::-1]

    y_pred = samplerate.resample(x, sr_new / sr_orig, fil)

    idx = slice(sr_new // 2, -sr_new // 2)

    err = np.mean(np.abs(y[idx] - y_pred[idx]))

    assert err <= rms, "{:g} > {:g}".format(err, rms)


@pytest.mark.parametrize("converter_type", ["sinc_fastest", "linear"])
@pytest.mark.parametrize("num_channels", [128, 129, 300])
def test_resample_many_channels(converter_type, num_channels):
    # The sinc converters take at most 128 channels; resample() converts wider
    # input in groups of channels (tuxu/python-samplerate#40).
    rng = np.random.default_rng(0)
    data = rng.standard_normal((1000, num_channels)).astype(np.float32)
    output = samplerate.resample(data, 0.5, converter_type)
    expected = np.stack(
        [
            samplerate.resample(np.ascontiguousarray(data[:, ch]), 0.5, converter_type)
            for ch in range(num_channels)
        ],
        axis=1,
    )
    assert output.shape == expected.shape
    np.testing.assert_array_equal(output, expected)


@pytest.mark.parametrize("num_channels", [1, 2])
def test_linear_single_first_frame_stays_in_bounds(num_channels):
    # libsamplerate's linear converter read the frame *before* the input
    # buffer when its first call got one frame (libsamplerate#234, patched in
    # external/patches). Put a sentinel there: the input is a view one frame
    # into a larger array, so the read lands on it instead of other memory.
    backing = np.full((2, num_channels), 1e6, dtype=np.float32)
    backing[1] = 0.5
    frame = backing[1:]
    if num_channels == 1:
        frame = frame[:, 0]
    assert not frame.flags.owndata  # a view: no copy hides the read

    resampler = samplerate.Resampler("linear", num_channels)
    output = resampler.process(frame, 1.5)
    np.testing.assert_array_equal(output, np.full_like(output, 0.5))
