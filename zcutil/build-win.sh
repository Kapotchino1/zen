#!/usr/bin/env bash

set -eu -o pipefail

# run update-alternatives --install /usr/bin/x86_64-w64-mingw32-gcc x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix 100
# and update-alternatives --install /usr/bin/x86_64-w64-mingw32-g++ x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix 100
# first
HOST=x86_64-w64-mingw32
CXX=x86_64-w64-mingw32-g++
CC=x86_64-w64-mingw32-gcc
CXXFLAGS="-DPTW32_STATIC_LIB -DCURVE_ALT_BN128 -fopenmp -pthread"
#CXXFLAGS="-DPTW32_STATIC_LIB -DCURVE_ALT_BN128 -fopenmp -pthread -g" # debugging information, huge binaries
CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site

function cmd_pref() {
    if type -p "$2" > /dev/null; then
        eval "$1=$2"
    else
        eval "$1=$3"
    fi
}

# If a g-prefixed version of the command exists, use it preferentially.
function gprefix() {
    cmd_pref "$1" "g$2" "$2"
}

gprefix READLINK readlink
cd "$(dirname "$("$READLINK" -f "$0")")/.."

# Allow user overrides to $MAKE. Typical usage for users who need it:
#   MAKE=gmake ./zcutil/build.sh -j$(nproc)
if [[ -z "${MAKE-}" ]]; then
    MAKE=make
fi

# Allow overrides to $BUILD and $HOST for porters. Most users will not need it.
#   BUILD=i686-pc-linux-gnu ./zcutil/build.sh
if [[ -z "${BUILD-}" ]]; then
    BUILD="$(./depends/config.guess)"
fi
if [[ -z "${HOST-}" ]]; then
    HOST="$BUILD"
fi

# Allow override to $CC and $CXX for porters. Most users will not need it.
if [[ -z "${CC-}" ]]; then
    CC=gcc
fi
if [[ -z "${CXX-}" ]]; then
    CXX=g++
fi

if [ "x$*" = 'x--help' ]
then
    cat <<EOF
Usage:

$0 --help
  Show this help message and exit.

$0 [ --enable-lcov || --disable-tests ] [ --disable-mining ] [ --disable-rust ] [ --enable-proton ] [ --disable-libs ] [ MAKEARGS... ]
    Build Zen and most of its transitive dependencies from
    source. MAKEARGS are applied to both dependencies and Zen itself.

  If --enable-lcov is passed, Zen is configured to add coverage
  instrumentation, thus enabling "make cov" to work.
  If --disable-tests is passed instead, the Zen tests are not built.

  If --disable-mining is passed, Zen is configured to not build any mining
  code. It must be passed after the test arguments, if present.

  If --disable-rust is passed, Zen is configured to not build any Rust language
  assets. It must be passed after test/mining arguments, if present.

  If --enable-proton is passed, Zen is configured to build the Apache Qpid Proton
  library required for AMQP support. This library is not built by default.
  It must be passed after the test/mining/Rust arguments, if present.

  If --disable-libs is passed, Zen is configured to not build any libraries like
  'libzcashconsensus'.
EOF
    exit 0
fi

set -x

# If --enable-lcov is the first argument, enable lcov coverage support:
LCOV_ARG=''
HARDENING_ARG='--enable-hardening'
TEST_ARG=''
if [ "x${1:-}" = 'x--enable-lcov' ]
then
    LCOV_ARG='--enable-lcov'
    HARDENING_ARG='--disable-hardening'
    shift
elif [ "x${1:-}" = 'x--disable-tests' ]
then
    TEST_ARG='--enable-tests=no'
    shift
fi

# If --disable-mining is the next argument, disable mining code:
MINING_ARG=''
if [ "x${1:-}" = 'x--disable-mining' ]
then
    MINING_ARG='--enable-mining=no'
    shift
fi

# If --disable-rust is the next argument, disable Rust code:
RUST_ARG=''
if [ "x${1:-}" = 'x--disable-rust' ]
then
    RUST_ARG='--enable-rust=no'
    shift
fi

# If --enable-proton is the next argument, enable building Proton code:
PROTON_ARG='--enable-proton=no'
if [ "x${1:-}" = 'x--enable-proton' ]
then
    PROTON_ARG=''
    shift
fi

# If --disable-libs is the next argument, build without libs:
LIBS_ARG=''
if [ "x${1:-}" = 'x--disable-libs' ]
then
    LIBS_ARG='--without-libs'
    shift
fi

PREFIX="$(pwd)/depends/$HOST/"

eval "$MAKE" --version
eval "$CC" --version
eval "$CXX" --version
as --version
ld -v

HOST="$HOST" BUILD="$BUILD" NO_RUST="$RUST_ARG" NO_PROTON="$PROTON_ARG" "$MAKE" "$@" NO_QT=1 -C ./depends/ V=1
./autogen.sh
CXXFLAGS="${CXXFLAGS}" CONFIG_SITE="${CONFIG_SITE}" CC="$CC" CXX="$CXX" ./configure --prefix="${PREFIX}" --host="$HOST" --build="$BUILD" "$RUST_ARG" "$HARDENING_ARG" "$LCOV_ARG" "$TEST_ARG" "$MINING_ARG" "$PROTON_ARG" "$LIBS_ARG" --with-gui=no --enable-static --disable-shared
sed -i 's/-lboost_system-mt /-lboost_system-mt-s /' configure
CC="${CC}" CXX="${CXX}" "$MAKE" "$@" V=1
