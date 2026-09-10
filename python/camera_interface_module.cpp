/**
 * @file    python/camera_interface_module.cpp
 * @brief   Python bindings for the camera-interface stack
 * @details Exposes Camera::Interface so a Python process can own the camera
 *          directly, without a camerad process or its text protocol in
 *          between. The controller and instrument are selected at CMake
 *          configure time, so a given build of this module drives exactly one
 *          of them; call instrument_name() and controller_name() to confirm
 *          which build was loaded.
 */

#include "camera_interface.h"
#include "common.h"
#include "logentry.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace py = pybind11;

namespace {

  /// Name of the instrument this module was configured with, or "none"
  constexpr const char* INSTRUMENT_NAME =
#ifdef CAMERAD_INSTRUMENT_NAME
    CAMERAD_INSTRUMENT_NAME;
#else
    "none";
#endif

  /// Name of the controller this module was configured with
  constexpr const char* CONTROLLER_NAME =
#if defined(CONTROLLER_ARCHON)
    "archon";
#elif defined(CONTROLLER_ASTROCAM)
    "astrocam";
#elif defined(CONTROLLER_BOB)
    "bob";
#elif defined(CONTROLLER_JOE)
    "joe";
#else
    "unknown";
#endif

  /// Look up a configuration value, returning fallback when the key is absent
  std::string config_value(Config &configfile,
                           const std::string &key,
                           const std::string &fallback) {
    for (int row = 0; row < configfile.n_rows; ++row) {
      if (configfile.param[row] == key) return configfile.arg[row];
    }
    return fallback;
  }

  /**
   * @brief  Run a camera command and return its result string
   * @details Every Camera::Interface command shares the same
   *          (args in, retstring out, long status) shape, so one helper covers
   *          all of them. Failure raises std::runtime_error, which pybind11
   *          surfaces as RuntimeError, matching how the hispec daemons already
   *          report a rejected keyword write.
   */
  std::string invoke(long status, const std::string &name, std::string &retstring) {
    if (status == NO_ERROR || status == HELP) return retstring;
    throw std::runtime_error(name + " failed: " +
                             (retstring.empty() ? "no detail" : retstring));
  }

  /**
   * @brief  Owns a Camera::Interface and the one-time setup camerad performs
   *
   * camerad.cpp reads the config, initializes logging, then runs four
   * configure steps before serving. Anything driving the interface has to do
   * the same, so that sequence lives here rather than being reimplemented by
   * every caller.
   */
  class CameraSession {
    public:
      /// Build an interface and run the full configure sequence
      CameraSession(const std::string &config_path, std::optional<bool> log_to_stderr) {
        this->interface = Camera::Interface::create();
        this->interface->configfile.filename = config_path;

        if (this->interface->configfile.read_config() != NO_ERROR) {
          throw std::runtime_error("could not read configuration " + config_path);
        }

        const std::string logpath = config_value(this->interface->configfile, "LOGPATH", "");
        if (logpath.empty()) {
          throw std::runtime_error("LOGPATH not specified in " + config_path);
        }
        const std::string log_tmzone = config_value(this->interface->configfile, "TM_ZONE_LOG", "local");
        const std::string configured = config_value(this->interface->configfile, "LOG_STDERR", "true");
        const bool to_console = log_to_stderr.value_or(configured != "false");
        if (init_log("camerad", logpath, to_console ? "true" : "false", log_tmzone) != NO_ERROR) {
          throw std::runtime_error("could not initialize logging in " + logpath);
        }

        this->interface->configure_controller();
        this->interface->configure_interface();
        this->interface->configure_instrument();
        this->interface->configure_frame_outputs();
      }

      // Release the camera before the logger, so teardown is still logged
      ~CameraSession() {
        this->interface.reset();
        close_log();
      }

      CameraSession(const CameraSession&) = delete;
      CameraSession& operator=(const CameraSession&) = delete;

      Camera::Interface& operator*() const { return *this->interface; }

    private:
      std::unique_ptr<Camera::Interface> interface;
  };

}

