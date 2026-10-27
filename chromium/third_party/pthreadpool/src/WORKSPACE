workspace(name = "pthreadpool")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# LINT.IfChange(googletest)
# Google Test framework, used by most unit-tests.
http_archive(
    name = "com_google_googletest",
    sha256 = "a4cb11930215b071168811982dfbebc82a2bb0f90db0e8713245931eb742ea46",
    strip_prefix = "googletest-d72f9c8aea6817cdf1ca0ac10887f328de7f3da2",
    urls = ["https://github.com/google/googletest/archive/d72f9c8aea6817cdf1ca0ac10887f328de7f3da2.zip"],
)
# LINT.ThenChange(cmake/DownloadGoogleTest.cmake,MODULE.bazel:googletest)

# LINT.IfChange(benchmark)
# Google Benchmark library, used in micro-benchmarks.
http_archive(
    name = "com_google_benchmark",
    sha256 = "28c7cac12cc25d87d3dcc8cbdefa4b03c32d1a27bd50e37ca466d8127c1688d834800c38f3c587a396188ee5fb7d1bd0971b41a599a5c4787f8742cb39ca47db",
    strip_prefix = "benchmark-1.5.3",
    urls = ["https://github.com/google/benchmark/archive/v1.5.3.zip"],
)
# LINT.ThenChange(cmake/DownloadGoogleBenchmark.cmake,MODULE.bazel:benchmark)

# FXdiv library, used for repeated integer division by the same factor
http_archive(
    name = "FXdiv",
    strip_prefix = "FXdiv-63058eff77e11aa15bf531df5dd34395ec3017c8",
    sha256 = "3d7b0e9c4c658a84376a1086126be02f9b7f753caa95e009d9ac38d11da444db",
    urls = ["https://github.com/Maratyszcza/FXdiv/archive/63058eff77e11aa15bf531df5dd34395ec3017c8.zip"],
)

# Android NDK location and version is auto-detected from $ANDROID_NDK_HOME environment variable
android_ndk_repository(name = "androidndk")

# Android SDK location and API is auto-detected from $ANDROID_HOME environment variable
android_sdk_repository(name = "androidsdk")
