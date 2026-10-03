/*
 * Python bindings for libsamplerate
 * Copyright (C) 2023  Robin Scheibler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * You should have received a copy of the MIT License along with this program.
 * If not, see <https://opensource.org/licenses/MIT>.
 */

#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <samplerate.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#ifndef VERSION_INFO
#define VERSION_INFO "nightly"
#endif

// Build information defaults (set by CMake)
#ifndef BUILD_TYPE
#define BUILD_TYPE "unknown"
#endif
#ifndef COMPILER_ID
#define COMPILER_ID "unknown"
#endif
#ifndef COMPILER_VERSION
#define COMPILER_VERSION "unknown"
#endif
#ifndef CMAKE_VERSION
#define CMAKE_VERSION "unknown"
#endif
#ifndef TARGET_ARCH
#define TARGET_ARCH "unknown"
#endif
#ifndef TARGET_OS
#define TARGET_OS "unknown"
#endif
#ifndef PYBIND11_VERSION_INFO
#define PYBIND11_VERSION_INFO "unknown"
#endif
#ifndef LIBSAMPLERATE_VERSION
#define LIBSAMPLERATE_VERSION "unknown"
#endif
#ifndef LTO_ENABLED
#define LTO_ENABLED 0
#endif

// Output frames allocated on top of ceil(input_frames * ratio).
//
// A call can generate more than that: with end_of_input the converter flushes
// the input it holds back (about ratio x its filter half-length, e.g. 144 input
// frames for sinc_best, so up to ~37k output frames at ratio 256), and after a
// ratio change libsamplerate ramps from the previous ratio, which has no fixed
// bound. Resampler.process() therefore grows the buffer whenever libsamplerate
// fills it, and this value only trades a little memory for fewer regrowths.
constexpr size_t OUTPUT_HEADROOM_FRAMES = 1024;

// libsamplerate's limits (SRC_MAX_RATIO and src_sinc.c's MAX_CHANNELS); its
// public header does not export them.
constexpr double MAX_RATIO = 256.0;
constexpr int SINC_MAX_CHANNELS = 128;
// Error codes from libsamplerate's common.h, also internal. src_strerror()
// takes them, so their values are fixed.
constexpr int SRC_ERR_MALLOC_FAILED = 1;
constexpr int SRC_ERR_BAD_SRC_RATIO = 6;
constexpr int SRC_ERR_BAD_CHANNEL_COUNT = 11;

// Minimum number of input frames before releasing the GIL during resampling
// when using automatic GIL management. Releasing and re-acquiring the GIL has
// overhead (~1-5 µs), which becomes negligible for larger data sizes but can
// significantly impact performance for small data sizes. This threshold
// balances single-threaded performance (avoiding GIL overhead for small data)
// with multi-threaded performance (allowing parallelism for large data).
// Empirically chosen based on benchmarks showing that at 1000 frames, the GIL
// overhead is < 1% of total execution time for even the fastest converter types.
long gil_release_threshold_frames = 1000;

namespace py = pybind11;
using namespace pybind11::literals;

using callback_t =
    std::function<py::array_t<float, py::array::c_style | py::array::forcecast>(
        void)>;
using np_array_f32 =
    py::array_t<float, py::array::c_style | py::array::forcecast>;

namespace samplerate {

// Helper to determine if GIL should be released based on user preference
// and data size. The release_gil parameter can be:
//   - py::none() or "auto": Release GIL only for large data (>= threshold)
//   - True: Always release GIL (good for multi-threaded applications)
//   - False: Never release GIL (good for single-threaded, small data)
bool should_release_gil(const py::object &release_gil, long num_frames) {
  if (release_gil.is_none()) {
    // "auto" mode: release GIL only for large data sizes
    return num_frames >= gil_release_threshold_frames;
  } else if (py::isinstance<py::bool_>(release_gil)) {
    return release_gil.cast<bool>();
  } else if (py::isinstance<py::str>(release_gil)) {
    std::string s = release_gil.cast<std::string>();
    if (s == "auto") {
      return num_frames >= gil_release_threshold_frames;
    }
    throw std::domain_error("Invalid release_gil value. Use True, False, None, or 'auto'.");
  }
  throw std::domain_error("Invalid release_gil type. Use True, False, None, or 'auto'.");
}

enum class ConverterType {
  sinc_best,
  sinc_medium,
  sinc_fastest,
  zero_order_hold,
  linear
};

class ResamplingException : public std::exception {
 public:
  explicit ResamplingException(int err_num) : message{describe(err_num)} {}
  explicit ResamplingException(std::string msg) : message{std::move(msg)} {}
  const char *what() const noexcept override { return message.c_str(); }

