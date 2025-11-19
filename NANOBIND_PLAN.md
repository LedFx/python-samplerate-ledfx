# Nanobind Migration Plan

## Overview
This document outlines the plan for migrating the python-samplerate-ledfx bindings from pybind11 to nanobind 2.9.2. The migration will create a new nanobind implementation that can be imported as `samplerate-nb` while maintaining the existing pybind11 bindings for regression testing.

## Goals
1. Create a drop-in replacement for the existing pybind11 bindings
2. Maintain API compatibility (seamless consumer experience)
3. Leverage nanobind's performance improvements (smaller binaries, faster compilation)
4. Enable comprehensive regression testing against pybind11 baseline
5. Development name: `samplerate-nb`, internal module: `samplerate` for easy migration

## Key Differences: pybind11 vs nanobind 2.9.2

### Philosophy
- **pybind11**: Broad feature coverage, aims to bind all of C++
- **nanobind**: Focused on common use cases, optimized for efficiency and simplicity

### Performance Benefits
- **Binary size**: ~5x smaller
- **Compile time**: ~4x faster
- **Runtime overhead**: ~10x reduction
- **Memory footprint**: ~2.3x reduction per wrapped object

### Technical Requirements
- **Minimum C++**: C++17 (vs C++14 for pybind11)
- **Minimum Python**: 3.8+
- **CMake**: 3.15+

### API Changes
- Very similar syntax to pybind11 for most use cases
- Some fringe features removed/changed
- Better stub generation with NDArray types
- Improved multi-threading support with localized locking

## Migration Phases

### Phase 1: Infrastructure Setup ✅
**Status**: Ready to begin

**Tasks**:
1. ✅ Explore repository structure
2. ✅ Understand existing pybind11 bindings
3. ✅ Build and test baseline (87 tests passing)
4. ✅ Research nanobind 2.9.2 features
5. Create NANOBIND_PLAN.md document
6. Update `.gitignore` for compiled extensions

**Deliverables**:
- Working baseline with all tests passing
- Comprehensive understanding of codebase
- Migration plan document

---

### Phase 2: Build System Configuration
**Status**: Not started

**Tasks**:
1. Update `external/CMakeLists.txt` to fetch nanobind
2. Create new `CMakeLists_nb.txt` for nanobind module
3. Update `setup.py` to support dual builds (both pybind11 and nanobind)
4. Configure nanobind module to output as `samplerate` (internal) but be importable as `samplerate-nb`
5. Test basic build infrastructure

**Deliverables**:
- CMake configuration for nanobind
- Build system supporting both binding libraries
- Empty nanobind module that compiles successfully

**Technical Notes**:
- Use FetchContent to get nanobind (similar to pybind11)
- nanobind_add_module() replaces pybind11_add_module()
- Separate build targets: `python-samplerate` (pybind11) and `python-samplerate-nb` (nanobind)

---

### Phase 3: Core Bindings Implementation
**Status**: Not started

**Tasks**:
1. Create `src/samplerate_nb.cpp` (new file, do not modify existing)
2. Port header includes from pybind11 to nanobind
3. Implement basic module structure
4. Port `ConverterType` enum
5. Port `ResamplingException` custom exception
6. Implement `get_converter_type()` helper function
7. Implement `error_handler()` function

**Deliverables**:
- Working module skeleton with basic types
- Exception handling matching pybind11 behavior
- Helper functions operational

**API Mapping**:
```cpp
pybind11                          → nanobind
#include <pybind11/pybind11.h>   → #include <nanobind/nanobind.h>
#include <pybind11/numpy.h>      → #include <nanobind/ndarray.h>
#include <pybind11/stl.h>        → #include <nanobind/stl/string.h>
#include <pybind11/functional.h> → #include <nanobind/stl/function.h>

namespace py = pybind11;         → namespace nb = nanobind;
PYBIND11_MODULE(...)             → NB_MODULE(...)
py::array_t<float>               → nb::ndarray<nb::numpy, float>
py::gil_scoped_release           → nb::gil_scoped_release
py::gil_scoped_acquire           → nb::gil_scoped_acquire
```

---

### Phase 4: Simple API Implementation
**Status**: Not started

**Tasks**:
1. Port `resample()` function
2. Adapt NumPy array handling for nanobind
3. Handle GIL release/acquire
4. Implement verbose parameter
5. Write test comparing against pybind11 `resample()`

**Deliverables**:
- Working `resample()` function
- Tests passing for Simple API
- Performance comparison data

**Testing Strategy**:
```python
import samplerate  # pybind11 version
import samplerate_nb  # nanobind version

# Regression test
output_pb = samplerate.resample(input_data, ratio, converter)
output_nb = samplerate_nb.resample(input_data, ratio, converter)
assert np.allclose(output_pb, output_nb)
```

---

### Phase 5: Full API Implementation
**Status**: Not started

**Tasks**:
1. Port `Resampler` class
2. Implement constructor, copy constructor, move constructor
3. Port `process()` method with NumPy array handling
4. Implement `set_ratio()`, `reset()`, `clone()` methods
5. Expose readonly attributes
6. Write tests for all Resampler functionality

**Deliverables**:
- Fully functional `Resampler` class
- All methods tested against pybind11 baseline
- Clone/copy semantics verified

**Key Considerations**:
- Ensure state management matches pybind11 behavior
- Verify destructor behavior (src_delete handles nullptr)
- Test multi-channel support (1D vs 2D arrays)

---

### Phase 6: Callback API Implementation
**Status**: Not started

