#!/usr/bin/env bash
#############################################################################
##
## Copyright (C) 2025 Ivan Komissarov (abbapoh@gmail.com).
## Contact: https://www.qt.io/licensing/
##
## This file is part of Qbs.
##
## $QT_BEGIN_LICENSE:LGPL$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 3 as published by the Free Software
## Foundation and appearing in the file LICENSE.LGPL3 included in the
## packaging of this file. Please review the following information to
## ensure the GNU Lesser General Public License version 3 requirements
## will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 2.0 or (at your option) the GNU General
## Public license version 3 or any later version approved by the KDE Free
## Qt Foundation. The licenses are as published by the Free Software
## Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-2.0.html and
## https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################
set -eu
set -o pipefail

function help() {
    cat <<EOF
usage: install-qbs [options]

Examples
  ./install-qbs.sh --version 2.6.0

Options
  -d, --directory <directory>
        Root directory where to install the components.
        Maps to C:/qbs on Windows, /opt/qbs on Linux and macOS by default.
  --host <host-os>
        The host operating system. Can be one of linux-x86_64,
        macos-x86_64, macos-arm64, windows-x86_64. Auto-detected by default.
  --version <version>
        The desired Qbs version.
  --no-default-mirror
        Skip https://download.qt.io (its MirrorBrain redirector) and use the
        explicit fallback mirrors directly. Useful where download.qt.io is
        blocked or unreachable.
EOF
}

case "$OSTYPE" in
    *linux*)
        HOST_OS=linux-x86_64
        INSTALL_DIR=/opt/qbs
        ;;
    darwin*)
        case "$(uname -m)" in
            arm64)
                HOST_OS=macos-arm64
                ;;
            x86_64)
                HOST_OS=macos-x86_64
                ;;
            *)
                HOST_OS=
                ;;
        esac
        INSTALL_DIR=/opt/qbs
        ;;
    msys|cygwin)
        HOST_OS=windows-x86_64
        INSTALL_DIR=/c/qbs
        ;;
    *)
        HOST_OS=
        INSTALL_DIR=
        ;;
esac

USE_DEFAULT_MIRROR=1

while [ $# -gt 0 ]; do
    case "$1" in
        --directory|-d)
            INSTALL_DIR="$2"
            shift
            ;;
        --host)
            HOST_OS="$2"
            shift
            ;;
        --version)
            VERSION="$2"
            shift
            ;;
        --no-default-mirror)
            USE_DEFAULT_MIRROR=
            ;;
        --help|-h)
            help
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            help
            exit 1
            ;;
    esac
    shift
done

if [ -z "${HOST_OS}" ]; then
    echo "No --host specified and auto-detection failed." >&2
    exit 1
fi

if [ -z "${INSTALL_DIR}" ]; then
    echo "No --directory specified and auto-detection failed." >&2
    exit 1
fi

if [ -z "${VERSION}" ]; then
    echo "No --version specified." >&2
    exit 1
fi

# Lead with download.qt.io: its MirrorBrain frontend redirects (302) to a healthy,
# geographically-close mirror automatically. The rest are explicit fallbacks taken from
# https://download.qt.io/static/mirrorlist/ in case the redirect target is itself unhealthy.
# A few geographically-spread fallbacks (EU, US, Asia, Oceania) are enough: they only fire if
# download.qt.io itself is unreachable. Keep the list short — every dead/stale entry costs retry
# time before failover, and hardcoded mirrors rot. Verify with:
#   curl -sIL "$M/official_releases/qbs/<ver>/qbs-linux-x86_64-<ver>.tar.gz" -o /dev/null -w '%{http_code}\n'
# Where download.qt.io is blocked/unreachable, --no-default-mirror
# drops it and goes straight to the explicit fallbacks.
FALLBACK_MIRRORS="\
    https://ftp.fau.de/qtproject \
    https://www.mirrorservice.org/sites/download.qt-project.org \
    https://mirrors.ocf.berkeley.edu/qt \
    https://mirrors.ustc.edu.cn/qtproject \
    https://mirror.aarnet.edu.au/pub/qtproject \
"
if [ -n "${USE_DEFAULT_MIRROR}" ]; then
    MIRRORS="https://download.qt.io ${FALLBACK_MIRRORS}"
else
    MIRRORS="${FALLBACK_MIRRORS}"
fi

# Hardened curl invocation reused for every download attempt:
#   --connect-timeout 5        : connections establish fast; keep it short so a blocked/unreachable
#                                mirror fails over quickly instead of hanging
#   --speed-limit/--speed-time : abort a stalled transfer (<1 KB/s for 30s) instead of hanging
#   --retry/--retry-all-errors : auto-retry transient failures (timeouts, connection resets);
#                                with no --retry-delay, curl uses exponential backoff (1s,2s,4s,…)
#   --fail                     : treat HTTP errors as failures so we fall through to next mirror
#   --location                 : follow MirrorBrain's 302 redirect from download.qt.io
CURL=(curl --fail --location --connect-timeout 5 \
      --retry 2 --retry-all-errors \
      --speed-limit 1024 --speed-time 30 --show-error --silent)

BASE_FILENAME="qbs-${HOST_OS}-${VERSION}"

case "${HOST_OS}" in
    linux-x86_64|macos-x86_64|macos-arm64)
        FILENAME="${BASE_FILENAME}.tar.gz"
        ;;
    windows-x86_64)
        FILENAME="${BASE_FILENAME}.zip"
        ;;
    *)
        echo "Unsupported host OS: ${HOST_OS}" >&2
        exit 1
        ;;
esac

DOWNLOAD_DIR=`mktemp -d 2>/dev/null || mktemp -d -t 'install-qbs'`

rm -rf "${INSTALL_DIR}/${BASE_FILENAME}"
mkdir -p "${INSTALL_DIR}"

# Download FILENAME from $1 (full URL), verify the archive is intact, then extract it.
# Returns non-zero on any failure so the caller can try the next mirror.
download_one() {
    echo "Trying ${1} ..." >&2
    "${CURL[@]}" "${1}" -o "${FILENAME}" || return 1
    case "${FILENAME}" in
        *.tar.gz)
            tar -tzf "${FILENAME}" >/dev/null 2>&1 || return 1
            tar -xzf "${FILENAME}"
            ;;
        *.zip)
            7z t "${FILENAME}" >/dev/null 2>&1 || return 1
            7z x -y -o"${BASE_FILENAME}" "${FILENAME}" >/dev/null 2>&1
            ;;
    esac
}

echo "Downloading Qbs ${VERSION}..." >&2
cd "${DOWNLOAD_DIR}"

DOWNLOADED=
for MIRROR in ${MIRRORS}; do
    if download_one "${MIRROR}/official_releases/qbs/${VERSION}/${FILENAME}"; then
        DOWNLOADED=1
        break
    fi
    echo "  failed, trying next mirror..." >&2
    rm -f "${FILENAME}"
done

if [ -z "${DOWNLOADED}" ]; then
    echo "All mirrors failed for Qbs ${VERSION}." >&2
    exit 1
fi

mv "${BASE_FILENAME}" "${INSTALL_DIR}"
rm "${FILENAME}"
echo "${INSTALL_DIR}/${BASE_FILENAME}/bin"