 private:
  // src_strerror() returns NULL for codes it does not know.
  static std::string describe(int err_num) {
    const char *msg = src_strerror(err_num);
    return msg ? msg : "libsamplerate error " + std::to_string(err_num);
  }
  std::string message = "";
};

bool is_sinc(int converter_type) {
  return converter_type == SRC_SINC_BEST_QUALITY ||
         converter_type == SRC_SINC_MEDIUM_QUALITY ||
         converter_type == SRC_SINC_FASTEST;
}

// libsamplerate checks the ratio only against [1/256, 256], which NaN passes,
// and only once called: by then process() has sized its output buffer from
// the ratio (an infinite or huge one asks for terabytes). Check it first.
void check_ratio(double ratio) {
  if (!(ratio >= 1.0 / MAX_RATIO && ratio <= MAX_RATIO))
    throw ResamplingException(SRC_ERR_BAD_SRC_RATIO);
}

// src_new() reports too many channels for the sinc converters as "Channel
// count must be >= 1."; say what is actually wrong.
SRC_STATE *new_state(int converter_type, int channels,
                     const std::function<SRC_STATE *(int *)> &create) {
  int err_num = 0;
  SRC_STATE *state = create(&err_num);
  if (state != nullptr) return state;
  if (err_num == SRC_ERR_BAD_CHANNEL_COUNT && channels > SINC_MAX_CHANNELS &&
      is_sinc(converter_type))
    throw ResamplingException(
        "The sinc converters support at most " +
        std::to_string(SINC_MAX_CHANNELS) + " channels, got " +
        std::to_string(channels) +
        ". resample() splits wider input itself; for Resampler or "
        "CallbackResampler, use one per group of at most " +
        std::to_string(SINC_MAX_CHANNELS) + " channels.");
  if (err_num == 0) err_num = SRC_ERR_MALLOC_FAILED;
  throw ResamplingException(err_num);
}

int get_converter_type(const py::object &obj) {
  if (py::isinstance<py::str>(obj)) {
    py::str py_s = obj;
    std::string s = static_cast<std::string>(py_s);
    if (s.compare("sinc_best") == 0) {
      return 0;
    } else if (s.compare("sinc_medium") == 0) {
      return 1;
    } else if (s.compare("sinc_fastest") == 0) {
      return 2;
    } else if (s.compare("zero_order_hold") == 0) {
      return 3;
    } else if (s.compare("linear") == 0) {
      return 4;
    }
  } else if (py::isinstance<py::int_>(obj)) {
    py::int_ val = obj;
    return static_cast<int>(val);
  } else if (py::isinstance<ConverterType>(obj)) {
    py::int_ c = obj.attr("value");
    return static_cast<int>(c);
  }

  throw std::domain_error("Unsupported converter type");
  return -1;
}

void error_handler(int errnum) {
  if (errnum == 0) return;  // not an error
  // src_strerror() has no message (NULL) for codes libsamplerate never returns
  if (src_strerror(errnum) == nullptr)
    throw std::runtime_error("libsamplerate raised an unknown error code");
  throw ResamplingException(errnum);
}

// Resamplers release the GIL while libsamplerate runs, so two Python threads
// could otherwise drive the same SRC_STATE at once (or free it mid-call).
// Concurrent use of one stateful resampler is a caller bug; raise instead of
// corrupting state. A mutex could deadlock: the holder needs the GIL to grow
// the output buffer, and a waiter may be holding it.
class InUseGuard {
 public:
  explicit InUseGuard(std::atomic<bool> &flag) : _flag(flag) {
    if (_flag.exchange(true))
      throw std::runtime_error(
          "resampler is already in use by another thread (or re-entered "
          "from its own callback)");
  }
  ~InUseGuard() { _flag = false; }
  InUseGuard(const InUseGuard &) = delete;
  InUseGuard &operator=(const InUseGuard &) = delete;

