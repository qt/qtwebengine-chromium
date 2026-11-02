// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pybind11/pybind11.h>

#include <memory>
#include <stdexcept>

#include "cast/standalone_sender/bindings/python/cast_runtime.h"
#include "cast/standalone_sender/bindings/python/sender_bridge.h"

namespace py = pybind11;

using openscreen::cast::CastRuntime;
using openscreen::cast::CastSenderBridge;

PYBIND11_MODULE(cast_sender, m) {
  m.doc() = "Python bindings for Open Screen Standalone Cast Sender.";

  py::class_<CastRuntime, std::shared_ptr<CastRuntime>>(m, "CastRuntime")
      .def(py::init([]() {
             static std::weak_ptr<CastRuntime> weak_instance;
             if (weak_instance.lock()) {
               throw std::runtime_error("CastRuntime already exists.");
             }
             auto instance = std::make_shared<CastRuntime>();
             weak_instance = instance;
             return instance;
           }),
           "Initialize the OpenScreen environment event loop.");

  py::class_<CastSenderBridge::StreamConfig> stream_config(
      m, "StreamConfig", "Configuration options for streaming.");
  stream_config.def(py::init<>(), "Initialize the config.");
  stream_config.def_readwrite("file_path",
                              &CastSenderBridge::StreamConfig::file_path,
                              "Absolute path to the video file to stream.");
  stream_config.def_readwrite(
      "remote_transport_id",
      &CastSenderBridge::StreamConfig::remote_transport_id,
      "The remote transport ID to use.");
  stream_config.def_readwrite("session_id",
                              &CastSenderBridge::StreamConfig::session_id,
                              "The session ID to use.");
  stream_config.def_readwrite("codec_name",
                              &CastSenderBridge::StreamConfig::codec_name,
                              "The codec to use.");
  stream_config.def_readwrite("max_bitrate",
                              &CastSenderBridge::StreamConfig::max_bitrate,
                              "The maximum bitrate to use.");
  stream_config.def_readwrite(
      "should_include_audio",
      &CastSenderBridge::StreamConfig::should_include_audio,
      "Whether to include audio.");

  py::class_<CastSenderBridge>(m, "CastSender")
      .def(py::init([](CastRuntime& env, std::string_view ip_address, int port,
                       std::string_view cert_path) {
             const openscreen::ErrorOr<openscreen::IPAddress> ip =
                 openscreen::IPAddress::Parse(ip_address);
             if (!ip.is_value()) {
               throw std::invalid_argument("Invalid IP address provided: " +
                                           std::string(ip_address));
             }
             return std::make_unique<CastSenderBridge>(
                 env.task_runner(), ip.value(), port, cert_path);
           }),
           py::arg("env"), py::arg("ip_address"), py::arg("port"),
           py::arg("cert_path") = "", py::keep_alive<1, 2>(),
           "Initialize CastSender with target receiver IP, port, and "
           "optional cert path.")
      .def(
          "stream_video",
          [](CastSenderBridge& self,
             const CastSenderBridge::StreamConfig& config, bool is_debug) {
            bool success = self.StreamVideo(config, is_debug);
            if (!success) {
              throw std::runtime_error(
                  "StreamVideo failed to initialize or execute. See logs for "
                  "details.");
            }
            return true;
          },
          py::arg("config"), py::arg("is_debug") = false,
          py::call_guard<py::gil_scoped_release>(),
          "Establish connection and stream video file. Safely releases the "
          "Python GIL during execution.")
      .def("teardown", &CastSenderBridge::Teardown,
           py::call_guard<py::gil_scoped_release>(),
           "Teardown the streaming task runner and gracefully stop active "
           "streaming.");
}