**Tasks**:
1. Port `CallbackResampler` class
2. Implement callback function handling
3. Port context manager support (`__enter__`, `__exit__`)
4. Implement `read()` method
5. Handle callback error propagation
6. Port callback wrapper function (`the_callback_func`)
7. Write comprehensive callback tests

**Deliverables**:
- Fully functional `CallbackResampler` class
- Context manager support working
- Callback error handling tested
- Multi-channel callback support verified

**Key Considerations**:
- Callback GIL management is critical (acquire when calling Python)
- Error propagation from C callback to Python needs careful handling
- Context manager should properly destroy state

---

### Phase 7: Module Organization
**Status**: Not started

**Tasks**:
1. Port submodule structure (`exceptions`, `converters`, `_internals`)
2. Implement convenience imports
3. Add version attributes (`__version__`, `__libsamplerate_version__`)
4. Verify all imports work as expected
5. Test import patterns used in tests

**Deliverables**:
- Module organization matching pybind11
- All convenience imports working
- Version information accessible

---

### Phase 8: Comprehensive Testing
**Status**: Not started

**Tasks**:
1. Run all existing tests against nanobind implementation
2. Create regression test suite comparing pybind11 vs nanobind
3. Test all converter types (0-4, strings, enum)
4. Test 1D and 2D array inputs
5. Test edge cases (empty arrays, large ratios, etc.)
6. Verify exception handling
7. Test clone operations
8. Performance benchmarking

**Deliverables**:
- All 87+ tests passing for nanobind
- Regression test suite showing identical behavior
- Performance comparison report
- Documentation of any behavioral differences

**Test Categories**:
- Simple API: `test_simple()`, `test_match()`
- Full API: `test_process()`, `test_Resampler_clone()`
- Callback API: `test_callback()`, `test_callback_with()`, `test_CallbackResampler_clone()`
- Type handling: `test_converter_type()`
- Exceptions: tests in `test_exception.py`
- Threading: `test_threading_performance.py`
- Async: `test_asyncio_performance.py`

---

### Phase 9: Performance Analysis
**Status**: Not started

**Tasks**:
1. Compare compile times (pybind11 vs nanobind)
2. Compare binary sizes
3. Benchmark runtime performance
4. Measure memory usage
5. Test multi-threading performance
6. Document findings

**Deliverables**:
- Performance comparison report
- Binary size comparison
- Runtime benchmark results
- Threading scalability data

**Metrics to Track**:
- Compilation time (cold and warm builds)
- Binary size (`.so` file)
- Function call overhead
- Memory per wrapped object
- Multi-threaded scaling

---

### Phase 10: Documentation & Integration
**Status**: Not started

**Tasks**:
1. Document build process for nanobind variant
2. Update README with nanobind information
3. Document performance improvements
4. Create migration guide for consumers
5. Document import changes (samplerate-nb)
6. Add CI/CD configuration for dual builds (if needed)

**Deliverables**:
- Updated documentation
- Migration guide for end users
- CI/CD configuration
- Final validation

---

## Technical Implementation Notes

### NumPy Array Handling

**pybind11**:
```cpp
py::array_t<float, py::array::c_style | py::array::forcecast> input
py::buffer_info inbuf = input.request();
```

**nanobind**:
```cpp
nb::ndarray<nb::numpy, float, nb::c_contig> input
// Direct shape/data access without buffer_info
size_t rows = input.shape(0);
float* data = input.data();
```

### GIL Management
Both libraries support similar GIL scoped release/acquire:
```cpp
// Release GIL for C++ operations
{
    nb::gil_scoped_release release;
    // ... call libsamplerate ...
}

// Acquire GIL for Python calls
{
    nb::gil_scoped_acquire acquire;
    // ... call Python callback ...
}
```

### Exception Handling
nanobind uses same approach but with `nb::` namespace:
```cpp
nb::register_exception<ResamplingException>(m_exceptions, "ResamplingError", PyExc_RuntimeError);
```

### Build Configuration
```cmake
# Fetch nanobind
FetchContent_Declare(
  nanobind
  GIT_REPOSITORY https://github.com/wjakob/nanobind
  GIT_TAG v2.9.2
)
FetchContent_MakeAvailable(nanobind)

# Create module
nanobind_add_module(python-samplerate-nb src/samplerate_nb.cpp)
```

## Success Criteria

1. ✅ **Functional Parity**: All 87+ tests pass with nanobind implementation
2. ✅ **API Compatibility**: Drop-in replacement (importable as samplerate-nb)
3. ✅ **Regression Testing**: Identical behavior to pybind11 version
4. ✅ **Performance**: Binary size reduction, faster compile times
5. ✅ **Documentation**: Clear migration path for consumers

## Risk Mitigation

1. **Preserve pybind11 bindings**: Keep original for regression testing
2. **Incremental implementation**: Test each component before moving on
3. **Comprehensive testing**: Use existing test suite as baseline
4. **Performance validation**: Benchmark at each phase
5. **Documentation**: Record all differences and workarounds

## Timeline Estimate

- **Phase 1**: Infrastructure Setup - ✅ Complete
- **Phase 2**: Build System - 1 hour
- **Phase 3**: Core Bindings - 2 hours
- **Phase 4**: Simple API - 1 hour
- **Phase 5**: Full API - 2 hours
- **Phase 6**: Callback API - 3 hours
- **Phase 7**: Module Organization - 1 hour
- **Phase 8**: Testing - 2 hours
- **Phase 9**: Performance Analysis - 1 hour
- **Phase 10**: Documentation - 1 hour

**Total Estimated Time**: ~14 hours

## Current Status

**Phase 1 Complete**: Infrastructure setup done, baseline established with 87 tests passing.

**Next Steps**: Begin Phase 2 - Build System Configuration
