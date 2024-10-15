# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import contextlib
import email.parser
import http.server
import json
import logging
import threading
from typing import TYPE_CHECKING, Final, Iterator, Mapping, Optional, Type

from immutabledict import immutabledict

from crossbench import plt
from crossbench.network.base import Network, TrafficShaper

if TYPE_CHECKING:
  from crossbench.path import LocalPath
  from crossbench.runner.groups.session import BrowserSessionRunGroup

# List of known headers that are served by the default HTTPServer and might
# be accidentally overridden by provided extra headers.
CONFLICTING_EXTRA_HEADERS: Final[frozenset[str]] = frozenset(
    map(lambda header: header.lower(),
        ("Content-Type", "Content-Length", "Last-Modified", "Server", "Date",
         "Connection", "Location")))


class CustomHeadersRequestHandler(http.server.SimpleHTTPRequestHandler):

  @classmethod
  def bind(
      cls,
      server_dir: LocalPath,
      extra_headers: Mapping[str, str],
  ) -> Type[http.server.SimpleHTTPRequestHandler]:
    # Use a temporary class to bind arguments.
    class BoundDirectoryRequestHandler(cls):

      def __init__(self, *args, **kwargs):
        super().__init__(
            *args,
            directory=str(server_dir),
            extra_headers=extra_headers,
            **kwargs)

    return BoundDirectoryRequestHandler

  def __init__(self,
               *args,
               directory: Optional[str] = None,
               extra_headers: Optional[Mapping[str, str]] = None,
               **kwargs):
    self._extra_headers: immutabledict[str, str] = (
        immutabledict(extra_headers) if extra_headers else immutabledict())
    super().__init__(*args, directory=directory, **kwargs)

  def end_headers(self):
    if self._extra_headers:
      self._send_custom_headers()
    super().end_headers()

  def _send_custom_headers(self):
    for key, value in self._extra_headers.items():
      self.send_header(key, value)


class LocalFileNetwork(Network):

  def __init__(self,
               path: LocalPath,
               traffic_shaper: Optional[TrafficShaper] = None,
               browser_platform: plt.Platform = plt.PLATFORM):
    super().__init__(traffic_shaper, browser_platform)
    self._path = path
    # TODO: support custom host
    self._host = "localhost"
    # TODO: add more arg parsing support
    self._port = 8000
    # TODO: support custom headers via command line
    self._extra_headers: immutabledict[str, str] = self._try_parse_headers()
    if self._extra_headers:
      self._validate_extra_headers()

  @property
  def path(self) -> LocalPath:
    return self._path

  def _try_parse_headers(self) -> immutabledict[str, str]:
    for name in ("HEADERS", "HEADERS.txt"):
      header_file = self._path / name
      if header_file.exists():
        return self._read_headers_file(header_file)
    return immutabledict()

  def _read_headers_file(self,
                         header_file: LocalPath) -> immutabledict[str, str]:
    with header_file.open("rb") as f:
      # Reuse python's email message library to parse headers
      message = email.parser.BytesParser().parsebytes(f.read())
      return immutabledict(message)

  def _validate_extra_headers(self):
    for key, value in self._extra_headers.items():
      if key.lower() in CONFLICTING_EXTRA_HEADERS:
        logging.error(
            "BROWSER Network: Extra header overrides server defaults: '%s: %s'",
            key, value)

  @contextlib.contextmanager
  def open(self, session: BrowserSessionRunGroup) -> Iterator[Network]:
    with super().open(session):
      with self._open_local_file_server():
        # TODO: properly hook up traffic shaper for the local http server
        with self._traffic_shaper.open(self, session):
          yield self

  @contextlib.contextmanager
  def _open_local_file_server(self):
    # TODO: write request log file to session results folder.
    # TODO: support  https server using SSLContext.wrap_socket(httpd.socket)
    request_handler_cls = CustomHeadersRequestHandler.bind(
        self._path, self._extra_headers)
    server = http.server.ThreadingHTTPServer((self._host, self._port),
                                             request_handler_cls)
    with self._server_thread(server):
      yield

  @contextlib.contextmanager
  def _server_thread(self, server: http.server.HTTPServer) -> Iterator[None]:
    with server:
      server_thread = threading.Thread(target=server.serve_forever)
      server_thread.daemon = True
      server_thread.start()
      try:
        yield
      finally:
        server.shutdown()
        server_thread.join()

  def __str__(self) -> str:
    extra_headers_str = ""
    if self._extra_headers:
      formatted_headers = json.dumps(dict(self._extra_headers))
      extra_headers_str = f" extra_headers={formatted_headers}"
    return ("LOCAL(path={self._path}, "
            f"speed={self.traffic_shaper}{extra_headers_str})")
