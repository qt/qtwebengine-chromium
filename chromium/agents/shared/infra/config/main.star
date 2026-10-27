#!/usr/bin/env lucicfg
#
# This is the LUCI configuration for the 'agents' project,
# that provides public agentic skills and configurations for chromium.
#
# The agents repository needs basically the simplest possible LUCI project:
# one presubmit (aka "try") builder with the following conventions:
#
# This is modeled after https://chromium.googlesource.com/website/+/refs/heads/main/infra/config/main.star.

load("@chromium-luci//builders.star", "os")

PROJECT_NAME = "chromium-agents"
PROJECT_REPO = "https://chromium.googlesource.com/chromium/agents"
# TODO(crbug.com/1457690): Update this when/if you get a custom logo.
PROJECT_LOGO = "https://storage.googleapis.com/chrome-infra-public/logo/chromium.svg"
RECIPE_CIPD_PACKAGE = "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build"
RECIPE_NAME = "run_presubmit"

lucicfg.check_version("1.46.3", "Please update depot_tools")

# Use LUCI Scheduler BBv2 names and add Scheduler realms configs.
lucicfg.enable_experiment("crbug.com/1182002")
lucicfg.config(
    config_dir = "generated",
    tracked_files = [
        "commit-queue.cfg",
        "cr-buildbucket.cfg",
        "project.cfg",
        "luci-logdog.cfg",
        "luci-milo.cfg",
        "luci-scheduler.cfg",
        "realms.cfg",
    ],
    fail_on_warnings = True,
)
luci.project(
    name = PROJECT_NAME,
    buildbucket = "cr-buildbucket.appspot.com",
    logdog = "luci-logdog",
    milo = "luci-milo",
    scheduler = "luci-scheduler",
    swarming = "chromium-swarm.appspot.com",
    acls = [
        acl.entry(
            [
                acl.BUILDBUCKET_READER,
                acl.LOGDOG_READER,
                acl.PROJECT_CONFIGS_READER,
                acl.SCHEDULER_READER,
            ],
            groups = ["all"],
        ),
        acl.entry([acl.SCHEDULER_OWNER], groups = ["project-chromium-admins"]),
        acl.entry([acl.LOGDOG_WRITER], groups = ["luci-logdog-chromium-writers"]),
    ],
    bindings = [
        luci.binding(
            roles = "role/configs.validator",
            groups = [
                "project-chromium-try-task-accounts",
                "project-chromium-ci-task-accounts",
            ]
        ),
    ],
)
luci.logdog(
    gs_bucket = "chromium-luci-logdog",
)
luci.milo(
    logo = PROJECT_LOGO,
)
luci.console_view(
    name = PROJECT_NAME,
    title = PROJECT_NAME,
    repo = PROJECT_REPO,
    refs = ["refs/heads/main"],
    favicon = "https://storage.googleapis.com/chrome-infra-public/logo/favicon.ico",
)
luci.gitiles_poller(
    name = "chromium-agents-trigger",
    bucket = "ci",
    repo = PROJECT_REPO,
    refs = ["refs/heads/main"],
)
luci.bucket(name = "ci", acls = [
    acl.entry(
        [acl.BUILDBUCKET_TRIGGERER],
    ),
])
luci.binding(
    realm = "ci",
    roles = "role/swarming.taskTriggerer",
    groups = "flex-ci-led-users",
)
luci.recipe(
    name = RECIPE_NAME,
    cipd_package = RECIPE_CIPD_PACKAGE,
    cipd_version = "refs/heads/main",
    use_bbagent = True,
)
luci.cq(
    submit_max_burst = 4,
    submit_burst_delay = 8 * time.minute,
)
luci.cq_group(
    name = PROJECT_NAME,
    watch = cq.refset(
        repo = PROJECT_REPO,
        refs = ["refs/heads/main"],
    ),
    acls = [
        acl.entry(
            [acl.CQ_COMMITTER],
            groups = ["project-chromium-submit-access"],
        ),
        acl.entry(
            [acl.CQ_DRY_RUNNER, acl.CQ_NEW_PATCHSET_RUN_TRIGGERER],
            groups = ["project-chromium-tryjob-access"],
        ),
    ],
    retry_config = cq.retry_config(
        single_quota = 1,
        global_quota = 2,
        failure_weight = 1,
        transient_failure_weight = 1,
        timeout_weight = 2,
    ),
    verifiers = [
        luci.cq_tryjob_verifier(
            builder = "chromium-agents-presubmit",
            disable_reuse = True,
            mode_allowlist = [
                cq.MODE_NEW_PATCHSET_RUN,
                cq.MODE_DRY_RUN,
                cq.MODE_FULL_RUN,
            ],
        ),
    ],
)
luci.bucket(name = "try", acls = [
    acl.entry(
        [acl.BUILDBUCKET_TRIGGERER],
        groups = ["project-chromium-tryjob-access"],
    ),
])
luci.binding(
    realm = "try",
    roles = "role/swarming.taskTriggerer",
    groups = "flex-try-led-users",
)
luci.builder(
    name = "chromium-agents-presubmit",
    bucket = "try",
    executable = RECIPE_NAME,
    service_account = "chromium-try-builder@chops-service-accounts.iam.gserviceaccount.com",
    dimensions = {
        "cpu": "x86-64",
        "os": os.LINUX_NOBLE.get_dimension("try", "chromium-agents-presubmit"),
        "pool": "luci.flex.try"
    },
    build_numbers = True,
)
