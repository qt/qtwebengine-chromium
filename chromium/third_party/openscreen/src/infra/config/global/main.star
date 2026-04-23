#!/usr/bin/env lucicfg
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Open Screen's LUCI configuration for post-submit and pre-submit builders.
"""

REPO_URL = "https://chromium.googlesource.com/openscreen"
CHROMIUM_REPO_URL = "https://chromium.googlesource.com/chromium/src"
MAC_VERSION = "Mac-15"
WINDOWS_VERSION = "Windows-10"
LINUX_VERSION = "Ubuntu-24.04"
REF = "refs/heads/main"

SISO_PROPERTY = "$build/siso"

# Use LUCI Scheduler BBv2 names and add Scheduler realms configs.
lucicfg.enable_experiment("crbug.com/1182002")

luci.project(
    name = "openscreen",
    milo = "luci-milo.appspot.com",
    buildbucket = "cr-buildbucket.appspot.com",
    logdog = "luci-logdog.appspot.com",
    swarming = "chromium-swarm.appspot.com",
    scheduler = "luci-scheduler.appspot.com",
    acls = [
        acl.entry(
            roles = [
                acl.BUILDBUCKET_READER,
                acl.SCHEDULER_READER,
                acl.PROJECT_CONFIGS_READER,
                acl.LOGDOG_READER,
            ],
            groups = "all",
        ),
        acl.entry(
            roles = acl.SCHEDULER_OWNER,
            groups = "project-openscreen-admins",
        ),
        acl.entry(
            roles = acl.LOGDOG_WRITER,
            groups = "luci-logdog-chromium-writers",
        ),
        acl.entry(
            roles = acl.CQ_COMMITTER,
            groups = "project-openscreen-committers",
        ),
        acl.entry(
            roles = acl.CQ_DRY_RUNNER,
            groups = "project-openscreen-tryjob-access",
        ),
    ],
)

luci.milo(
    logo = ("https://storage.googleapis.com/chrome-infra-public/logo/" +
            "openscreen-logo.png"),
)

luci.logdog(gs_bucket = "chromium-luci-logdog")

# Gitiles pollers are used for triggering CI builders.
luci.gitiles_poller(
    name = "main-gitiles-trigger",
    bucket = "ci",
    repo = REPO_URL,
)
luci.gitiles_poller(
    name = "chromium-trigger",
    bucket = "ci",
    repo = CHROMIUM_REPO_URL,
)

# Whereas tryjob verifiers are used for triggering try builders.
luci.cq_group(
    name = "openscreen-build-config",
    watch = cq.refset(
        repo = REPO_URL,
        refs = ["refs/heads/.+"],
    ),
)
luci.cq(status_host = "chromium-cq-status.appspot.com")

luci.bucket(
    name = "try",
    acls = [
        acl.entry(
            roles = [acl.BUILDBUCKET_TRIGGERER, acl.CQ_COMMITTER],
            groups = [
                "project-openscreen-tryjob-access",
                "service-account-cq",
            ],
        ),
    ],
)
luci.bucket(
    name = "try.shadow",
    shadows = "try",
    constraints = luci.bucket_constraints(
        pools = ["luci.flex.try"],
        service_accounts = [
            "openscreen-try-builder@chops-service-accounts.iam.gserviceaccount.com",
        ],
    ),
    bindings = [
        # For led permissions.
        luci.binding(
            roles = "role/buildbucket.creator",
            groups = [
                "mdb/chrome-build-access-sphinx",
            ],
        ),
    ],
    dynamic = True,
)
luci.bucket(
    name = "ci",
    acls = [
        acl.entry(
            roles = [acl.BUILDBUCKET_TRIGGERER],
            users = "luci-scheduler@appspot.gserviceaccount.com",
        ),
    ],
)
luci.bucket(
    name = "ci.shadow",
    shadows = "ci",
    constraints = luci.bucket_constraints(
        pools = ["luci.flex.ci"],
        service_accounts = [
            "openscreen-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
        ],
    ),
    bindings = [
        # For led permissions.
        luci.binding(
            roles = "role/buildbucket.creator",
            groups = [
                "mdb/chrome-build-access-sphinx",
            ],
        ),
    ],
    dynamic = True,
)

luci.console_view(
    name = "ci",
    title = "OpenScreen CI Builders",
    repo = REPO_URL,
)
luci.console_view(
    name = "try",
    title = "OpenScreen Try Builders",
    repo = REPO_URL,
)

_siso = struct(
    project = struct(
        DEFAULT_TRUSTED = "rbe-chromium-trusted",
        DEFAULT_UNTRUSTED = "rbe-chromium-untrusted",
    ),
)

def get_properties(
        target_cpu,
        is_debug = True,
        is_gcc = False,
        is_asan = False,
        is_tsan = False,
        is_msan = False,
        use_coverage = False,
        cast_receiver = False,
        chromium = False,
        is_presubmit = False,
        is_component_build = None,
        is_ci = None):
    """Property generator method, used to configure the build system.

    Args:
      target_cpu: the target CPU. May differ from current_cpu or host_cpu
        if cross compiling.
      is_debug: if False, the build mode is release instead of debug.
      is_gcc: if True, the GCC compiler is used instead of clang.
      is_asan: if True, this is an address sanitizer build.
      is_msan: if True, this is a memory sanitizer build.
      is_tsan: if True, this is a thread sanitizer build.
      use_coverage: if True, this is a code coverage build.
      cast_receiver: if True, this build should include the cast standalone
        sender and receiver binaries.
      chromium: if True, the build is for use in an embedder, such as Chrome.
      is_presubmit: if True, this is a presubmit run.
      is_component_build: if set, enables or disables component builds.
      is_ci: If set, it adds is_ci flag to the properties.

    Returns:
        A collection of GN properties for the build system.
    """
    properties = {
        "clang_use_chrome_plugins": False,
        "target_cpu": target_cpu,
        "$recipe_engine/swarming": {
            "server": "https://chromium-swarm.appspot.com",
        },
    }
    if not is_debug:
        properties["is_debug"] = False
    if is_gcc:
        properties["is_clang"] = False
        properties["use_custom_libcxx"] = False
    if is_asan:
        properties["is_asan"] = True
    if is_msan:
        properties["is_msan"] = True
    if is_tsan:
        properties["is_tsan"] = True
    if use_coverage:
        properties["use_coverage"] = True
    if cast_receiver:
        properties["have_ffmpeg"] = True
        properties["have_libsdl2"] = True
        properties["have_libopus"] = True
        properties["have_libvpx"] = True
    if chromium:
        properties["builder_group"] = "client.openscreen.chromium"
        properties[SISO_PROPERTY] = {
            "configs": ["builder"],
            "enable_cloud_monitoring": True,
            "enable_cloud_profiler": True,
            "enable_cloud_trace": True,
            "metrics_project": "chromium-reclient-metrics",
            "project": _siso.project.DEFAULT_UNTRUSTED,
        }

    if is_presubmit:
        properties["repo_name"] = "openscreen"
        properties["runhooks"] = "true"

    if is_component_build != None:
        properties["is_component_build"] = is_component_build

    if is_ci:
        properties["is_ci"] = is_ci
    return properties

def builder(builder_type, name, os, cpu, properties):
    """Defines a builder.

    Args:
      builder_type: "ci" or "try".
      name: name of the builder to define.
      os: the target operating system.
      cpu: the target architecture, such as "arm64."
      properties: configuration to be passed to GN.
    """
    recipe_id = "openscreen"
    if properties:
        if "builder_group" in properties:
            recipe_id = "chromium"
        elif "runhooks" in properties:
            recipe_id = "run_presubmit"

    caches = []
    if os == MAC_VERSION:
        caches.append(swarming.cache("osx_sdk"))

    triggers = None
    if builder_type == "ci":
        triggers = ["chromium-trigger" if recipe_id == "chromium" else "main-gitiles-trigger"]

    luci.builder(
        name = name,
        bucket = builder_type,
        executable = luci.recipe(
            name = recipe_id,
            recipe = recipe_id,
            cipd_package =
                "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build",
            cipd_version = "refs/heads/main",
            use_bbagent = True,
        ),
        dimensions = {
            "pool": "luci.flex." + builder_type,
            "os": os,
            "cpu": cpu,
        },
        caches = caches,
        properties = properties,
        service_account =
            "openscreen-{}-builder@chops-service-accounts.iam.gserviceaccount.com"
                .format(builder_type),
        triggered_by = triggers,
    )

    # CI jobs get triggered by |triggers|, try jobs get trigged by the commit
    # queue instead.
    if builder_type == "try":
        # We mark some bots as experimental to not block the build.
        experiment_percentage = None
        if name in [
            "linux_arm64",
            "linux_arm64_cast_receiver",
            "linux_x64_coverage",
            "win_x64",
            "chromium_win_x64",
        ]:
            experiment_percentage = 100

        luci.cq_tryjob_verifier(
            builder = "try/" + name,
            cq_group = "openscreen-build-config",
            experiment_percentage = experiment_percentage,
        )

    luci.console_view_entry(
        builder = "{}/{}".format(builder_type, name),
        console_view = builder_type,
        category = "{}|{}".format(os, cpu),
        short_name = name,
    )

def ci_builder(name, os, cpu, properties):
    """Defines a post submit builder.

       Args:
        name: name of the builder to define.
        os: the target operating system.
        cpu: the target central processing unit.
        properties: configuration to be passed to GN.
    """
    builder("ci", name, os, cpu, properties)

def try_builder(name, os, cpu, properties):
    """Defines a pre submit builder.

    Args:
      name: name of the builder to define.
      os: the target operating system.
      cpu: the target central processing unit.
      properties: configuration to be passed to GN.
    """
    builder("try", name, os, cpu, properties)

def try_and_ci_builders(name, os, cpu, properties):
    """Defines a similarly configured try and ci builder pair.

    Args:
      name: name of the builder to define.
      os: the target operating system.
      cpu: the target central processing unit.
      properties: configuration to be passed to GN.
    """
    try_builder(name, os, cpu, properties)

    ci_properties = dict(properties)
    ci_properties["is_ci"] = True
    if SISO_PROPERTY in ci_properties:
        ci_properties[SISO_PROPERTY] = dict(ci_properties[SISO_PROPERTY])
        ci_properties[SISO_PROPERTY]["project"] = _siso.project.DEFAULT_TRUSTED
    ci_builder(name, os, cpu, ci_properties)

# BUILDER CONFIGURATIONS
# Follow the pattern: <platform>_<arch>
# For builders other than the generic debug config, use <platform>_<arch>_<config>
# For Chromium builders, use chromium_<platform>_<arch>_<config>

try_builder(
    "openscreen_presubmit",
    LINUX_VERSION,
    "x86-64",
    get_properties("x64", is_presubmit = True, is_debug = False),
)
try_and_ci_builders(
    "linux_arm64_cast_receiver",
    LINUX_VERSION,
    # This bot relies on cross-compilation.
    "x86-64",
    get_properties("arm64", cast_receiver = True, is_component_build = False),
)
try_and_ci_builders("linux_x64_coverage", LINUX_VERSION, "x86-64", get_properties("x64", use_coverage = True))
try_and_ci_builders("linux_x64", LINUX_VERSION, "x86-64", get_properties("x64", is_asan = True))
try_and_ci_builders(
    "linux_x64_gcc",
    LINUX_VERSION,
    "x86-64",
    get_properties("x64", is_gcc = True),
)
try_and_ci_builders(
    "linux_x64_msan_rel",
    LINUX_VERSION,
    "x86-64",
    get_properties("x64", is_debug = False, is_msan = True),
)
try_and_ci_builders(
    "linux_x64_tsan_rel",
    LINUX_VERSION,
    "x86-64",
    get_properties("x64", is_debug = False, is_tsan = True),
)
try_and_ci_builders(
    "linux_arm64",
    LINUX_VERSION,
    # This bot relies on cross-compilation.
    "x86-64",
    get_properties("arm64", is_component_build = False),
)
try_and_ci_builders("mac_arm64", MAC_VERSION, "arm64", get_properties("arm64"))
try_and_ci_builders(
    "win_x64",
    WINDOWS_VERSION,
    "x86-64",
    get_properties("x64"),
)
try_and_ci_builders(
    "chromium_linux_x64",
    LINUX_VERSION,
    "x86-64",
    get_properties("x64", chromium = True),
)
try_and_ci_builders(
    "chromium_mac_arm64",
    MAC_VERSION,
    "arm64",
    get_properties("arm64", chromium = True),
)
try_and_ci_builders(
    "chromium_win_x64",
    WINDOWS_VERSION,
    "x86-64",
    get_properties("x64", chromium = True),
)
