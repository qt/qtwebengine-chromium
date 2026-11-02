#! /bin/bash

set -ex
set -o pipefail

cidir=$(dirname "$0")
# shellcheck source=/dev/null
[ -f "${cidir}/fcenv" ] && . "${cidir}/fcenv"

case "$OSTYPE" in
    msys) MyPWD=$(pwd -W) ;;
    *BSD) PATH=$PATH:/usr/local/bin ;;
    *) MyPWD=$(pwd) ;;
esac
enable=()
disable=()
distcheck=0
enable_install=1
disable_check=0
clean_build=1
cross=0
subproject=0
subprojectname=""
buildsys="meson"
type="shared"
arch=""
buildopt=()
optimization=""
sanitize=""
SRCDIR="$MyPWD"
export MAKE=${MAKE:-make}
export BUILD_ID=${BUILD_ID:-fontconfig-$$}
export PREFIX=${PREFIX:-$MyPWD/prefix}
export BUILDDIR=${BUILDDIR:-$MyPWD/build}
export BUILDLOG=${BUILDLOG:-${BUILDDIR}/fc-build.log}
export CI_MERGE_REQUEST_PROJECT_URL=${CI_MERGE_REQUEST_PROJECT_URL:-https://gitlab.freedesktop.org/fontconfig/fontconfig}
export CI_COMMIT_REF_NAME=${CI_COMMIT_REF_NAME:-main}

if [ -z "$FC_DISTRO_NAME" ]; then
    . /etc/os-release || :
    FC_DISTRO_NAME=$ID
fi
if [ -z "$FC_DISTRO_NAME" ]; then
    echo "***"
    echo "*** Unable to detect OS. cross-compiling may not work. Please consider setting FC_DISTRO_NAME"
    echo "***"
    sleep 3
fi

while getopts a:A:cCe:d:hINO:s:St:X: OPT
do
    case $OPT in
        'a') arch=$OPTARG ;;
        'A') sanitize=$OPTARG ; optimization="g" ;;
        'c') distcheck=1 ;;
        'C') disable_check=1 ;;
        'e') enable+=("$OPTARG") ;;
        'd') disable+=("$OPTARG") ;;
        'I') enable_install=0 ;;
        'N') clean_build=0 ;;
        'O') optimization=$OPTARG ;;
        's') buildsys=$OPTARG ;;
        'S') subproject=1 ;;
        't') type=$OPTARG ;;
        'X') backend=$OPTARG ;;
        'h'|'*')
            set +x
            echo "Usage: $0 [-a ARCH] [-A SANITIZER] [-c] [-C] [-e OPT] [-d OPT] [-h] [-I] [-N] [-O N] [-s BUILDSYS] [-S] [-t BUILDTYPE] [-X XMLBACKEND]"
            echo "Options:"
            echo "  -a ARCH        Use ARCH for cross-compile. Depends on BUILDSYS"
            echo "  -A SANITIZER   Use the address sanitizer. Take effect on meson only"
            echo "  -c             Run distcheck"
            echo "  -C             Do not run unit tests"
            echo "  -e OPT         Enable OPT feature to build"
            echo "  -d OPT         Disable OPT feature to build"
            echo "  -I             Do not run install process"
            echo "  -N             Do not clean build directory"
            echo "  -O N           Optimization level to build"
            echo "  -s BUILDSYS    Use BUILDSYS to build (default: $buildsys)"
            echo "  -S             Run sub-project build. Take effect on meson only"
            echo "  -t BUILDTYPE   shared build or static build (default: $type)"
            echo "  -X XMLBACKEND  Use XMLBACKEND for xml parser library"
            echo ""
            echo "FC_BUILD_PLATFORM: Set a platform for cross-compiling, mingw or android"
            exit 1
            ;;
    esac
done
case x"$FC_BUILD_PLATFORM" in
    'xmingw') cross=1 ;;
    'xandroid') cross=1 ;;
    *) cross=0 ;;
esac

env

# shellcheck disable=SC2329
clean_exit() {
    rc=$?
    trap - INT TERM ABRT EXIT
    if [ -n "$TASK" ]; then
        echo "Aborting from \"$TASK\" with the exit code $rc"
    fi
    mv /tmp/fc-build.log "${BUILDLOG}" || :
    exit "$rc"
}

trap clean_exit INT TERM ABRT EXIT

if [ -f "${SRCDIR}/.gitlab-ci/${FC_DISTRO_NAME}-setup.sh" ]; then
    # shellcheck source=/dev/null
    . "${SRCDIR}/.gitlab-ci/${FC_DISTRO_NAME}-setup.sh"
fi