 private:
  std::atomic<bool> &_flag;
};

class Resampler {
 private:
  SRC_STATE *_state = nullptr;
  mutable std::atomic<bool> _in_use{false};

 public:
  int _converter_type = 0;
  int _channels = 0;

 public:
  Resampler(const py::object &converter_type, int channels)
      : _converter_type(get_converter_type(converter_type)),
        _channels(channels) {
    _state = new_state(_converter_type, _channels, [&](int *err) {
      return src_new(_converter_type, _channels, err);
    });
  }

  // copy constructor
  Resampler(const Resampler &r)
      : _converter_type(r._converter_type), _channels(r._channels) {
    InUseGuard guard(r._in_use);
    int _err_num = 0;
    _state = src_clone(r._state, &_err_num);
    error_handler(_err_num);
  }

  // move constructor
  Resampler(Resampler &&r)
      : _state(r._state),
        _converter_type(r._converter_type),
        _channels(r._channels) {
    r._state = nullptr;
    r._converter_type = 0;
    r._channels = 0;
  }

  ~Resampler() { src_delete(_state); }  // src_delete handles nullptr case

  py::array_t<float, py::array::c_style> process(
      const py::array_t<float, py::array::c_style | py::array::forcecast> &input,
      double sr_ratio, bool end_of_input,
      const py::object &release_gil = py::none()) {
    InUseGuard guard(_in_use);

    // accessors for the arrays
    py::buffer_info inbuf = input.request();

    // set the number of channels
    int channels = 1;
    if (inbuf.ndim == 2)
      channels = inbuf.shape[1];
    else if (inbuf.ndim > 2)
      throw std::domain_error("Input array should have at most 2 dimensions");

    if (channels != _channels || channels == 0)
      throw std::domain_error("Invalid number of channels in input data.");
    check_ratio(sr_ratio);

    const auto in_frames = static_cast<size_t>(inbuf.shape[0]);
    const auto *in_ptr = static_cast<const float *>(inbuf.ptr);
    size_t capacity = static_cast<size_t>(std::ceil(in_frames * sr_ratio)) +
                      OUTPUT_HEADROOM_FRAMES;
    const bool release = should_release_gil(release_gil, inbuf.shape[0]);

    // allocate output array
    std::vector<size_t> out_shape{capacity};
    if (inbuf.ndim == 2) out_shape.push_back(static_cast<size_t>(channels));
    auto output = py::array_t<float, py::array::c_style>(out_shape);

    size_t frames_used = 0;
    size_t frames_gen = 0;
    while (true) {
      // libsamplerate struct
      SRC_DATA src_data = {
          in_ptr + frames_used * channels,                // data_in
          output.mutable_data() + frames_gen * channels,  // data_out
          long(in_frames - frames_used),                  // input_frames
          long(capacity - frames_gen),                    // output_frames
          0,             // input_frames_used, filled by libsamplerate
          0,             // output_frames_gen, filled by libsamplerate
          end_of_input,  // end_of_input
          sr_ratio       // src_ratio, sampling rate conversion ratio
      };

      int err_code;
      if (release) {
        py::gil_scoped_release release_guard;
        err_code = src_process(_state, &src_data);
      } else {
        err_code = src_process(_state, &src_data);
      }
      error_handler(err_code);

      frames_used += src_data.input_frames_used;
      frames_gen += src_data.output_frames_gen;

      // libsamplerate stops short of a full buffer only once it has used all
      // the input (and, with end_of_input, flushed what it held back).
      if (frames_gen < capacity) break;

      // Buffer full: there may be more output. Grow it and carry on rather
      // than dropping the input libsamplerate has not used yet.
      capacity *= 2;
      out_shape[0] = capacity;
      auto larger = py::array_t<float, py::array::c_style>(out_shape);
      std::copy_n(output.data(), frames_gen * channels, larger.mutable_data());
      output = larger;
    }

    // create a shorter view of the array
    out_shape[0] = frames_gen;
    py::buffer_info outbuf = output.request();
    return py::array_t<float, py::array::c_style>(
        out_shape, outbuf.strides, static_cast<float *>(outbuf.ptr), output);
  }

  void set_ratio(double new_ratio) {
    InUseGuard guard(_in_use);
    check_ratio(new_ratio);
    error_handler(src_set_ratio(_state, new_ratio));
  }

