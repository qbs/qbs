#!/usr/bin/env bash
#############################################################################
##
## Copyright (C) 2020 Ivan Komissarov (abbapoh@gmail.com).
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
set -e

#
# It might be desired to keep settings for Qbs testing
# in a separate folder.
#
export QBS_AUTOTEST_SETTINGS_DIR="${QBS_AUTOTEST_SETTINGS_DIR:-/tmp/qbs-settings}"

BUILD_OPTIONS="\
    -DWITH_UNIT_TESTS=1 \
    -DQBS_INSTALL_HTML_DOCS=1 \
    -DQBS_INSTALL_QCH_DOCS=1 \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    ${BUILD_OPTIONS} \
"

QMAKE_PATH="${QMAKE_PATH:-$(which qmake)}"
QT_DIR=$(dirname ${QMAKE_PATH})/../

# Use shadow build
mkdir -p build
pushd build

#
# Build all default products of Qbs
#
cmake -GNinja -DQt5_DIR=${QT_DIR}/lib/cmake/Qt5/ ${BUILD_OPTIONS} ..
cmake --build .
cmake --install . --prefix "install-root"

#
# Set up profiles for the freshly built Qbs if not
# explicitly specified otherwise
#
if [ -z "${QBS_AUTOTEST_PROFILE}" ]; then

    export QBS_AUTOTEST_PROFILE=autotestprofile
    RUN_OPTIONS="\
        --settings-dir ${QBS_AUTOTEST_SETTINGS_DIR} \
    "

    ./bin/qbs setup-toolchains \
            ${RUN_OPTIONS} \
            --detect

    ./bin/qbs setup-qt \
            ${RUN_OPTIONS} \
            "${QMAKE_PATH}" ${QBS_AUTOTEST_PROFILE}

    ./bin/qbs config \
            ${RUN_OPTIONS} \
            ${QBS_AUTOTEST_PROFILE}.baseProfile gcc

fi

#
# Run all autotests with QBS_AUTOTEST_PROFILE. Some test cases might run for
# over 10 minutes. Output an empty line every 9:50 minutes to prevent a 10min
# timeout on Travis CI.
#
(while true; do echo "" && sleep 590; done) &
trap "kill $!; wait $! 2>/dev/null || true; killall sleep || true" EXIT
ctest -j $(nproc --all)

popd
