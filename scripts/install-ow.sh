#!/usr/bin/env bash
#############################################################################
##
## Copyright (C) 2022 Denis Shienkov <denis.shienkov@gmail.com>
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

function show_help() {
    cat <<EOF
usage: install-ow [options] [components]

Examples
  ./install-ow.sh --platform win --architecture x64

Options
  -d, --directory <directory>
        Root directory where to install the components.
        Maps to c:/watcom on Windows, /opt/watcom on Linux
        by default.

  --platform <platform-os>
        The host platform. Can be one of linux, win. Auto-detected by default.

  --architecture <architecture-os>
        The host architecture. Can be one of x86, x64. Auto-detected by default.

  --version <version>
        The desired toolchain version. Currently supported only
        1.9 and 2.0 versions.

EOF
}

VERSION=2.0 # Default latest version (a fork of original toolchain)..

case "$OSTYPE" in
    *linux*)
        PLATFORM="linux"
        INSTALL_DIR="/opt/watcom"
        EXE_SUFFIX=
        ;;
    msys)
        PLATFORM="win"
        INSTALL_DIR="/c/watcom"
        EXE_SUFFIX=".exe"
        ;;
    *)
        PLATFORM=
        INSTALL_DIR=
        EXE_SUFFIX=
        ;;
esac

case "$HOSTTYPE" in
    x86_64)
        ARCHITECTURE="x64"
        ;;
    x86)
        ARCHITECTURE="x86"
        ;;
    *)
        ARCHITECTURE=
        ;;
esac

while [ $# -gt 0 ]; do
    case "$1" in
        --directory|-d)
            INSTALL_DIR="$2"
            shift
            ;;
        --platform)
            PLATFORM="$2"
            shift
            ;;
        --architecture)
            ARCHITECTURE="$2"
            shift
            ;;
        --version)
            VERSION="$2"
            shift
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            ;;
    esac
    shift
done

if [ -z "${INSTALL_DIR}" ]; then
    echo "No --directory specified or auto-detection failed." >&2
    exit 1
fi

if [ -z "${PLATFORM}" ]; then
    echo "No --platform specified or auto-detection failed." >&2
    exit 1
fi

if [ -z "${ARCHITECTURE}" ]; then
    echo "No --architecture specified or auto-detection failed." >&2
    exit 1
fi

if [ -z "${VERSION}" ]; then
    echo "No --version specified." >&2
    exit 1
fi

DOWNLOAD_DIR=`mktemp -d 2>/dev/null || mktemp -d -t 'ow-installer'`

BASE_URL_PREFIX="https://github.com/open-watcom/open-watcom-"

if [[ "${VERSION}" == "1.9" ]]; then
    # Original old OW v1.9 release supports only the 32-bit packages!
    if [[ "${PLATFORM}" =~ "linux" ]]; then
        BIN_DIR="binl"
        HOST="linux"
    elif [[ "${PLATFORM}" =~ "win" ]]; then
        HOST="win32"
        BIN_DIR="binnt"
    fi
    URL="${BASE_URL_PREFIX}${VERSION}/releases/download/ow1.9/open-watcom-c-${HOST}-1.9${EXE_SUFFIX}"
else
    if [[ "${PLATFORM}" =~ "linux" ]]; then
        if [[ "${ARCHITECTURE}" =~ "x86" ]]; then
            BIN_DIR="binl"
        elif [[ "${ARCHITECTURE}" =~ "x64" ]]; then
            BIN_DIR="binl"
        fi
    elif [[ "${PLATFORM}" =~ "win" ]]; then
        if [[ "${ARCHITECTURE}" =~ "x86" ]]; then
            BIN_DIR="binnt"
        elif [[ "${ARCHITECTURE}" =~ "x64" ]]; then
            BIN_DIR="binnt64"
        fi
    fi
    # Default URL for the latest OW v2.0 fork.
    URL="${BASE_URL_PREFIX}v2/releases/download/Current-build/open-watcom-2_0-c-${PLATFORM}-${ARCHITECTURE}${EXE_SUFFIX}"
fi

INSTALLER="${DOWNLOAD_DIR}/setup${EXE_SUFFIX}"

echo "Downloading from ${URL}..." >&2
curl --progress-bar -L -o ${INSTALLER} ${URL} >&2

echo "Installing to ${INSTALL_DIR}..." >&2
if [[ "${PLATFORM}" =~ "linux" ]]; then
    chmod +777 ${INSTALLER}
fi

${INSTALLER} -dDstDir=${INSTALL_DIR} -i -np -ns
echo "${INSTALL_DIR}/${BIN_DIR}"

rm -f ${INSTALLER}
