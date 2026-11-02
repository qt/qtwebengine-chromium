#! /usr/bin/env python3
# Copyright (C) 2026 fontconfig Authors
# SPDX-License-Identifier: HPND

import argparse
import os
import pathlib
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time
import traceback
from typing import List, Dict, Optional

# PyYAML is loaded after venv setup (installed in venv, not required system-wide)
try:
    import yaml
except ImportError:
    yaml = None


# Global log file descriptor
_global_logfd = None


def log(message, end='\n'):
    """Write to both stdout and log file"""
    print(message, end=end, flush=True)
    if _global_logfd and not _global_logfd.closed:
        try:
            _global_logfd.write(message + end)
            _global_logfd.flush()
        except:
            pass


def get_venv_paths(venv_path: pathlib.Path) -> tuple:
    """Get venv bin directory and executable paths based on platform"""
    # MSYS2 uses bin/ with .exe files, pure Windows uses Scripts/
    scripts_dir = venv_path / "Scripts"
    bin_dir = venv_path / "bin"

    # Determine venv bin directory
    venv_bin = scripts_dir if scripts_dir.exists() else \
               bin_dir if bin_dir.exists() else \
               scripts_dir if sys.platform == "win32" else bin_dir

    # Determine executable suffix
    exe_suffix = ".exe" if (venv_bin / "python.exe").exists() or \
                          (not venv_bin.exists() and sys.platform == "win32") else ""

    return venv_bin, venv_bin / f"python{exe_suffix}", venv_bin / f"pip{exe_suffix}"