  void reset() {
    InUseGuard guard(_in_use);
    error_handler(src_reset(_state));
  }

  Resampler clone() const { return Resampler(*this); }
};

class CallbackResampler;

namespace {

long the_callback_func(void *cb_data, float **data);

// The CallbackResampler whose read() is running on this thread.
thread_local CallbackResampler *active_reader = nullptr;

}  // namespace

class CallbackResampler {
 private:
  SRC_STATE *_state = nullptr;
  callback_t _callback = nullptr;
  np_array_f32 _current_buffer;
  size_t _buffer_ndim = 0;
  std::exception_ptr _callback_error;
  mutable std::atomic<bool> _in_use{false};

 public:
  double _ratio = 0.0;
  int _converter_type = 0;
  size_t _channels = 0;

 private:
  void _create() {
    // No callback data: src_clone() copies it, so a clone would call back into
    // the original. the_callback_func uses active_reader instead.
    _state = new_state(_converter_type, (int)_channels, [&](int *err) {
      return src_callback_new(the_callback_func, _converter_type,
                              (int)_channels, err, nullptr);
    });
  }

  void _destroy() {
    if (_state != nullptr) {
      src_delete(_state);
      _state = nullptr;
    }
  }

 public:
  CallbackResampler(const callback_t &callback_func, double ratio,
                    const py::object &converter_type, size_t channels)
      : _callback(callback_func),
        _ratio(ratio),
        _converter_type(get_converter_type(converter_type)),
        _channels(channels) {
    _create();
  }

  // copy constructor
  CallbackResampler(const CallbackResampler &r)
      : _callback(r._callback),
        // libsamplerate keeps reading the last callback buffer across read()
        // calls, and the cloned state points into it too: keep it alive.
        _current_buffer(r._current_buffer),
        _buffer_ndim(r._buffer_ndim),
        _ratio(r._ratio),
        _converter_type(r._converter_type),
        _channels(r._channels) {
    InUseGuard guard(r._in_use);
    int _err_num = 0;
    _state = src_clone(r._state, &_err_num);
    if (_state == nullptr) error_handler(_err_num);
  }

  // move constructor
  CallbackResampler(CallbackResampler &&r)
      : _state(r._state),
        _callback(r._callback),
        _current_buffer(std::move(r._current_buffer)),
        _buffer_ndim(r._buffer_ndim),
        _callback_error(std::move(r._callback_error)),
        _ratio(r._ratio),
        _converter_type(r._converter_type),
        _channels(r._channels) {
    r._state = nullptr;
    r._callback = nullptr;
    r._buffer_ndim = 0;
    r._ratio = 0.0;
    r._converter_type = 0;
    r._channels = 0;
  }

  ~CallbackResampler() { _destroy(); }

  void set_buffer(const np_array_f32 &new_buf) { _current_buffer = new_buf; }
  size_t get_channels() { return _channels; }
  void set_callback_error(std::exception_ptr e) { _callback_error = e; }

  np_array_f32 callback(void) {
    auto input = _callback();

    auto inbuf = input.request();
    if (_buffer_ndim == 0) _buffer_ndim = inbuf.ndim;

    _current_buffer = input;
    return input;
  }

