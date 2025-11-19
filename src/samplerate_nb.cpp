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
#include <iostream>
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

  // Convenience imports
  m.attr("ResamplingError") = m_exceptions.attr("ResamplingError");
  m.attr("ConverterType") = m_converters.attr("ConverterType");
}