class Build:
    def __init__(self, args):
        self.args = args
        self.cidir = pathlib.Path(__file__).parent
        self.srcdir = pathlib.Path.cwd()
        self.task = None
        self.config = {}

        self._setup_environment()
        self._load_config()

    def _log(self, message, end='\n'):
        """Write to both stdout and log file"""
        log(message, end=end)

    def _load_fcenv(self):
        """Load environment variables from fcenv file if it exists"""
        fcenv_path = self.cidir / "fcenv"
        if not fcenv_path.exists():
            return

        log(f">>> Loading environment from {fcenv_path}")

        var_pattern = re.compile(r'^([A-Z_][A-Z0-9_]*)=(.*)$')

        try:
            with open(fcenv_path, 'r') as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith('#'):
                        continue

                    if match := var_pattern.match(line):
                        key, value = match.groups()
                        # Remove surrounding quotes if present
                        if (value.startswith('"') and value.endswith('"')) or \
                           (value.startswith("'") and value.endswith("'")):
                            value = value[1:-1]
                        os.environ[key] = value
                        log(f"  {key}={value}")
        except Exception as e:
            log(f"Warning: Failed to load fcenv: {e}")

    def _setup_environment(self):
        """Setup environment variables and detect OS"""
        self._load_fcenv()

        # Set environment variable defaults
        os.environ.setdefault("MAKE", "make")
        os.environ.setdefault("BUILD_ID", f"fontconfig-{os.getpid()}")
        os.environ.setdefault("PREFIX", str(self.srcdir / "prefix"))
        os.environ.setdefault("BUILDDIR", str(self.srcdir / "build"))
        os.environ.setdefault("CI_MERGE_REQUEST_PROJECT_URL", "https://gitlab.freedesktop.org/fontconfig/fontconfig")
        os.environ.setdefault("CI_COMMIT_REF_NAME", "main")
        os.environ.setdefault("BUILDLOG", str(pathlib.Path(os.environ["BUILDDIR"]) / "fc-build.log"))
        os.environ.setdefault("SOURCE_DATE_EPOCH", str(int(time.time())))

        self.builddir = pathlib.Path(os.environ["BUILDDIR"])
        self.prefix = pathlib.Path(os.environ["PREFIX"])
        self.buildlog = pathlib.Path(os.environ["BUILDLOG"])

        if not os.environ.get("FC_DISTRO_NAME"):
            self._detect_distro()

        self.distro_name = os.environ.get("FC_DISTRO_NAME", "")

    def _detect_distro(self):
        """Detect distribution from platform or /etc/os-release"""
        system = platform.system()

        if system == 'Darwin':
            os.environ["FC_DISTRO_NAME"] = "darwin"
        elif system == 'Windows':
            os.environ["FC_DISTRO_NAME"] = "windows"
        else:
            # Try Linux /etc/os-release
            try:
                with open("/etc/os-release") as f:
                    for line in f:
                        if line.startswith("ID="):
                            os.environ["FC_DISTRO_NAME"] = line.split("=")[1].strip().strip('"')
                            return
            except FileNotFoundError:
                pass

            log("***\n*** Unable to detect OS. cross-compiling may not work. "
                "Please consider setting FC_DISTRO_NAME\n***")
            time.sleep(3)

    def _load_config(self):
        """Load build configuration from YAML"""
        config_file = self.cidir / "build.yaml"
        try:
            with open(config_file, 'r') as f:
                self.config = yaml.safe_load(f) or {}
        except Exception as e:
            log(f"Warning: Failed to load build config: {e}")
            self.config = {}

    def _set_env_vars(self, env_vars: Dict[str, str], context: str):
        """Set environment variables from config"""
        self._log(f"\n>>> Setting {context} environment variables")
        for key, value in env_vars.items():
            expanded = os.path.expandvars(value)
            os.environ[key] = expanded
            self._log(f"  {key}={expanded}")

    def _setup_certificates(self):
        """Setup SSL certificate paths using certifi"""
        try:
            result = subprocess.run(
                [sys.executable, "-m", "certifi"],
                capture_output=True, text=True, check=True
            )
            if cert_path := result.stdout.strip():
                os.environ["SSL_CERT_FILE"] = os.environ["REQUESTS_CA_BUNDLE"] = cert_path
                self._log(f"\n>>> Setting certificate paths:\n  SSL_CERT_FILE={cert_path}\n  REQUESTS_CA_BUNDLE={cert_path}")
        except Exception as e:
            self._log(f"\nWarning: Failed to setup certificates: {e}")

    def _stream_subprocess(self, args: List[str], shell: bool = False,
                          cwd: Optional[pathlib.Path] = None) -> subprocess.CompletedProcess:
        """Run subprocess and stream output to log"""
        proc = subprocess.Popen(
            args,
            stderr=subprocess.STDOUT,
            stdout=subprocess.PIPE,
            text=True,
            cwd=cwd or self.srcdir,
            shell=shell,
            env=os.environ.copy()
        )

        for line in proc.stdout:
            self._log(line, end='')

        proc.wait()
        return subprocess.CompletedProcess(args, proc.returncode)

    def _run_shell_script(self, script: str, shell: str = 'sh',
                         description: str = "", ignore_errors: bool = False) -> bool:
        """Run a shell script and log output"""
        if not script.strip():
            return True

        if description:
            self._log(f"\n>>> {description}")

        result = self._stream_subprocess([shell, '-c', script], shell=False)

        if result.returncode != 0 and not ignore_errors:
            error_msg = f"{description} failed" if description else "Script failed"
            raise subprocess.CalledProcessError(result.returncode, f"{shell} -c <script>", output=error_msg)

        return result.returncode == 0

    def _run_phase_script(self, phase_cfg: Dict, context: str, ignore_errors: bool = False):
        """Run a phase configuration (env + script)"""
        if not phase_cfg:
            return

        # Run script first (may create directories/install dependencies)
        if script := phase_cfg.get('script'):
            self._run_shell_script(script, phase_cfg.get('shell', 'sh'),
                                  f"Running {context} script", ignore_errors)

        # Then set environment variables (may reference things created by script)
        if env_vars := phase_cfg.get('env'):
            self._set_env_vars(env_vars, context)

    def _run_distro_script(self, phase: str):
        """Run distro-specific script for a given phase"""
        if not self.distro_name:
            return

        distro_cfg = self.config.get(self.distro_name, {})
        phase_cfg = distro_cfg.get(phase)

        self._run_phase_script(phase_cfg, f"{self.distro_name} {phase}")

        # Handle certificate setup for macOS and similar platforms
        if phase_cfg and phase_cfg.get('needs_cert_setup'):
            self._setup_certificates()

    def _run_feature_setup(self, feature: str):
        """Run feature-specific setup from config"""
        features = self.config.get('common', {}).get('features', {})
        self._run_phase_script(features.get(feature), f"{feature} feature", ignore_errors=True)

    def _run_command(self, args: List[str], task: str, cwd: Optional[pathlib.Path] = None):
        """Run a command and log output"""
        self.task = task
        self._log(f"\n>>> Running: {task}")
        self._log(f">>> Command: {' '.join(args)}")

        if cwd is None:
            cwd = self.builddir if getattr(self, '_in_builddir', False) else None
        result = self._stream_subprocess(args, cwd=cwd)

        if result.returncode != 0:
            raise subprocess.CalledProcessError(result.returncode, args)

    def _clean_paths(self, *paths: pathlib.Path):
        """Remove paths if they exist"""
        for path in paths:
            if path.exists():
                shutil.rmtree(path)

    def _is_cross(self) -> bool:
        """Check if this is a cross-compile build"""
        return os.environ.get("FC_BUILD_PLATFORM") in {"mingw", "android"}

    def prepare(self):
        raise NotImplementedError

    def compile(self):
        raise NotImplementedError

    def install(self):
        raise NotImplementedError

    def test(self):
        raise NotImplementedError

    def distcheck(self):
        raise NotImplementedError