PYBIND11_MODULE(camera_interface, module) {
  module.doc() = "Direct control of a camera-interface camera, without camerad";

  module.def("instrument_name", [] { return std::string(INSTRUMENT_NAME); },
             "Return the instrument this module was built for");
  module.def("controller_name", [] { return std::string(CONTROLLER_NAME); },
             "Return the controller this module was built for");

  py::class_<CameraSession>(module, "Camera",
      "One camera, configured from a camerad .cfg file.\n\n"
      "Construction performs the same setup camerad does at startup: read the\n"
      "config, initialize logging, then configure the controller, interface,\n"
      "instrument and frame outputs. It does not connect to the controller;\n"
      "call open() for that.")

    // None defers to LOG_STDERR in the config, so a console session and a
    // daemon can share one setting
    .def(py::init<const std::string&, std::optional<bool>>(),
         py::arg("config_path"), py::arg("log_to_stderr") = py::none())

    // Blocking commands release the GIL so a long exposure cannot stall the
    // rest of the Python process, which for a daemon means its whole RPC loop.
    .def("open",
         [](CameraSession &self, const std::string &args) {
           std::string retstring;
           const long status = (*self).connect_controller(args, retstring);
           return invoke(status, "open", retstring);
         },
         py::arg("args") = "", py::call_guard<py::gil_scoped_release>(),
         "Connect to the controller")

    .def("close",
         [](CameraSession &self, const std::string &args) {
           std::string retstring;
           const long status = (*self).disconnect_controller(args, retstring);
           return invoke(status, "close", retstring);
         },
         py::arg("args") = "", py::call_guard<py::gil_scoped_release>(),
         "Disconnect from the controller")

    .def("load",
         [](CameraSession &self, const std::string &args) {
           std::string retstring;
           const long status = (*self).load_firmware(args, retstring);
           return invoke(status, "load", retstring);
         },
         py::arg("args") = "", py::call_guard<py::gil_scoped_release>(),
         "Load firmware, defaulting to DEFAULT_FIRMWARE from the config")

    .def("power",
         [](CameraSession &self, const std::string &args) {
           std::string retstring;
           const long status = (*self).power(args, retstring);
           return invoke(status, "power", retstring);
         },
         py::arg("args") = "", py::call_guard<py::gil_scoped_release>(),
         "Query power state, or set it with \"on\" or \"off\"")

    .def("exptime",
         [](CameraSession &self, const std::string &args) {
           std::string retstring;
           const long status = (*self).exptime(args, retstring);
           return invoke(status, "exptime", retstring);
         },
         py::arg("args") = "", py::call_guard<py::gil_scoped_release>(),
         "Query the exposure time in seconds, or set it")

    .def("expose",
         [](CameraSession &self, const std::string &args) {
           std::string retstring;
           const long status = (*self).expose(args, retstring);
           return invoke(status, "expose", retstring);
         },
         py::arg("args") = "", py::call_guard<py::gil_scoped_release>(),
         "Take an exposure, or a counted series of them")

    .def("abort",
         [](CameraSession &self, const std::string &args) {
           std::string retstring;
           const long status = (*self).abort(args, retstring);
           return invoke(status, "abort", retstring);
         },
         py::arg("args") = "", py::call_guard<py::gil_scoped_release>(),
         "Abort the exposure in progress")

    .def("native",
         [](CameraSession &self, const std::string &args) {
           std::string retstring;
           const long status = (*self).native(args, retstring);
           return invoke(status, "native", retstring);
         },
         py::arg("args"), py::call_guard<py::gil_scoped_release>(),
         "Send a raw command straight to the controller")

    .def("is_instrument_command",
         [](CameraSession &self, const std::string &command) {
           return (*self).is_instrument_command(command);
         },
         py::arg("command"),
         "Return True if this build's instrument handles the named command")

    .def("instrument_commands",
         [](CameraSession &self) { return (*self).instrument_commands(); },
         "Return the instrument-specific command names this build supports")

    // Instrument commands stay a passthrough rather than one binding each, so
    // a build whose instrument gains a command exposes it with no change here.
    .def("instrument_cmd",
         [](CameraSession &self, const std::string &command, const std::string &args) {
           std::string retstring;
           const long status = (*self).instrument_cmd(command, args, retstring);
           return invoke(status, command, retstring);
         },
         py::arg("command"), py::arg("args") = "",
         py::call_guard<py::gil_scoped_release>(),
         "Run an instrument-specific command")

    .def("exposure_modes",
         [](CameraSession &self) { return (*self).get_exposure_modes(); },
         "Return the exposure mode names this build supports")

    // Snapshot, not a barrier: the FITS writer queues and drops by design
    .def("output_status",
         [](CameraSession &self) {
           py::list all;
           for (const auto &status : (*self).frame_output_status()) {
             py::dict entry;
             entry["name"]           = status.name;
             entry["frames_written"] = status.frames_written;
             entry["frames_dropped"] = status.frames_dropped;
             entry["last_written"]   = status.last_written;
             all.append(std::move(entry));
           }
           return all;
         },
         "Return a per-output snapshot of frames written, dropped, and last file");
}
