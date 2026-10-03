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

# Tracks master: the newest release, 0.2.2, is from 2021. Since then master
# has SSE2 lrint() on x86-64 (faster, same output) and CMake fixes, among them
# a minimum version CMake 4 accepts. Renovate bumps the commit; move back to a
# tag once a release includes these.
set(LIBSAMPLERATE_COMMIT 0844c208f683527c08ea8a80acc13b398aa9c8bf) # master
FetchContent_Declare(
  libsamplerate
  GIT_REPOSITORY https://github.com/libsndfile/libsamplerate
  GIT_TAG ${LIBSAMPLERATE_COMMIT}
  # Fixes from upstream PRs that are not merged yet; see each patch's header.
  PATCH_COMMAND ${CMAKE_COMMAND} -DPATCH_DIR=${CMAKE_CURRENT_SOURCE_DIR}/patches
                -P ${CMAKE_CURRENT_SOURCE_DIR}/apply_patches.cmake
)
string(SUBSTRING "${LIBSAMPLERATE_COMMIT}" 0 7 _libsamplerate_short)
set(LIBSAMPLERATE_VERSION "0.2.2+git.${_libsamplerate_short}" CACHE STRING "libsamplerate version" FORCE)

FetchContent_MakeAvailable(libsamplerate)