class BuildMeson(Build):
    def __init__(self, args):
        super().__init__(args)
        self.buildopt = []
        self.subprojectname = ""

    def _meson_cmd(self, *args):
        """Run meson command with standard options"""
        return ["meson", *args, "-C", str(self.builddir)]

    def _handle_features(self):
        """Handle enabled/disabled features"""
        if self.args.enable_feature:
            for feature in self.args.enable_feature:
                self.buildopt.append(f"-D{self.subprojectname}{feature}=enabled")
                self._run_feature_setup(feature)

        if self.args.disable_feature:
            self.buildopt.extend(
                f"-D{self.subprojectname}{f}=disabled"
                for f in self.args.disable_feature
            )

    def _handle_subproject_build(self):
        """Handle subproject build setup"""
        if not self.args.subproject_build:
            return

        self.buildopt.append("--force-fallback-for=fontconfig")

        subproject_dir = self.srcdir / "fc-ci-meson-subproject"
        self._clean_paths(subproject_dir)

        # Clone fc-ci-meson-subproject
        self._run_command([
            "git", "clone",
            "https://gitlab.freedesktop.org/fontconfig/fontconfig-ci/fc-ci-meson-subproject.git"
        ], "git clone", cwd=self.srcdir)

        # Clone fontconfig into subprojects
        subprojects_dir = subproject_dir / "subprojects"
        self._run_command([
            "git", "clone",
            f"{os.environ['CI_MERGE_REQUEST_PROJECT_URL']}.git"
        ], "git clone fontconfig", cwd=subprojects_dir)

        # Handle merge request
        if iid := os.environ.get("CI_MERGE_REQUEST_IID"):
            fontconfig_dir = subprojects_dir / "fontconfig"
            self._run_command([
                "git", "fetch", "origin",
                f"merge-requests/{iid}/head:mr-{iid}"
            ], "git fetch MR", cwd=fontconfig_dir)
            self._run_command([
                "git", "switch", f"mr-{iid}"
            ], "git switch MR", cwd=fontconfig_dir)

        # Update srcdir to point to subproject directory for subsequent meson commands
        self.srcdir = subproject_dir
        self.subprojectname = "fontconfig:"

    def _build_options(self):
        """Build meson options"""
        self._handle_features()
        self._handle_subproject_build()

        if self.args.optimization:
            self.buildopt.append(f"--optimization={self.args.optimization}")

        if self.args.xmlbackend:
            self.buildopt.append(f"-D{self.subprojectname}xml-backend={self.args.xmlbackend}")

        if self._is_cross() and self.args.arch:
            self.buildopt.extend([
                "--cross-file", str(self.cidir / f"{self.args.arch}.txt")
            ])
            self._run_distro_script('cross')

        self.buildopt.append(f"--default-library={self.args.buildtype}")

        if self.args.sanitize:
            self.buildopt.extend([
                f"-Db_sanitize={self.args.sanitize}",
                "-Db_lundef=false"
            ])

    def prepare(self):
        """Prepare meson build"""
        self._build_options()

        if not self.args.no_clean:
            self._clean_paths(self.builddir, self.prefix)

        self._run_command([
            "meson", "setup",
            f"--prefix={self.prefix}",
            f"-D{self.subprojectname}nls=enabled",
            f"-D{self.subprojectname}cache-build=disabled",
            f"-D{self.subprojectname}iconv=enabled",
            *self.buildopt,
            str(self.builddir)
        ], "meson setup")

    def compile(self):
        self._run_command(self._meson_cmd("compile", "-v"), "meson compile")

    def install(self):
        self._run_command(self._meson_cmd("install"), "meson install")

    def test(self):
        self._run_command(self._meson_cmd("test", "-v"), "meson test")

    def distcheck(self):
        self._run_command(self._meson_cmd("dist"), "meson dist")


