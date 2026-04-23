# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import os
import shutil
import zipfile
from typing import TYPE_CHECKING, ClassVar, Final, Iterable, Mapping, \
    Optional, Type

from typing_extensions import override

from crossbench.browsers.downloader import Downloader
from crossbench.browsers.webkit.version import WebKitVersion

if TYPE_CHECKING:
  from crossbench.browsers.version import BrowserVersion
  from crossbench.path import AnyPathLike, LocalPath
  from crossbench.plt.base import Platform

_MACOS_NAME_LOOKUP: Final[Mapping[int, str]] = {
    14: "mac-sonoma-x86_64%20arm64-release",
    15: "mac-sequoia-x86_64%20arm64-release"
}


class WebKitDownloader(Downloader):

  @classmethod
  @override
  def name(cls) -> str:
    return "WebKit-Nightly"

  @classmethod
  @override
  def _get_loader_cls(
      cls, browser_platform: Platform) -> Type[WebKitDownloader] | None:
    if browser_platform.is_macos:
      return WebKitDownloaderMacOS
    return None

  @classmethod
  @override
  def is_valid_version(cls, path_or_identifier: str) -> bool:
    return WebKitVersion.is_valid_unique(path_or_identifier)

  @classmethod
  def _is_valid(cls, path_or_identifier: AnyPathLike,
                browser_platform: Platform) -> bool:
    if cls.is_valid_version(str(path_or_identifier)):
      return True
    path = browser_platform.path(path_or_identifier)
    return (browser_platform.exists(path) and
            path.name.endswith(cls.ARCHIVE_SUFFIX))

  def __init__(self, version_identifier: str | LocalPath, browser_type: str,
               platform_name: str, browser_platform: Platform) -> None:
    assert not browser_type
    assert not platform_name
    if not browser_platform.is_macos:
      raise ValueError("Unsupported platform for downloading webkit nightly: "
                       f"{browser_platform}")
    # TODO: use platform.major_version to get the right name:
    # webkit_platform_name = _MACOS_NAME_LOOKUP.get(browser_platform.version)
    webkit_platform_name = _MACOS_NAME_LOOKUP.get(15)
    if not webkit_platform_name:
      raise ValueError("Unsupported macos platform version for downloading "
                       f"webkit nightly: {browser_platform}")
    super().__init__(version_identifier, "webkit-nightly", webkit_platform_name,
                     browser_platform)

  @override
  def _parse_version(self, version_identifier: str) -> BrowserVersion:
    return WebKitVersion.parse(version_identifier)


class WebKitDownloaderMacOS(WebKitDownloader):
  BASE_URL: ClassVar[
      str] = "https://s3-us-west-2.amazonaws.com/minified-archives.webkit.org/"
  ARCHIVE_SUFFIX: ClassVar[str] = ".zip"

  @classmethod
  @override
  def is_valid(cls, path_or_identifier: AnyPathLike,
               browser_platform: Platform) -> bool:
    return cls._is_valid(path_or_identifier, browser_platform)

  @override
  def _requested_version_validation(self) -> None:
    pass

  @override
  def _find_archive_url(self) -> tuple[BrowserVersion, Optional[str]]:
    if not self.requested_version.is_complete:
      raise NotImplementedError(
          "Only full webkit version identifiers are supported.")
    folder_url = f"{self.BASE_URL}{self._platform_name}/"
    # We are not validating the URL here, since we don't have gsutil access
    # and there is no index page
    return tuple(self._archive_urls(folder_url, self.requested_version))[0]

  @override
  def _download_archive(self, archive_url: str, tmp_dir: LocalPath) -> None:
    self.host_platform.download_to(archive_url,
                                   tmp_dir / f"archive{self.ARCHIVE_SUFFIX}")
    archive_candidates = list(tmp_dir.glob("*"))
    assert len(archive_candidates) == 1, (
        f"Download tmp dir contains more than one file: {tmp_dir} "
        f"{archive_candidates}")
    candidate = archive_candidates[0]
    assert not self._archive_path.exists(), (
        f"Archive was already downloaded: {self._archive_path}")
    shutil.move(os.fspath(candidate), os.fspath(self._archive_path))

  @override
  def _archive_urls(
      self, folder_url: str,
      version: BrowserVersion) -> Iterable[tuple[BrowserVersion, str]]:
    archive_name = f"{version.parts_str}@main{self.ARCHIVE_SUFFIX}"
    return ((version, f"{folder_url}{archive_name}"),)

  @override
  def _installed_app_path(self) -> LocalPath:
    return self._extracted_path() / "Release" / "MiniBrowser.app"

  @override
  def _install_archive(self, archive_path: LocalPath) -> None:
    extracted_path = self._extracted_path()
    with zipfile.ZipFile(archive_path, "r") as zip_file:
      zip_file.extractall(extracted_path)
    assert self.host_platform.is_dir(extracted_path), (
        f"Could not extract {archive_path} into {extracted_path}")
