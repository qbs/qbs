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
        Maps to C:/qbs on Windows, /opt/qbs on Linux by default.
  --host <host-os>
        The host operating system. Can be one of linux-x86_64,
        windows-x86_64. Auto-detected by default.
  --version <version>
        The desired Qbs version.
EOF
}

case "$OSTYPE" in
    *linux*)
        HOST_OS=linux-x86_64
        INSTALL_DIR=/opt/qbs
        ;;
    msys)
        HOST_OS=windows-x86_64
        INSTALL_DIR=/c/qbs
        ;;
    *)
        HOST_OS=
        INSTALL_DIR=
        ;;
esac

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

MIRRORS="\
    http://ftp.acc.umu.se/mirror/qt.io/qtproject \
    http://ftp.fau.de/qtproject \
    http://download.qt.io \
"

for MIRROR in ${MIRRORS}; do
    if curl "${MIRROR}/official_releases/qbs/${VERSION}/" -s -f -o /dev/null; then
        echo "Selected mirror: ${MIRROR}" >&2
        break;
    else
        echo "Server ${MIRROR} not availabe. Trying next alternative..." >&2
        MIRROR=""
    fi
done

BASE_FILENAME="qbs-${HOST_OS}-${VERSION}"

DOWNLOAD_DIR=`mktemp -d 2>/dev/null || mktemp -d -t 'install-qbs'`

rm -rf ${INSTALL_DIR}/${BASE_FILENAME}
mkdir -p ${INSTALL_DIR}

echo "Downloading Qbs ${VERSION}..." >&2
cd ${DOWNLOAD_DIR}
if [[ ${HOST_OS} == "linux-x86_64" ]]; then
    FILENAME="${BASE_FILENAME}.tar.gz"
    curl -L "${MIRROR}/official_releases/qbs/${VERSION}/${FILENAME}" > ${FILENAME}
    tar -xzf ${FILENAME}
elif [[ ${HOST_OS} == "windows-x86_64" ]]; then
    FILENAME="${BASE_FILENAME}.zip"
    curl -L "${MIRROR}/official_releases/qbs/${VERSION}/${FILENAME}" > ${FILENAME}
    7z x -y -o${BASE_FILENAME} ${FILENAME} >/dev/null 2>&1
fi
mv ${BASE_FILENAME} ${INSTALL_DIR}
rm ${FILENAME}
echo "${INSTALL_DIR}/${BASE_FILENAME}/bin"