if [ "$buildsys" == "autotools" ]; then
    if [ $subproject -eq 1 ]; then
        echo "Subproject build not supported in autotools"
        exit 1
    fi
    for i in "${enable[@]}"; do
        buildopt+=("--enable-$i")
    done
    for i in "${disable[@]}"; do
        buildopt+=("--disable-$i")
    done
    case x"$backend" in
        'xexpat')
            buildopt+=(--disable-libxml2)
            ;;
        'xlibxml2')
            buildopt+=(--enable-libxml2)
            ;;
    esac
    case x"$type" in
        'xshared')
            buildopt+=(--enable-shared)
            buildopt+=(--disable-static)
            ;;
        'xstatic')
            buildopt+=(--disable-shared)
            buildopt+=(--enable-static)
            ;;
        'xboth')
            buildopt+=(--enable-shared)
            buildopt+=(--enable-static)
            ;;
    esac
    if [ $cross -eq 1 ] && [ -n "$arch" ]; then
        buildopt+=("--host=$arch")
        if [ ! -f ".gitlab-ci/${FC_DISTRO_NAME}-cross.sh" ]; then
            echo "No ${FC_DISTRO_NAME}-cross.sh available"
            exit 1
        fi
        # shellcheck source=/dev/null
        . ".gitlab-ci/${FC_DISTRO_NAME}-cross.sh"
    fi
    if [ $clean_build -eq 1 ]; then
        rm -rf "$BUILDDIR" "$PREFIX" || :
        mkdir "$BUILDDIR" "$PREFIX"
    fi
    cd "$BUILDDIR"
    TASK="autogen.sh"
    ../autogen.sh --prefix="$PREFIX" --disable-cache-build "${buildopt[@]}" 2>&1 | tee /tmp/fc-build.log
    TASK="make"
    $MAKE V=1 2>&1 | tee -a /tmp/fc-build.log
    if [ $enable_install -eq 1 ]; then
        TASK="make install"
        $MAKE install V=1 2>&1 | tee -a /tmp/fc-build.log
    fi
    if [ $disable_check -eq 0 ]; then
        TASK="make check"
        $MAKE check V=1 2>&1 | tee -a /tmp/fc-build.log
    fi
    if [ $distcheck -eq 1 ]; then
        TASK="make distcheck"
        $MAKE distcheck V=1 2>&1 | tee -a /tmp/fc-build.log
    fi
elif [ "$buildsys" == "meson" ]; then
    TASK="pip install"
    python3 -m venv .venv
    # shellcheck source=/dev/null
    . .venv/bin/activate
    pip install "meson>=1.11.0"
#   tomli not required for Python >= 3.11
    pip install tomli
    pip install pytest pytest-tap requests
    for i in "${enable[@]}"; do
        buildopt+=("-D$i=enabled")

        # Update bindgen on Fontations builds to improve support for constants in fcint.h
        if [[ "$i" == "fontations" ]]; then
            TASK="cargo install"
            cargo install bindgen-cli
            # Prepend the cargo bin directory to PATH
            if [[ -d "$HOME/.cargo/bin" ]]; then
                export PATH="$HOME/.cargo/bin:$PATH"
                echo "Cargo bin directory added to PATH."
            else
                echo "Cargo bin directory not found."
            fi
        fi
    done
    if [ $subproject -eq 1 ]; then
        buildopt+=(--force-fallback-for=fontconfig)
        if [ -d fc-ci-meson-subproject ]; then
            rm -rf fc-ci-meson-subproject
        fi
        TASK="git clone"
        git clone https://gitlab.freedesktop.org/fontconfig/fontconfig-ci/fc-ci-meson-subproject.git
        cd fc-ci-meson-subproject
        pushd subprojects
        git clone "${CI_MERGE_REQUEST_PROJECT_URL}.git"
        if [ -n "$CI_MERGE_REQUEST_IID" ]; then
            pushd fontconfig
            git fetch origin "merge-requests/$CI_MERGE_REQUEST_IID/head:mr-$CI_MERGE_REQUEST_IID"
            git switch "mr-$CI_MERGE_REQUEST_IID"
            popd
        else
            # use main branch instead
            :
        fi
        popd
        subprojectname="fontconfig:"
    fi
    TASK=
    if [ -n "$optimization" ]; then
        buildopt+=("--optimization=$optimization")
    fi
    for i in "${disable[@]}"; do
        buildopt+=("-D${subprojectname}$i=disabled")
    done
    case x"$backend" in
        'xexpat')
            buildopt+=("-D${subprojectname}xml-backend=expat")
            ;;
        'xlibxml2')
            buildopt+=("-D${subprojectname}xml-backend=libxml2")
            ;;
    esac
    if [ $cross -eq 1 ] && [ -n "$arch" ]; then
        buildopt+=(--cross-file)
        buildopt+=(".gitlab-ci/$arch.txt")
        if [ ! -f ".gitlab-ci/$FC_DISTRO_NAME-cross.sh" ]; then
            echo "No $FC_DISTRO_NAME-cross.sh available"
            exit 1
        fi
        if [ $subproject -eq 1 ]; then
            # shellcheck source=/dev/null
            . "subprojects/fontconfig/.gitlab-ci/$FC_DISTRO_NAME-cross.sh"
        else
            # shellcheck source=/dev/null
            . ".gitlab-ci/$FC_DISTRO_NAME-cross.sh"
        fi
    fi
    buildopt+=("--default-library=$type")
    if [ -n "$sanitize" ]; then
        buildopt+=("-Db_sanitize=$sanitize")
        # for memory sanitizer
        buildopt+=(-Db_lundef=false)
    fi
    if [ $clean_build -eq 1 ]; then
        rm -rf "$BUILDDIR" "$PREFIX" || :
    fi
    TASK="meson setup"
    meson setup --prefix="$PREFIX" -D${subprojectname}nls=enabled -D${subprojectname}cache-build=disabled -D${subprojectname}iconv=enabled "${buildopt[@]}" "$BUILDDIR" 2>&1 | tee /tmp/fc-build.log
    TASK="meson compile"
    meson compile -v -C "$BUILDDIR" 2>&1 | tee -a /tmp/fc-build.log
    if [ $enable_install -eq 1 ]; then
        TASK="meson install"
        meson install -C "$BUILDDIR" 2>&1 | tee -a /tmp/fc-build.log
    fi
    if [ $disable_check -eq 0 ]; then
        TASK="meson test"
        meson test -v -C "$BUILDDIR" 2>&1 | tee -a /tmp/fc-build.log
    fi
    if [ $distcheck -eq 1 ]; then
        TASK="meson dist"
        meson dist -C "$BUILDDIR" 2>&1 | tee -a /tmp/fc-build.log
    fi
    deactivate
fi
TASK=
exit 0
