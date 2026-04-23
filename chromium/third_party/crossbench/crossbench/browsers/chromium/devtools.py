# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import contextlib
import datetime as dt
import json
import logging
from contextlib import closing
from typing import TYPE_CHECKING, Any, Callable, Iterator, Self, Tuple
import websocket
from websocket import create_connection

if TYPE_CHECKING:
  from crossbench.browsers.browser import Browser
  from crossbench.plt.base import Platform


class DevToolsInBrowserClient():
  """Manages communication with the Chrome DevTools Protocol from within
     the browser context.
  """

  def open_frontend(self, browser: Browser, panel_name: str) -> None:
    with closing(create_connection(browser.ws_endpoint)) as ws:
      ws.send(
          json.dumps({
              "id": 1,  # short lived connection, id can be anything
              "method": "Target.openDevTools",
              "params": {
                  "targetId": browser.current_window_id(),
                  "panelId": panel_name
              },
          }))
      result = json.loads(ws.recv())
      if "result" not in result:
        raise RuntimeError(f"Failed to open DevTools. Response: {result}")
      if "targetId" not in result["result"]:
        raise RuntimeError(f"Failed to open DevTools, no targetId: {result}")
      self._devtools_window_id = result["result"]["targetId"]


class DevToolsRemoteClient:
  """Manages communication with the Chrome DevTools Protocol."""

  def __init__(self,
               platform: Platform,
               requested_local_port: int = 0,
               remote_devtools_identifier: str = "chrome_devtools_remote"):
    self._platform: Platform = platform
    self._requested_local_port: int = requested_local_port
    self._remote_devtools_identifier: str = remote_devtools_identifier
    self._ws: websocket.WebSocket | None = None
    self._devtools_port: int = 0

  def connect(self) -> None:
    """Establishes a WebSocket connection to the DevTools service."""
    if self._ws and self._ws.connected:
      return
    try:
      self._devtools_port = self._platform.ports.forward_devtools(
          local_port=self._requested_local_port,
          remote_identifier=self._remote_devtools_identifier)
      self._ws = websocket.WebSocket()
      self._ws.connect(
          f"ws://localhost:{self._devtools_port}/devtools/browser/")
      logging.debug("DevTools connected: ws://localhost:%s/devtools/browser/",
                    self._devtools_port)
    except (websocket.WebSocketException, ConnectionRefusedError,
            TimeoutError) as e:
      logging.error("DevTools connection error: %s", e)
      self._disconnect_internal()
      raise
    except Exception as e:
      logging.error("Unexpected error during DevTools connection: %s", e)
      self._disconnect_internal()
      raise

  def _disconnect_internal(self) -> None:
    if self._ws and self._ws.connected:
      try:
        self._ws.close()
      except websocket.WebSocketException as e:
        logging.warning("Error closing DevTools WebSocket: %s", e)
    self._ws = None
    if self._devtools_port:
      try:
        self._platform.ports.stop_forward(self._devtools_port)
      except Exception as e:  # noqa: BLE001
        # Best effort to remove forwarding, log if it fails but don't crash
        logging.warning(
            "Error removing DevTools port forwarding for port %s: %s",
            self._devtools_port, e)
    self._devtools_port = 0

  def disconnect(self) -> None:
    """Closes the WebSocket connection and removes port forwarding."""
    self._disconnect_internal()
    logging.debug("DevTools disconnected")

  def send_command(self, command_payload: dict[str, Any]) -> Tuple[bool, dict]:
    """Sends a command to DevTools and checks the response ID.

    Args:
      command_payload: The command payload to send. Must include an 'id'.

    Returns:
      Tuple of [bool, dict]
      bool: True if the command was sent successfully and the response ID
            matches, False otherwise.
      dict: the full response message returned by the websocket. Empty on error.
    """
    if not self._ws or not self._ws.connected:
      logging.error("DevTools is not connected. Cannot send command.")
      return False, {}

    expected_id = command_payload.get("id")
    if expected_id is None:
      logging.error("DevTools command requires an 'id' in the payload.")
      return False, {}

    try:
      self._ws.send(json.dumps(command_payload).encode("utf-8"))
      data = self._ws.recv()
      response = json.loads(data)
      return response.get("id") == expected_id, response
    except (websocket.WebSocketException, ConnectionRefusedError,
            TimeoutError) as e:
      logging.error("DevTools communication error: %s", e)
      return False, {}
    except json.JSONDecodeError as e:
      logging.error("Error decoding JSON response from DevTools: %s", e)
      return False, {}

  def dispatch_command(self, command_payload: dict[str, Any]) -> bool:
    """Dispatches a command to DevTools. Does not wait for any response.

    Args:
      command_payload: The command payload to send. Must include an 'id'.

    Returns:
      bool: True if the command was sent successfully, False otherwise.
    """
    if not self._ws or not self._ws.connected:
      logging.error("DevTools is not connected. Cannot send command.")
      return False

    expected_id = command_payload.get("id")
    if expected_id is None:
      logging.error("DevTools command requires an 'id' in the payload.")
      return False

    try:
      self._ws.send(json.dumps(command_payload).encode("utf-8"))
      return True
    except (websocket.WebSocketException, ConnectionRefusedError,
            TimeoutError) as e:
      logging.error("DevTools communication error: %s", e)
      return False
    except json.JSONDecodeError as e:
      logging.error("Error decoding JSON response from DevTools: %s", e)
      return False

  def poll_for_response(
      self,
      condition_fn: Callable[[], bool],
      process_fn: Callable[[dict], None],
      timeout: dt.timedelta = dt.timedelta(seconds=1),
  ) -> bool:
    """Polls for DevTools events and processes them until a condition is met or
       a timeout occurs.

    Args:
      condition_fn: A boolean function that determines whether we should
                    continue polling for more events. Polling stops when this
                    function returns False.
      process_fn:   Function that takes each response as input for
                    processing.
      timeout:      Total number of seconds to poll for events before timing
                    out.

    Returns:
      bool: True if the condition was met (condition_fn returned False),
            False if the timeout was reached.
    """
    if not self._ws or not self._ws.connected:
      logging.error("DevTools is not connected. Cannot poll events.")
      return False
    deadline = dt.datetime.now() + timeout
    try:
      self._ws.settimeout(timeout.total_seconds())
      while condition_fn():
        data = self._ws.recv()
        response = json.loads(data)
        process_fn(response)
        if dt.datetime.now() > deadline:
          return False
        self._ws.settimeout((deadline - dt.datetime.now()).total_seconds())
    except (TimeoutError, json.JSONDecodeError):
      return False
    finally:
      self._ws.settimeout(None)
    return True

  @contextlib.contextmanager
  def open(self) -> Iterator[Self]:
    self.connect()
    try:
      yield self
    finally:
      self.disconnect()