  py::array_t<float, py::array::c_style> read(
      size_t frames, const py::object &release_gil = py::none()) {
    InUseGuard guard(_in_use);
    // ratio is writable from Python; libsamplerate would take a NaN
    check_ratio(_ratio);

    // allocate output array
    std::vector<size_t> out_shape{frames, _channels};
    auto output = py::array_t<float, py::array::c_style>(out_shape);
    py::buffer_info outbuf = output.request();

    if (_state == nullptr) _create();

    // Perform callback resampling with optional GIL release.
    // Note: the_callback_func will acquire GIL when calling Python callback.
    // src_callback_read() calls the_callback_func on this thread; tell it
    // which resampler is reading (restored afterwards, for nested reads).
    auto do_callback_read = [&]() {
      auto *outer = std::exchange(active_reader, this);
      size_t gen = src_callback_read(_state, _ratio, (long)frames,
                                     static_cast<float *>(outbuf.ptr));
      active_reader = outer;
      return std::make_pair(gen, gen == 0 ? src_error(_state) : 0);
    };

    size_t output_frames_gen;
    int err_code;
    if (should_release_gil(release_gil, (long)frames)) {
      py::gil_scoped_release release;
      auto result = do_callback_read();
      output_frames_gen = result.first;
      err_code = result.second;
    } else {
      auto result = do_callback_read();
      output_frames_gen = result.first;
      err_code = result.second;
    }

    // re-raise anything the callback raised, now that we are out of
    // libsamplerate's C code
    if (_callback_error)
      std::rethrow_exception(std::exchange(_callback_error, nullptr));

    // check error status
    if (output_frames_gen == 0) {
      error_handler(err_code);
    }

    // if there is only one channel and the input array had only on dimension
    // we also output a 1D array
    if (_channels == 1 && _buffer_ndim == 1) {
      out_shape.pop_back();
      output = py::array_t<float, py::array::c_style>(
          out_shape, static_cast<float *>(outbuf.ptr));
    }

    // create a shorter view of the array
    if (output_frames_gen < frames) {
      out_shape[0] = output_frames_gen;
      auto strides = std::vector<py::ssize_t>(output.strides(),
                                              output.strides() + output.ndim());
      return py::array_t<float, py::array::c_style>(
          out_shape, strides, static_cast<float *>(output.request().ptr),
          output);
    }

    return output;
  }

  void set_starting_ratio(double new_ratio) {
    InUseGuard guard(_in_use);
    check_ratio(new_ratio);
    error_handler(src_set_ratio(_state, new_ratio));
    _ratio = new_ratio;
  }

  void reset() {
    InUseGuard guard(_in_use);
    error_handler(src_reset(_state));
  }

  CallbackResampler clone() const { return CallbackResampler(*this); }
  CallbackResampler &__enter__() { return *this; }
  void __exit__(const py::object &/*exc_type*/, const py::object &/*exc*/,
                const py::object &/*exc_tb*/) {
    InUseGuard guard(_in_use);
    _destroy();
  }
};

namespace {

long the_callback_func(void * /*cb_data*/, float **data) {
  CallbackResampler *cb = active_reader;
  int cb_channels = cb->get_channels();

  // read() may release the GIL around src_callback_read(). Hold it for the
  // whole callback: inbuf's destructor releases a Python buffer too.
  py::gil_scoped_acquire acquire;

  // Exceptions must not unwind through libsamplerate's C frames. Store them,
  // return 0 (no more input) and let read() re-raise.
  try {
    // get the data as a numpy array
    py::buffer_info inbuf = cb->callback().request();

    // end of stream is signaled by a None, which is cast to a ndarray with
    // ndim == 0
    if (inbuf.ndim == 0) return 0;

    // set the number of channels
    int channels = 1;
    if (inbuf.ndim == 2)
      channels = inbuf.shape[1];
    else if (inbuf.ndim > 2)
      throw std::domain_error("Input array should have at most 2 dimensions");

    if (channels != cb_channels || channels == 0)
      throw std::domain_error("Invalid number of channels in input data.");

    // the array stays alive in cb->_current_buffer until the next callback
    *data = static_cast<float *>(inbuf.ptr);

    return (long)inbuf.shape[0];
  } catch (...) {
    cb->set_callback_error(std::current_exception());
    return 0;
  }
}

}  // namespace

py::array_t<float, py::array::c_style> resample(
    const py::array_t<float, py::array::c_style | py::array::forcecast> &input,
    double sr_ratio, const py::object &converter_type, bool verbose,
    const py::object &release_gil = py::none()) {
  // input array has shape (n_samples, n_channels)

  // accessors for the arrays
  py::buffer_info inbuf = input.request();

  // set the number of channels
  int channels = 1;
  if (inbuf.ndim == 2)
    channels = inbuf.shape[1];
  else if (inbuf.ndim > 2)
    throw std::domain_error("Input array should have at most 2 dimensions");

  if (channels == 0)
    throw std::domain_error("Invalid number of channels (0) in input data.");

  const int converter = get_converter_type(converter_type);
  check_ratio(sr_ratio);

  // src_simple() is src_new() + one src_process() with end_of_input +
  // src_delete(), but cannot continue when the output buffer is full.
  // Resampler.process() does the same and grows the buffer instead.
  py::array_t<float, py::array::c_style> output;
  if (channels <= SINC_MAX_CHANNELS || !is_sinc(converter)) {
    output = Resampler(py::int_(converter), channels)
                 .process(input, sr_ratio, /*end_of_input=*/true, release_gil);
  } else {
    // The sinc converters take at most SINC_MAX_CHANNELS channels. Channels
    // are independent, so convert them in groups; each group has the same
    // length and ratio, so the same number of output frames.
    const auto frames = static_cast<size_t>(inbuf.shape[0]);
    const auto *in_ptr = static_cast<const float *>(inbuf.ptr);
    float *out_ptr = nullptr;
    size_t out_frames = 0;
    for (int first = 0; first < channels; first += SINC_MAX_CHANNELS) {
      const int width = std::min(SINC_MAX_CHANNELS, channels - first);
      np_array_f32 group({frames, static_cast<size_t>(width)});
      float *g = group.mutable_data();
      for (size_t f = 0; f < frames; ++f)
        std::copy_n(in_ptr + f * channels + first, width, g + f * width);

      auto part = Resampler(py::int_(converter), width)
                      .process(group, sr_ratio, true, release_gil);
      const auto part_frames = static_cast<size_t>(part.shape(0));
      if (first == 0) {
        out_frames = part_frames;
        output = py::array_t<float, py::array::c_style>(
            {out_frames, static_cast<size_t>(channels)});
        out_ptr = output.mutable_data();
      } else if (part_frames != out_frames) {
        throw std::runtime_error(
            "channel groups resampled to different lengths");
      }
      const float *p = part.data();
      for (size_t f = 0; f < out_frames; ++f)
        std::copy_n(p + f * width, width, out_ptr + f * channels + first);
    }
  }

  if (verbose) {
    py::print("samplerate info:");
    py::print(inbuf.shape[0], " input frames used");
    py::print(output.shape(0), " output frames generated");
  }

  return output;
}

}  // namespace samplerate

