# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import warnings
from functools import cache

from google import auth as google_auth
from google.auth.transport import requests as auth_requests

from crossbench import plt
from crossbench.cli import ui
from crossbench.pinpoint.helper import annotate


@cache
def get_auth_session() -> auth_requests.AuthorizedSession:
  # TODO(b/455510346): Figure out how to fix the quota warning properly.
  warnings.filterwarnings("ignore", module="google.auth._default")
  try:
    # TODO(b/455510346): Make sure it supports @chromium.org accounts.
    with annotate("Authenticating"):
      credentials, _ = google_auth.default(
          scopes=["https://www.googleapis.com/auth/userinfo.email"])
      return auth_requests.AuthorizedSession(credentials)
  except google_auth.exceptions.DefaultCredentialsError:
    user_input = ui.prompt(
        "Authentication failed. Please run 'gcloud auth application-default login' "
        "to configure your credentials.\n"
        "Would you like to run it now?", "[Y/n] ").lower().strip()
    if user_input in ["", "y", "yes"]:
      plt.PLATFORM.sh(
          "gcloud", "auth", "application-default", "login", check=True)
      return get_auth_session()
    raise
