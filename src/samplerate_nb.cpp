/*
 * Python bindings for libsamplerate using nanobind
 * Copyright (C) 2025  LedFx Team
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

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/string.h>
#include <samplerate.h>

#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

#ifndef VERSION_INFO
#define VERSION_INFO "nightly"
#endif

// This value was empirically and somewhat arbitrarily chosen; increase it for further safety.
#define END_OF_INPUT_EXTRA_OUTPUT_FRAMES 10000

namespace nb = nanobind;
using namespace nb::literals;

// Type aliases for nanobind arrays
using nb_array_f32 = nb::ndarray<nb::numpy, float, nb::c_contig>;
using callback_t = std::function<nb_array_f32(void)>;

namespace samplerate {

enum class ConverterType {
  sinc_best,
  sinc_medium,
  sinc_fastest,
  zero_order_hold,
  linear
};

class ResamplingException : public std::exception {
 public:
  explicit ResamplingException(int err_num) : message{src_strerror(err_num)} {}
  const char *what() const noexcept override { return message.c_str(); }

 private:
  std::string message = "";
};

int get_converter_type(const nb::object &obj) {
  if (nb::isinstance<nb::str>(obj)) {
    std::string s = nb::cast<std::string>(obj);
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
  } else if (nb::isinstance<nb::int_>(obj)) {
    return nb::cast<int>(obj);
  } else if (nb::isinstance<ConverterType>(obj)) {
    nb::int_ c = obj.attr("value");
    return nb::cast<int>(c);
  }

  throw std::domain_error("Unsupported converter type");
  return -1;
}

void error_handler(int errnum) {
  if (errnum > 0 && errnum < 24) {
    throw ResamplingException(errnum);
  } else if (errnum != 0) {  // the zero case is excluded as it is not an error
    // this will throw a segmentation fault if we call src_strerror here
    // also, these should never happen
    throw std::runtime_error("libsamplerate raised an unknown error code");
  }
}

nb::ndarray<nb::numpy, float> resample(
    const nb::ndarray<nb::numpy, const float, nb::c_contig> &input,
    double sr_ratio, const nb::object &converter_type, bool verbose) {
  // input array has shape (n_samples, n_channels)
  int converter_type_int = get_converter_type(converter_type);

  // Get array dimensions
  size_t ndim = input.ndim();
  size_t num_frames = input.shape(0);
  
  // set the number of channels
  int channels = 1;
  if (ndim == 2) {
    channels = input.shape(1);
  } else if (ndim > 2) {
    throw std::domain_error("Input array should have at most 2 dimensions");
  }

  if (channels == 0) {
    throw std::domain_error("Invalid number of channels (0) in input data.");
  }

  // Add buffer space to match Resampler.process() behavior with end_of_input=True
  // src_simple internally behaves like end_of_input=True, so it may generate
  // extra samples from buffer flushing, especially for certain converters
  const auto new_size =
      static_cast<size_t>(std::ceil(num_frames * sr_ratio))
      + END_OF_INPUT_EXTRA_OUTPUT_FRAMES;

  // Allocate output array
  size_t total_elements = new_size * channels;
  float* output_data = new float[total_elements];
  
  // Create capsule for memory management
  nb::capsule owner(output_data, [](void* p) noexcept {
    delete[] static_cast<float*>(p);
  });

  // libsamplerate struct
  SRC_DATA src_data = {
      const_cast<float *>(input.data()),  // data_in
      output_data,                         // data_out
      static_cast<long>(num_frames),       // input_frames
      long(new_size),                      // output_frames
      0,        // input_frames_used, filled by libsamplerate
      0,        // output_frames_gen, filled by libsamplerate
      0,        // end_of_input, not used by src_simple ?
      sr_ratio  // src_ratio, sampling rate conversion ratio
  };

  // Release GIL for the entire resampling operation
  int err_code;
  long output_frames_gen;
  long input_frames_used;
  {
    nb::gil_scoped_release release;
    err_code = src_simple(&src_data, converter_type_int, channels);
    output_frames_gen = src_data.output_frames_gen;
    input_frames_used = src_data.input_frames_used;
  }
  error_handler(err_code);

  // Handle unexpected output size
  if ((size_t)output_frames_gen > new_size) {
    // This means our fudge factor is too small.
    throw std::runtime_error("Generated more output samples than expected!");
  }

  if (verbose) {
    nb::print("samplerate info:");
    std::ostringstream oss1, oss2;
    oss1 << input_frames_used << " input frames used";
    oss2 << output_frames_gen << " output frames generated";
    nb::print(oss1.str().c_str());
    nb::print(oss2.str().c_str());
  }

  // Create output ndarray with proper shape and stride
  size_t output_shape[2];
  int64_t output_stride[2];
  
  if (ndim == 2) {
    output_shape[0] = output_frames_gen;
    output_shape[1] = channels;
    output_stride[0] = channels * sizeof(float);
    output_stride[1] = sizeof(float);
    
    return nb::ndarray<nb::numpy, float>(
        output_data,
        2,
        output_shape,
        owner,
        output_stride
    );
  } else {
    output_shape[0] = output_frames_gen;
    output_stride[0] = sizeof(float);
    
    return nb::ndarray<nb::numpy, float>(
        output_data,
        1,
        output_shape,
        owner,
        output_stride
    );
  }
}

class Resampler {
 private:
  SRC_STATE *_state = nullptr;

 public:
  int _converter_type = 0;
  int _channels = 0;

 public:
  Resampler(const nb::object &converter_type, int channels)
      : _converter_type(get_converter_type(converter_type)),
        _channels(channels) {
    int _err_num = 0;
    _state = src_new(_converter_type, _channels, &_err_num);
    error_handler(_err_num);
  }

  // copy constructor
  Resampler(const Resampler &r)
      : _converter_type(r._converter_type), _channels(r._channels) {
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

  nb::ndarray<nb::numpy, float> process(
      const nb::ndarray<nb::numpy, const float, nb::c_contig> &input,
      double sr_ratio, bool end_of_input) {
    // Get array dimensions
    size_t ndim = input.ndim();
    size_t num_frames = input.shape(0);

    // set the number of channels
    int channels = 1;
    if (ndim == 2)
      channels = input.shape(1);
    else if (ndim > 2)
      throw std::domain_error("Input array should have at most 2 dimensions");

    if (channels != _channels || channels == 0)
      throw std::domain_error("Invalid number of channels in input data.");

    // Add a "fudge factor" to the size. This is because the actual number of
    // output samples generated on the last call when input is terminated can
    // be more than the expected number of output samples during mid-stream
    // steady-state processing. (Also, when the stream is started, the number
    // of output samples generated will generally be zero or otherwise less
    // than the number of samples in mid-stream processing.)
    const auto new_size =
        static_cast<size_t>(std::ceil(num_frames * sr_ratio))
        + END_OF_INPUT_EXTRA_OUTPUT_FRAMES;

    // Allocate output array
    size_t total_elements = new_size * channels;
    float* output_data = new float[total_elements];
    
    // Create capsule for memory management
    nb::capsule owner(output_data, [](void* p) noexcept {
      delete[] static_cast<float*>(p);
    });

    // libsamplerate struct
    SRC_DATA src_data = {
        const_cast<float *>(input.data()),  // data_in
        output_data,                         // data_out
        static_cast<long>(num_frames),       // input_frames
        long(new_size),                      // output_frames
        0,             // input_frames_used, filled by libsamplerate
        0,             // output_frames_gen, filled by libsamplerate
        end_of_input,  // end_of_input
        sr_ratio       // src_ratio, sampling rate conversion ratio
    };

    // Release GIL for the entire resampling operation
    int err_code;
    long output_frames_gen;
    {
      nb::gil_scoped_release release;
      err_code = src_process(_state, &src_data);
      output_frames_gen = src_data.output_frames_gen;
    }
    error_handler(err_code);

    // Handle unexpected output size
    if ((size_t)output_frames_gen > new_size) {
      // This means our fudge factor is too small.
      throw std::runtime_error("Generated more output samples than expected!");
    }

    // Create output ndarray with proper shape and stride
    size_t output_shape[2];
    int64_t output_stride[2];
    
    if (ndim == 2) {
      output_shape[0] = output_frames_gen;
      output_shape[1] = channels;
      output_stride[0] = channels * sizeof(float);
      output_stride[1] = sizeof(float);
      
      return nb::ndarray<nb::numpy, float>(
          output_data,
          2,
          output_shape,
          owner,
          output_stride
      );
    } else {
      output_shape[0] = output_frames_gen;
      output_stride[0] = sizeof(float);
      
      return nb::ndarray<nb::numpy, float>(
          output_data,
          1,
          output_shape,
          owner,
          output_stride
      );
    }
  }

  void set_ratio(double new_ratio) {
    error_handler(src_set_ratio(_state, new_ratio));
  }

  void reset() { error_handler(src_reset(_state)); }

  Resampler clone() const { return Resampler(*this); }
};

}  // namespace samplerate

namespace sr = samplerate;

NB_MODULE(samplerate, m) {
  m.doc() = "A simple python wrapper library around libsamplerate using nanobind";
  m.attr("__version__") = VERSION_INFO;
  m.attr("__libsamplerate_version__") = LIBSAMPLERATE_VERSION;

  auto m_exceptions = m.def_submodule(
      "exceptions", "Sub-module containing sampling exceptions");
  auto m_converters = m.def_submodule(
      "converters", "Sub-module containing the samplerate converters");
  auto m_internals = m.def_submodule("_internals", "Internal helper functions");

  // give access to this function for testing
  m_internals.def(
      "get_converter_type", &sr::get_converter_type,
      "Convert python object to integer of converter type or raise an error "
      "if illegal");

  m_internals.def(
      "error_handler", &sr::error_handler,
      "A function to translate libsamplerate error codes into exceptions");

  nb::register_exception_translator([](const std::exception_ptr &p, void *payload) {
    try {
      std::rethrow_exception(p);
    } catch (const sr::ResamplingException &e) {
      PyErr_SetString(PyExc_RuntimeError, e.what());
    }
  });

  // Create ResamplingError as an alias to the Python RuntimeError
  m_exceptions.attr("ResamplingError") = nb::handle(PyExc_RuntimeError);

  nb::enum_<sr::ConverterType>(m_converters, "ConverterType", R"mydelimiter(
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
                   "verbose"_a = false);

  nb::class_<sr::Resampler>(m_converters, "Resampler", R"mydelimiter(
    Resampler.

    Parameters
    ----------
    converter_type : ConverterType, str, or int
        Sample rate converter (default: `sinc_best`).
    num_channels : int
        Number of channels.
  )mydelimiter")
      .def(nb::init<const nb::object &, int>(),
           "converter_type"_a = "sinc_best", "channels"_a = 1)
      .def(nb::init<sr::Resampler>())
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
        verbose : bool
            If `True`, print additional information about the conversion.

        Returns
        -------
        output_data : ndarray
            Resampled input data.
      )mydelimiter",
           "input"_a, "ratio"_a, "end_of_input"_a = false)
      .def("reset", &sr::Resampler::reset, "Reset internal state.")
      .def("set_ratio", &sr::Resampler::set_ratio,
           "Set a new conversion ratio immediately.")
      .def("clone", &sr::Resampler::clone,
           "Creates a copy of the resampler object with the same internal "
           "state.")
      .def_ro("converter_type", &sr::Resampler::_converter_type,
                     "Converter type.")
      .def_ro("channels", &sr::Resampler::_channels,
                     "Number of channels.");

  // Convenience imports
  m.attr("ResamplingError") = m_exceptions.attr("ResamplingError");
  m.attr("resample") = m_converters.attr("resample");
  m.attr("Resampler") = m_converters.attr("Resampler");
  m.attr("ConverterType") = m_converters.attr("ConverterType");
}