namespace sr = samplerate;

PYBIND11_MODULE(samplerate, m) {
  m.doc() =
      "A simple python wrapper library around libsamplerate";  // optional
                                                               // module
                                                               // docstring
  m.attr("__version__") = VERSION_INFO;
  m.attr("__libsamplerate_version__") = LIBSAMPLERATE_VERSION;

  m.def("set_gil_release_threshold", [](long threshold) {
    gil_release_threshold_frames = threshold;
  }, "Set the minimum number of frames required to release the GIL in 'auto' mode.");

  m.def("get_gil_release_threshold", []() {
    return gil_release_threshold_frames;
  }, "Get the minimum number of frames required to release the GIL in 'auto' mode.");

  m.def("get_build_info", []() {
    py::dict info;
    info["version"] = VERSION_INFO;
    info["libsamplerate_version"] = LIBSAMPLERATE_VERSION;
    info["build_type"] = BUILD_TYPE;
    info["compiler_id"] = COMPILER_ID;
    info["compiler_version"] = COMPILER_VERSION;
    info["cmake_version"] = CMAKE_VERSION;
    info["target_arch"] = TARGET_ARCH;
    info["target_os"] = TARGET_OS;
    info["pybind11_version"] = PYBIND11_VERSION_INFO;
    // C++ standard - MSVC uses _MSVC_LANG instead of __cplusplus
#ifdef _MSVC_LANG
    #define CPP_STD_VALUE _MSVC_LANG
#else
    #define CPP_STD_VALUE __cplusplus
#endif
#if CPP_STD_VALUE >= 202002L
    info["cpp_standard"] = "C++20";
#elif CPP_STD_VALUE >= 201703L
    info["cpp_standard"] = "C++17";
#elif CPP_STD_VALUE >= 201402L
    info["cpp_standard"] = "C++14";
#elif CPP_STD_VALUE >= 201103L
    info["cpp_standard"] = "C++11";
#else
    info["cpp_standard"] = "pre-C++11";
#endif
#undef CPP_STD_VALUE
    // LTO status (passed from CMake)
#if LTO_ENABLED
    info["lto_enabled"] = true;
#else
    info["lto_enabled"] = false;
#endif
    // Pointer size (32 vs 64 bit)
    info["pointer_size_bits"] = sizeof(void*) * 8;
    // Float size sanity check
    info["float_size_bytes"] = sizeof(float);
    info["gil_release_threshold"] = gil_release_threshold_frames;
    return info;
  }, R"doc(
Get detailed build information for debugging purposes.

Returns
-------
dict
    Dictionary containing:
    - version: Package version
    - libsamplerate_version: libsamplerate library version
    - build_type: Build configuration (Release, Debug, etc.)
    - compiler_id: Compiler used (MSVC, GNU, Clang, etc.)
    - compiler_version: Compiler version string
    - cmake_version: CMake version used for build
    - target_arch: Target architecture (x86_64, arm64, etc.)
    - target_os: Target operating system
    - pybind11_version: pybind11 version
    - cpp_standard: C++ standard used
    - lto_enabled: Whether Link Time Optimization was enabled
    - pointer_size_bits: Pointer size (32 or 64)
    - float_size_bytes: Size of float type (should be 4)
    - gil_release_threshold: Current GIL release threshold
)doc");

  auto m_exceptions = m.def_submodule(
      "exceptions", "Sub-module containing sampling exceptions");
  auto m_converters = m.def_submodule(
      "converters", "Sub-module containing the samplerate converters");
  auto m_internals = m.def_submodule("_internals", "Internal helper functions");

  // give access to this function for testing
  m_internals.def(
      "get_converter_type", &sr::get_converter_type,
      "Convert python object to integer of converter tpe or raise an error "
      "if illegal");

  m_internals.def(
      "error_handler", &sr::error_handler,
      "A function to translate libsamplerate error codes into exceptions");

  py::register_exception<sr::ResamplingException>(
      m_exceptions, "ResamplingError", PyExc_RuntimeError);

  m_converters.def("resample", &sr::resample, R"mydelimiter(
    Resample the signal in `input_data` at once.

    Parameters
    ----------
    input_data : ndarray
        Input data.
        Input data with one or more channels is represented as a 2D array of shape
        (`num_frames`, `num_channels`).
        A single channel can be provided as a 1D array of `num_frames` length.
        For use with `libsamplerate`, `input_data`
        is converted to 32-bit float and C (row-major) memory order.
    ratio : float
        Conversion ratio = output sample rate / input sample rate.
    converter_type : ConverterType, str, or int
        Sample rate converter (default: `sinc_best`).
    verbose : bool
        If `True`, print additional information about the conversion.
    release_gil : bool, str, or None
        Controls GIL release during resampling for multi-threading:
        - `None` or `"auto"` (default): Release GIL only for large data (>= 1000 frames)
        - `True`: Always release GIL (best for multi-threaded applications)
        - `False`: Never release GIL (best for single-threaded, small data)

    Returns
    -------
    output_data : ndarray
        Resampled input data.

    Note
    ----
    If samples are to be processed in chunks, `Resampler` and
    `CallbackResampler` will provide better results and allow for variable
    conversion ratios.
  )mydelimiter",
                   "input"_a, "ratio"_a, "converter_type"_a = "sinc_best",
                   "verbose"_a = false, "release_gil"_a = py::none());

  py::class_<sr::Resampler>(m_converters, "Resampler", R"mydelimiter(
    Resampler.

    Parameters
    ----------
    converter_type : ConverterType, str, or int
        Sample rate converter (default: `sinc_best`).
    num_channels : int
        Number of channels.
  )mydelimiter")
      .def(py::init<const py::object &, int>(),
           "converter_type"_a = "sinc_best", "channels"_a = 1)
      .def(py::init<sr::Resampler>())
      .def("process", &sr::Resampler::process, R"mydelimiter(
        Resample the signal in `input_data`.

        Parameters
        ----------
        input_data : ndarray
            Input data.
            Input data with one or more channels is represented as a 2D array of shape
            (`num_frames`, `num_channels`).
            A single channel can be provided as a 1D array of `num_frames` length.
            For use with `libsamplerate`, `input_data` is converted to 32-bit float and
            C (row-major) memory order.
        ratio : float
            Conversion ratio = output sample rate / input sample rate.
        end_of_input : int
            Set to `True` if no more data is available, or to `False` otherwise.
        release_gil : bool, str, or None
            Controls GIL release during resampling for multi-threading:
            - `None` or `"auto"` (default): Release GIL only for large data (>= 1000 frames)
            - `True`: Always release GIL (best for multi-threaded applications)
            - `False`: Never release GIL (best for single-threaded, small data)

        Returns
        -------
        output_data : ndarray
            Resampled input data.
      )mydelimiter",
           "input"_a, "ratio"_a, "end_of_input"_a = false, "release_gil"_a = py::none())
      .def("reset", &sr::Resampler::reset, "Reset internal state.")
      .def("set_ratio", &sr::Resampler::set_ratio,
           "Set a new conversion ratio immediately.")
      .def("clone", &sr::Resampler::clone,
           "Creates a copy of the resampler object with the same internal "
           "state.")
      .def_readonly("converter_type", &sr::Resampler::_converter_type,
                    "Converter type.")
      .def_readonly("channels", &sr::Resampler::_channels,
                    "Number of channels.");

  py::class_<sr::CallbackResampler>(m_converters, "CallbackResampler",
                                    R"mydelimiter(
    CallbackResampler.

    Parameters
    ----------
    callback : function
        Function that returns new frames on each call, or `None` otherwise.
        Input data with one or more channels is represented as a 2D array of shape
        (`num_frames`, `num_channels`).
        A single channel can be provided as a 1D array of `num_frames` length.
        For use with `libsamplerate`, `input_data` is converted to 32-bit float and
        C (row-major) memory order.
    ratio : float
        Conversion ratio = output sample rate / input sample rate.
    converter_type : ConverterType, str, or int
        Sample rate converter.
    channels : int
        Number of channels.
    )mydelimiter")
      .def(py::init<const callback_t &, double, const py::object &, int>(),
           "callback"_a, "ratio"_a, "converter_type"_a = "sinc_best",
           "channels"_a = 1)
      .def(py::init<sr::CallbackResampler>())
      .def("read", &sr::CallbackResampler::read, R"mydelimiter(
            Read a number of frames from the resampler.

            Parameters
            ----------
            num_frames : int
                Number of frames to read.
            release_gil : bool, str, or None
                Controls GIL release during resampling for multi-threading:
                - `None` or `"auto"` (default): Release GIL only for large data (>= 1000 frames)
                - `True`: Always release GIL (best for multi-threaded applications)
                - `False`: Never release GIL (best for single-threaded, small data)

            Returns
            -------
            output_data : ndarray
                Resampled frames as a (`num_output_frames`, `num_channels`) or
                (`num_output_frames`,) array. Note that this may return fewer frames
                than requested, for example when no more input is available.
           )mydelimiter",
           "num_frames"_a, "release_gil"_a = py::none())
      .def("reset", &sr::CallbackResampler::reset, "Reset state.")
      .def("set_starting_ratio", &sr::CallbackResampler::set_starting_ratio,
           "Set the starting conversion ratio for the next `read` call.")
      .def("clone", &sr::CallbackResampler::clone,
           "Create a copy of the resampler object.")
      .def("__enter__", &sr::CallbackResampler::__enter__,
           py::return_value_policy::reference_internal)
      .def("__exit__", &sr::CallbackResampler::__exit__)
      .def_readwrite(
          "ratio", &sr::CallbackResampler::_ratio,
          "Conversion ratio = output sample rate / input sample rate.")
      .def_readonly("converter_type", &sr::CallbackResampler::_converter_type,
                    "Converter type.")
      .def_readonly("channels", &sr::CallbackResampler::_channels,
                    "Number of channels.");

  py::enum_<sr::ConverterType>(m_converters, "ConverterType", R"mydelimiter(
      Enum of samplerate converter types.

      Pass any of the members, or their string or value representation, as
      ``converter_type`` in the resamplers.
    )mydelimiter")
      .value("sinc_best", sr::ConverterType::sinc_best)
      .value("sinc_medium", sr::ConverterType::sinc_medium)
      .value("sinc_fastest", sr::ConverterType::sinc_fastest)
      .value("zero_order_hold", sr::ConverterType::zero_order_hold)
      .value("linear", sr::ConverterType::linear)
      .export_values();

  // Convenience imports
  m.attr("ResamplingError") = m_exceptions.attr("ResamplingError");
  m.attr("resample") = m_converters.attr("resample");
  m.attr("CallbackResampler") = m_converters.attr("CallbackResampler");
  m.attr("Resampler") = m_converters.attr("Resampler");
  m.attr("ConverterType") = m_converters.attr("ConverterType");
}
