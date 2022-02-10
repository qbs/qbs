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
usage: install-dm [options] [components]

Examples
  ./install-dm.sh --version 8.57

Options
  -d, --directory <directory>
        Root directory where to install the components.
        Maps to c:/dm on Windows by default.

  --version <version>
        The desired toolchain version.
        Currently supported only 8.57 version.

EOF
}

VERSION="8.57"
INSTALL_DIR="/c/dm"

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

DOWNLOAD_DIR=`mktemp -d 2>/dev/null || mktemp -d -t 'dm-tmp'`

DM_URL="http://ftp.digitalmars.com/Digital_Mars_C++/Patch/dm${VERSION//./}c.zip"
UTILS_URL="http://ftp.digitalmars.com/bup.zip"
DOS_LIBS_URL="http://ftp.digitalmars.com/Digital_Mars_C++/Patch/dm850dos.zip"
DOSX_LIBS_URL="http://ftp.digitalmars.com/Digital_Mars_C++/Patch/dm831x.zip"

DM_ZIP="${DOWNLOAD_DIR}/dm.zip"
UTILS_ZIP="${DOWNLOAD_DIR}/utils.zip"
DOS_LIBS_ZIP="${DOWNLOAD_DIR}/doslibs.zip"
DOSX_LIBS_ZIP="${DOWNLOAD_DIR}/dosxlibs.zip"

echo "Downloading compiler from ${DM_URL}..." >&2
curl --progress-bar -L -o ${DM_ZIP} ${DM_URL} >&2

echo "Downloading utils from ${UTILS_URL}..." >&2
curl --progress-bar -L -o ${UTILS_ZIP} ${UTILS_URL} >&2

echo "Downloading DOS libs from ${DOS_LIBS_URL}..." >&2
curl --progress-bar -L -o ${DOS_LIBS_ZIP} ${DOS_LIBS_URL} >&2

echo "Downloading DOSX libs from ${DOSX_LIBS_URL}..." >&2
curl --progress-bar -L -o ${DOSX_LIBS_ZIP} ${DOSX_LIBS_URL} >&2

echo "Unpacking compiler to ${INSTALL_DIR}..." >&2
7z x -y -o${INSTALL_DIR} ${DM_ZIP} >/dev/null 2>&1

echo "Unpacking utils to ${INSTALL_DIR}..." >&2
7z x -y -o${INSTALL_DIR} ${UTILS_ZIP} >/dev/null 2>&1

echo "Unpacking DOS libs to ${INSTALL_DIR}..." >&2
7z x -y -o${INSTALL_DIR} ${DOS_LIBS_ZIP} >/dev/null 2>&1

echo "Unpacking DOSX libs to ${INSTALL_DIR}..." >&2
7z x -y -o${INSTALL_DIR} ${DOSX_LIBS_ZIP} >/dev/null 2>&1

echo "${INSTALL_DIR}/dm/bin"

rm -f ${DM_ZIP}
rm -f ${UTILS_ZIP}
rm -f ${DOS_LIBS_ZIP}
rm -f ${DOSX_LIBS_ZIP}
