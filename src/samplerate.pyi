from typing import Optional, Union, Callable, Iterator, Tuple, overload, TypedDict
import numpy as np
import numpy.typing as npt

class BuildInfo(TypedDict):
    version: str
    libsamplerate_version: str
    build_type: str
    compiler_id: str
    compiler_version: str
    cmake_version: str
    target_arch: str
    target_os: str
    pybind11_version: str
    cpp_standard: str
    lto_enabled: bool
    pointer_size_bits: int
    float_size_bytes: int
    gil_release_threshold: int

class ConverterType:
    sinc_best: int
    sinc_medium: int
    sinc_fastest: int
    zero_order_hold: int
    linear: int

class ResamplingError(RuntimeError): ...

def set_gil_release_threshold(threshold: int) -> None: ...
def get_gil_release_threshold() -> int: ...
def get_build_info() -> BuildInfo: ...

def resample(
    input_data: npt.NDArray[np.float32],
    ratio: float,
    converter_type: Union[ConverterType, str, int] = "sinc_best",
    verbose: bool = False,
    release_gil: Optional[Union[bool, str]] = None,
) -> npt.NDArray[np.float32]: ...

class Resampler:
    converter_type: int
    channels: int
    def __init__(
        self,
        converter_type: Union[ConverterType, str, int] = "sinc_best",
        channels: int = 1,
    ) -> None: ...
    def process(
        self,
        input_data: npt.NDArray[np.float32],
        ratio: float,
        end_of_input: bool = False,
        release_gil: Optional[Union[bool, str]] = None,
    ) -> npt.NDArray[np.float32]: ...
    def reset(self) -> None: ...
    def set_ratio(self, new_ratio: float) -> None: ...
    def clone(self) -> "Resampler": ...

class CallbackResampler:
    ratio: float
    converter_type: int
    channels: int
    def __init__(
        self,
        callback: Callable[[], Optional[npt.NDArray[np.float32]]],
        ratio: float,
        converter_type: Union[ConverterType, str, int] = "sinc_best",
        channels: int = 1,
    ) -> None: ...
    def read(
        self,
        num_frames: int,
        release_gil: Optional[Union[bool, str]] = None,
    ) -> npt.NDArray[np.float32]: ...
    def reset(self) -> None: ...
    def set_starting_ratio(self, new_ratio: float) -> None: ...
    def clone(self) -> "CallbackResampler": ...
    def __enter__(self) -> "CallbackResampler": ...
    def __exit__(self, exc_type, exc, exc_tb) -> None: ...
