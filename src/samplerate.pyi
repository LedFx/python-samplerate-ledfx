from collections.abc import Callable
from types import ModuleType
from typing import ClassVar, Self, TypeAlias, TypedDict, type_check_only

import numpy as np
import numpy.typing as npt

__version__: str
__libsamplerate_version__: str

# pybind11 submodules; the classes and functions below are also reachable
# through them (samplerate.converters.Resampler, ...).
converters: ModuleType
exceptions: ModuleType

@type_check_only
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

# A pybind11 enum, not an enum.Enum.
class ConverterType:
    sinc_best: ClassVar[ConverterType]
    sinc_medium: ClassVar[ConverterType]
    sinc_fastest: ClassVar[ConverterType]
    zero_order_hold: ClassVar[ConverterType]
    linear: ClassVar[ConverterType]
    __members__: ClassVar[dict[str, ConverterType]]
    def __init__(self, value: int) -> None: ...
    def __int__(self) -> int: ...
    def __index__(self) -> int: ...
    @property
    def name(self) -> str: ...
    @property
    def value(self) -> int: ...

class ResamplingError(RuntimeError): ...

_Converter: TypeAlias = ConverterType | str | int
_ReleaseGil: TypeAlias = bool | str | None
_Input = npt.ArrayLike

def set_gil_release_threshold(threshold: int) -> None: ...
def get_gil_release_threshold() -> int: ...
def get_build_info() -> BuildInfo: ...
def resample(
    input: _Input,
    ratio: float,
    converter_type: _Converter = "sinc_best",
    verbose: bool = False,
    release_gil: _ReleaseGil = None,
) -> npt.NDArray[np.float32]: ...

class Resampler:
    @property
    def converter_type(self) -> int: ...
    @property
    def channels(self) -> int: ...
    def __init__(
        self,
        converter_type: _Converter = "sinc_best",
        channels: int = 1,
    ) -> None: ...
    def process(
        self,
        input: _Input,
        ratio: float,
        end_of_input: bool = False,
        release_gil: _ReleaseGil = None,
    ) -> npt.NDArray[np.float32]: ...
    def reset(self) -> None: ...
    def set_ratio(self, new_ratio: float) -> None: ...
    def clone(self) -> Resampler: ...

class CallbackResampler:
    ratio: float
    @property
    def converter_type(self) -> int: ...
    @property
    def channels(self) -> int: ...
    def __init__(
        self,
        callback: Callable[[], _Input | None],
        ratio: float,
        converter_type: _Converter = "sinc_best",
        channels: int = 1,
    ) -> None: ...
    def read(
        self,
        num_frames: int,
        release_gil: _ReleaseGil = None,
    ) -> npt.NDArray[np.float32]: ...
    def reset(self) -> None: ...
    def set_starting_ratio(self, new_ratio: float) -> None: ...
    def clone(self) -> CallbackResampler: ...
    def __enter__(self) -> Self: ...
    def __exit__(self, exc_type: object, exc: object, exc_tb: object) -> None: ...
