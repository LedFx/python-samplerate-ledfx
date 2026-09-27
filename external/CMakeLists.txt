include(FetchContent)
# Set pybind11 Python finding policy to use new FindPython
set(PYBIND11_FINDPYTHON ON CACHE BOOL "Use FindPython instead of FindPythonInterp")

# pybind11
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11
  GIT_TAG 97bf890db679505a14dfe547a5e77bb2bd05dc90 # 3.1.0
)
## Change this if you update pybind11 version
set(PYBIND11_VERSION_INFO "3.1.0" CACHE STRING "pybind11 version")  # <-- update pybind11 version here


FetchContent_MakeAvailable(pybind11)


# libsamplerate
set(BUILD_TESTING OFF CACHE BOOL "Disable libsamplerate test build")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

FetchContent_Declare(
  libsamplerate
  GIT_REPOSITORY https://github.com/libsndfile/libsamplerate
  GIT_TAG c96f5e3de9c4488f4e6c97f59f5245f22fda22f7 # 0.2.2
)
set(LIBSAMPLERATE_VERSION 0.2.2 CACHE STRING PUBLIC)  # <-- update libsamplerate version here

FetchContent_MakeAvailable(libsamplerate)
