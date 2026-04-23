# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import datetime as dt
import logging
from typing import TYPE_CHECKING, ClassVar, Final, Optional, Self, Type

from typing_extensions import override

from crossbench.action_runner.action.enums import ReadyState
from crossbench.browsers.attributes import BrowserAttributes
from crossbench.probes.json import JsonResultProbe, JsonResultProbeContext

if TYPE_CHECKING:
  from crossbench.browsers.browser import Browser
  from crossbench.env.runner_env import RunnerEnv
  from crossbench.probes.probe import ProbeConfigParser
  from crossbench.runner.run import Run


class ChromeMetricsInternalsProbe(JsonResultProbe):
  """
  Probe that collects  structured metrics from
  chrome://metrics-internals/structured.
  """
  NAME: ClassVar = "chrome_metrics_internals"

  @classmethod
  @override
  def config_parser(cls) -> ProbeConfigParser[Self]:
    parser = super().config_parser()
    parser.add_argument(
        "project_name",
        aliases=("timestrip",),
        type=str,
        help="Project filter for structured metrics.")
    parser.add_argument(
        "event_name", type=str, help="Event filter for structured metrics.")
    parser.add_argument(
        "event_index",
        type=int,
        default=-1,
        help=("Index of the event to collect. Default is -1, "
              "which collects the last event."))
    parser.add_argument(
        "metric_key",
        type=str,
        required=True,
        help=("Field name of the metric to collect."))
    return parser

  def __init__(self, project_name: str, event_name: str, event_index: int,
               metric_key: str) -> None:
    super().__init__()
    self._project_name = project_name
    self._event_name = event_name
    self._event_index = event_index
    self._metric_key = metric_key

  @property
  def project_name(self) -> str:
    return self._project_name

  @property
  def event_name(self) -> str:
    return self._event_name

  @property
  def event_index(self) -> int:
    return self._event_index

  @property
  def metric_key(self) -> str:
    return self._metric_key

  def validate_browser(self, env: RunnerEnv, browser: Browser) -> None:
    super().validate_browser(env, browser)
    self.expect_browser(browser, BrowserAttributes.CHROMIUM_BASED)

  def get_context_cls(self) -> Type[ChromeMetricsInternalsProbeContext]:
    return ChromeMetricsInternalsProbeContext


class ChromeMetricsInternalsProbeContext(
    JsonResultProbeContext[ChromeMetricsInternalsProbe]):

  # JS code that overrides the chrome.send response handler and requests
  # structured metrics.
  STRUCTURED_METRICS_SEND: Final[str] = """
function webUIResponse(id, isSuccess, response) {
  if (id === "crossbench_structured_metrics_1") {
    window.crossbench_structured_metrics = response;
  }
}
window.cr.webUIResponse = webUIResponse;
chrome.send("fetchStructuredMetricsEvents", ["crossbench_structured_metrics_1"]);
"""

  # JS code that checks if there is a structured metrics response.
  STRUCTURED_METRICS_WAIT: Final[
      str] = "return !!window.crossbench_structured_metrics"

  # JS code that returns the structured metrics response.
  STRUCTURED_METRICS_DATA: Final[
      str] = "return window.crossbench_structured_metrics"

  def __init__(self, probe: ChromeMetricsInternalsProbe, run: Run) -> None:
    super().__init__(probe, run)
    self._metric_value: Optional[int] = None

  def _read_metric_key(self, label: str) -> Optional[int]:
    with self.run.actions(
        f"Probe({self.probe.name}) dump structured metrics {label}") as actions:
      actions.show_url(
          f"chrome://metrics-internals/structured?project={self.probe.project_name}&event={self.probe.event_name}",
          ready_state=ReadyState.COMPLETE,
          target="_new_tab",
          timeout=dt.timedelta(seconds=10))
      actions.js(self.STRUCTURED_METRICS_SEND)
      actions.wait_js_condition(self.STRUCTURED_METRICS_WAIT, 0.1, timeout=10.0)
      data = actions.js(self.STRUCTURED_METRICS_DATA)
      if not data:
        raise RuntimeError(f"No structured metrics data fetched for {label}")
      metrics = data[self.probe.event_index]["metrics"]
      metrics_dict = {m["key"]: m["value"] for m in metrics}
      if self.probe.metric_key not in metrics_dict:
        logging.error("Metric key %s not found in structured metrics for %s",
                      self.probe.metric_key, label)
        logging.error("Available metrics: %s", list(metrics_dict.keys()))
        return None
      self.run.browser.close_tab(relative_tab_index=-1)
      return int(metrics_dict[self.probe.metric_key])

  def start(self) -> None:
    logging.info("No structured metrics are present at start.")
    super().start()

  def stop(self) -> None:
    self._metric_value = self._read_metric_key("stop")
    super().stop()
