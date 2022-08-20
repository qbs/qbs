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
set -o pipefail

function show_help() {
    cat <<EOF
usage: install-ow [options] [components]

Examples
  ./install-ow.sh --version 2.0

Options
  -d, --directory <directory>
        Root directory where to install the components.
        Maps to c:/watcom on Windows, /opt/watcom on Linux
        by default.

  --version <version>
        The desired toolchain version.
        Currently supported only 2.0 version.

EOF
}

VERSION=2.0
INSTALL_DIR=
BIN_DIR=

if [[ "$OSTYPE" =~ "linux" ]]; then
    INSTALL_DIR="/opt/watcom"
    if [[ "$HOSTTYPE" == "x86_64" ]]; then
        BIN_DIR="binl64"
    elif [[ "$HOSTTYPE" == "x86" ]]; then
        BIN_DIR="binl"
    fi
elif [[ "$OSTYPE" == "msys" ]]; then
    INSTALL_DIR="/c/watcom"
    if [[ "$HOSTTYPE" == "x86_64" ]]; then
        BIN_DIR="binnt64"
    elif [[ "$HOSTTYPE" == "x86" ]]; then
        BIN_DIR="binnt"
    fi
fi

while [ $# -gt 0 ]; do
    case "$1" in
        --directory|-d)
            INSTALL_DIR="$2"
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

if [ -z "${VERSION}" ]; then
    echo "No --version specified." >&2
    exit 1
fi

DOWNLOAD_DIR=`mktemp -d 2>/dev/null || mktemp -d -t 'ow-tmp'`

VERSION_MAJOR=`echo $VERSION | cut -d. -f1`
VERSION_MINOR=`echo $VERSION | cut -d. -f2`

OW_URL="https://github.com/open-watcom/open-watcom-v${VERSION_MAJOR}/releases/download/Current-build/ow-snapshot.tar.xz"
OW_TAR="${DOWNLOAD_DIR}/ow.tar.xz"

echo "Downloading compiler from ${OW_URL}..." >&2
curl --progress-bar -L -o ${OW_TAR} ${OW_URL} >&2

echo "Unpacking compiler to ${INSTALL_DIR}..." >&2
7z x "${OW_TAR}" -so | 7z x -aoa -si -ttar -o"${INSTALL_DIR}" >/dev/null 2>&1

echo "${INSTALL_DIR}/${BIN_DIR}"

rm -f ${OW_TAR}