class BuildAutotools(Build):
    def __init__(self, args):
        super().__init__(args)
        self.buildopt = []
        self.make = os.environ.get("MAKE", "make")

        if args.subproject_build:
            raise RuntimeError("Subproject build not supported in autotools")

    def _make_cmd(self, *targets):
        """Build make command with targets"""
        return [self.make, *targets, "V=1"]

    def _build_options(self):
        """Build autotools configure options"""
        if self.args.enable_feature:
            self.buildopt.extend(f"--enable-{f}" for f in self.args.enable_feature)

        if self.args.disable_feature:
            self.buildopt.extend(f"--disable-{f}" for f in self.args.disable_feature)

        # XML backend options
        if self.args.xmlbackend == "expat":
            self.buildopt.append("--disable-libxml2")
        elif self.args.xmlbackend == "libxml2":
            self.buildopt.append("--enable-libxml2")

        # Build type options
        self.buildopt.extend({
            "shared": ["--enable-shared", "--disable-static"],
            "static": ["--disable-shared", "--enable-static"],
            "both": ["--enable-shared", "--enable-static"]
        }.get(self.args.buildtype, []))

        if self._is_cross() and self.args.arch:
            self.buildopt.append(f"--host={self.args.arch}")
            self._run_distro_script('cross')

    def prepare(self):
        """Prepare autotools build"""
        self._build_options()

        if not self.args.no_clean:
            self._clean_paths(self.builddir, self.prefix)

        self.builddir.mkdir(parents=True, exist_ok=True)
        self.prefix.mkdir(parents=True, exist_ok=True)

        os.chdir(self.builddir)
        self._in_builddir = True

        self._run_command([
            str(self.srcdir / "autogen.sh"),
            f"--prefix={self.prefix}",
            "--disable-cache-build",
            *self.buildopt
        ], "autogen.sh")

    def compile(self):
        self._run_command(self._make_cmd(), "make")

    def install(self):
        self._run_command(self._make_cmd("install"), "make install")

    def test(self):
        self._run_command(self._make_cmd("check"), "make check")

    def distcheck(self):
        self._run_command(self._make_cmd("distcheck"), "make distcheck")


def ensure_venv(skip_venv=False):
    """Ensure build.py is running in a virtual environment"""
    if skip_venv or hasattr(sys, 'real_prefix') or sys.base_prefix != sys.prefix:
        return

    venv_path = pathlib.Path.cwd() / ".venv"
    needs_setup = not venv_path.exists()

    # Create venv if needed
    if needs_setup:
        log(">>> Creating Python virtual environment")
        if subprocess.run([sys.executable, "-m", "venv", str(venv_path)]).returncode != 0:
            sys.exit("Error: venv creation failed")

    venv_bin, venv_python, venv_pip = get_venv_paths(venv_path)

    # Install PyYAML if venv was just created
    if needs_setup:
        log(">>> Installing PyYAML in virtual environment")
        if subprocess.run([str(venv_pip), "install", "PyYAML"]).returncode != 0:
            sys.exit("Error: PyYAML installation failed")

    # Activate venv and re-execute
    os.environ.update({"PATH": f"{venv_bin}:{os.environ['PATH']}", "VIRTUAL_ENV": str(venv_path)})
    log(">>> Re-executing build.py in virtual environment")
    sys.exit(subprocess.run([str(venv_python)] + sys.argv).returncode)


