# https://stackoverflow.com/questions/51907755/building-a-pybind11-module-with-cpp-and-cuda-sources-using-cmake

cmake_minimum_required(VERSION 3.15)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Find Python before setting up the project
find_package(Python COMPONENTS Interpreter Development REQUIRED)

message(STATUS "Found Python prefix ${PYTHON_PREFIX}")
list(PREPEND CMAKE_PREFIX_PATH "${PYTHON_PREFIX}")

project(python-samplerate)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

cmake_policy(SET CMP0148 NEW)

# adds the external dependencies
add_subdirectory(external)

# Option to build nanobind version (default OFF to maintain compatibility)
option(BUILD_NANOBIND "Build nanobind version in addition to pybind11" OFF)

# Build pybind11 version (default)
pybind11_add_module(python-samplerate src/samplerate.cpp)

target_include_directories(python-samplerate PRIVATE ./external/libsamplerate/include)

if(MSVC)
    target_compile_options(python-samplerate PRIVATE /EHsc /MP /bigobj)
    set(CMAKE_EXE_LINKER_FLAGS /MANIFEST:NO)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
    CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR
    (CMAKE_CXX_COMPILER_ID MATCHES "Intel" AND NOT WIN32))
    target_compile_options(python-samplerate PRIVATE -std=c++14 -O3 -Wall -Wextra -fPIC)
endif()

### stick the package and libsamplerate version into the module
target_compile_definitions(python-samplerate
    PUBLIC LIBSAMPLERATE_VERSION="${LIBSAMPLERATE_VERSION}"
    PRIVATE $<$<BOOL:${PACKAGE_VERSION_INFO}>:VERSION_INFO="${PACKAGE_VERSION_INFO}">
)

### Final target setup
set_target_properties(
    python-samplerate
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME "samplerate"
        LINKER_LANGUAGE C
    )

target_link_libraries(python-samplerate PUBLIC samplerate)

# Build nanobind version if requested
if(BUILD_NANOBIND)
    nanobind_add_module(python-samplerate-nb src/samplerate_nb.cpp)
    
    target_include_directories(python-samplerate-nb PRIVATE ./external/libsamplerate/include)
    
    if(MSVC)
        target_compile_options(python-samplerate-nb PRIVATE /EHsc /MP /bigobj)
    endif()
    
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
        CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR
        (CMAKE_CXX_COMPILER_ID MATCHES "Intel" AND NOT WIN32))
        target_compile_options(python-samplerate-nb PRIVATE -std=c++17 -O3 -Wall -Wextra -fPIC)
    endif()
    
    ### stick the package and libsamplerate version into the module
    target_compile_definitions(python-samplerate-nb
        PUBLIC LIBSAMPLERATE_VERSION="${LIBSAMPLERATE_VERSION}"
        PRIVATE $<$<BOOL:${PACKAGE_VERSION_INFO}>:VERSION_INFO="${PACKAGE_VERSION_INFO}">
    )
    
    ### Final target setup
    set_target_properties(
        python-samplerate-nb
        PROPERTIES
            PREFIX ""
            OUTPUT_NAME "samplerate"
            LINKER_LANGUAGE C
        )
    
    target_link_libraries(python-samplerate-nb PUBLIC samplerate)
endif()

