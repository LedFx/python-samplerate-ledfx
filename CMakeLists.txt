# https://stackoverflow.com/questions/51907755/building-a-pybind11-module-with-cpp-and-cuda-sources-using-cmake

cmake_minimum_required(VERSION 3.15)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

message(STATUS "Found Python prefix ${PYTHON_PREFIX}")
list(PREPEND CMAKE_PREFIX_PATH "${PYTHON_PREFIX}")

project(python-samplerate)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

cmake_policy(SET CMP0148 NEW)

# adds the external dependencies
add_subdirectory(external)

pybind11_add_module(python-samplerate src/samplerate.cpp)

target_include_directories(python-samplerate PRIVATE ./external/libsamplerate/include)

if(MSVC)
    target_compile_options(python-samplerate PRIVATE /EHsc /MP /bigobj /O2)
    set(CMAKE_EXE_LINKER_FLAGS /MANIFEST:NO)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
    CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR
    (CMAKE_CXX_COMPILER_ID MATCHES "Intel" AND NOT WIN32))
    target_compile_options(python-samplerate PRIVATE -std=c++14 -O3 -Wall -Wextra -fPIC)
endif()

### Final target setup - must be before compile_definitions so LTO generator expression works
set_target_properties(
    python-samplerate
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME "samplerate"
        LINKER_LANGUAGE C
        INTERPROCEDURAL_OPTIMIZATION TRUE
    )

### stick the package and libsamplerate version into the module
target_compile_definitions(python-samplerate
    PUBLIC LIBSAMPLERATE_VERSION="${LIBSAMPLERATE_VERSION}"
    PRIVATE $<$<BOOL:${PACKAGE_VERSION_INFO}>:VERSION_INFO="${PACKAGE_VERSION_INFO}">
    # Build information for debugging
    PRIVATE BUILD_TYPE="$<CONFIG>"
    PRIVATE COMPILER_ID="${CMAKE_CXX_COMPILER_ID}"
    PRIVATE COMPILER_VERSION="${CMAKE_CXX_COMPILER_VERSION}"
    PRIVATE CMAKE_VERSION="${CMAKE_VERSION}"
    PRIVATE TARGET_ARCH="${CMAKE_SYSTEM_PROCESSOR}"
    PRIVATE TARGET_OS="${CMAKE_SYSTEM_NAME}"
    PUBLIC PYBIND11_VERSION_INFO="${PYBIND11_VERSION_INFO}" 
    PRIVATE LTO_ENABLED=$<BOOL:$<TARGET_PROPERTY:python-samplerate,INTERPROCEDURAL_OPTIMIZATION>>
)

target_link_libraries(python-samplerate PUBLIC samplerate)