def main():
    global _global_logfd

    # Create log file early so all output is captured
    temp_logfile = tempfile.NamedTemporaryFile(
        mode='w', prefix='fc-build-', suffix='.log',
        delete=False
    )
    _global_logfd = temp_logfile

    parser = argparse.ArgumentParser(
        description="Build fontconfig using meson or autotools",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Environment variables:
  FC_BUILD_PLATFORM  Set a platform for cross-compiling (mingw or android)
  FC_DISTRO_NAME     Distribution name for distro-specific setup
"""
    )
    parser.add_argument("-a", "--arch", help="Target architecture to cross-compile")
    parser.add_argument("-A", "--sanitize", help="Enable the address sanitizer (meson only)")
    parser.add_argument("-c", "--distcheck", action="store_true", help="Run distcheck")
    parser.add_argument("-C", "--no-test", action="store_true", help="Do not run unit tests")
    parser.add_argument("-e", "--enable-feature", action="append",
                       help="Enable specific features (can be used multiple times)")
    parser.add_argument("-d", "--disable-feature", action="append",
                       help="Disable specific features (can be used multiple times)")
    parser.add_argument("-I", "--no-install", action="store_true", help="Do not run installation")
    parser.add_argument("-N", "--no-clean", action="store_true", help="Do not clean build directory")
    parser.add_argument("-O", "--optimization", help="Optimization level to build")
    parser.add_argument("-V", "--no-venv", action="store_true", help="Do not use virtual environment")
    parser.add_argument("-s", "--buildsys", choices=["meson", "autotools"], default="meson",
                       help="Use the buildsys to build")
    parser.add_argument("-S", "--subproject-build", action="store_true",
                       help="Run the sub-project build (meson only)")
    parser.add_argument("-t", "--buildtype", choices=["shared", "static", "both"], default="shared",
                       help="Build type: shared, static, or both")
    parser.add_argument("-X", "--xmlbackend", choices=["expat", "libxml2"], default="expat",
                       help="XML parser library to use")

    args = parser.parse_args()

    if args.sanitize and not args.optimization:
        args.optimization = "g"

    # Ensure venv after parsing args (allows --help without venv setup)
    ensure_venv(skip_venv=args.no_venv)

    # Install packages from build.yaml (only when using venv)
    if not args.no_venv:
        try:
            with open(pathlib.Path(__file__).parent / "build.yaml", 'r') as f:
                config = yaml.safe_load(f)

            if packages := config.get('common', {}).get('packages', []):
                venv_pip = str(get_venv_paths(pathlib.Path.cwd() / ".venv")[2])
                log(">>> Installing packages from build.yaml")
                if subprocess.run([venv_pip, "install"] + packages).returncode != 0:
                    sys.exit("Error: Package installation failed")
        except FileNotFoundError:
            pass
        except Exception as e:
            log(f"Warning: Failed to install packages from build.yaml: {e}")

    try:
        build = BuildMeson(args) if args.buildsys == "meson" else BuildAutotools(args)

        build._log(f"Build System: {args.buildsys}\n")

        build._log("Environment variables:")
        for key, value in sorted(os.environ.items()):
            build._log(f"{key}={value}")
        build._log("")

        build._run_distro_script('setup')
        build._run_distro_script('install')

        build.prepare()
        build.compile()

        if not args.no_install:
            build.install()

        if not args.no_test:
            build.test()

        if args.distcheck:
            build.distcheck()

        return 0

    except Exception as e:
        # Get task name if build object exists
        task = getattr(locals().get('build'), 'task', None)
        error_prefix = f"\nAborting from \"{task}\" with error: " if task else "\nBuild failed: "
        error_msg = f"{error_prefix}{e}\n{traceback.format_exc()}"

        print(error_msg, file=sys.stderr, end='')
        log(error_msg)
        return 1
    finally:
        # Close log file and move to final location
        if _global_logfd and not _global_logfd.closed:
            temp_log_path = _global_logfd.name
            _global_logfd.close()

            try:
                buildlog = pathlib.Path(os.environ.get("BUILDLOG", "build/fc-build.log"))
                buildlog.parent.mkdir(parents=True, exist_ok=True)
                shutil.move(temp_log_path, buildlog)
            except Exception:
                pass


if __name__ == "__main__":
    sys.exit(main())
