#!/usr/bin/env bash
#############################################################################
##
## Copyright (C) 2019 Richard Weickelt.
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
# Qbs is built with the address sanitizer enabled.
# Suppress findings in some parts of Qbs / dependencies.
#
export LSAN_OPTIONS="suppressions=$( cd "$(dirname "$0")" ; pwd -P )/address-sanitizer-suppressions.txt:print_suppressions=0"

if [ -z "${QBS_BUILD_PROFILE}" ]; then
    QBS_BUILD_PROFILE=$(qbs config defaultProfile | cut -d: -f2 | tr -d '[:space:]')
fi
if [ -z "${QBS_BUILD_PROFILE}" ]; then
    echo "Either QBS_BUILD_PROFILE or a defaultProfile must be set."
    exit 1
fi

#
# Additional build options
#
BUILD_OPTIONS="\
    profile:${QBS_BUILD_PROFILE} \
    modules.qbsbuildconfig.enableAddressSanitizer:true \
    modules.qbsbuildconfig.enableProjectFileUpdates:true \
    modules.qbsbuildconfig.enableUnitTests:true \
    modules.cpp.treatWarningsAsErrors:true \
    modules.cpp.separateDebugInformation:true \
    ${BUILD_OPTIONS} \
    config:release \
"

#
# Build all default products of Qbs
#
qbs resolve ${BUILD_OPTIONS}
qbs build ${BUILD_OPTIONS}

WITH_DOCS=${WITH_DOCS:-1}
if [ "$WITH_DOCS" -ne 0 ]; then
    qbs build -p "qbs documentation" ${BUILD_OPTIONS}
fi

QMAKE_PATH=${QMAKE_PATH:-$(which qmake)}

#
# Set up profiles for the freshly built Qbs if not
# explicitly specified otherwise
#
if [ -z "${QBS_AUTOTEST_PROFILE}" ]; then

    export QBS_AUTOTEST_PROFILE=autotestprofile
    export QBS_AUTOTEST_SETTINGS_DIR=`mktemp -d 2>/dev/null || mktemp -d -t 'qbs-settings'`

    QBS_AUTOTEST_QMAKE_PATH=${QBS_AUTOTEST_QMAKE_PATH:-${QMAKE_PATH}}

    RUN_OPTIONS="\
        --settings-dir ${QBS_AUTOTEST_SETTINGS_DIR} \
    "

    qbs run -p qbs_app ${BUILD_OPTIONS} -- setup-toolchains \
            ${RUN_OPTIONS} \
            --detect

    qbs run -p qbs_app ${BUILD_OPTIONS} -- setup-qt \
            ${RUN_OPTIONS} \
            "${QBS_AUTOTEST_QMAKE_PATH}" ${QBS_AUTOTEST_PROFILE}

    # Make sure that the Qt profile uses the same toolchain profile
    # that was used for building in case a custom QBS_BUILD_PROFILE
    # was set. Otherwise setup-qt automatically uses the default
    # toolchain profile.
    if [ -z "${QBS_AUTOTEST_BASE_PROFILE}" ]; then
        QBS_AUTOTEST_BASE_PROFILE=$(qbs config profiles.${QBS_BUILD_PROFILE}.baseProfile | cut -d: -f2)
    fi
    if [ ! -z "${QBS_AUTOTEST_BASE_PROFILE}" ]; then
        echo "Setting base profile for ${QBS_AUTOTEST_PROFILE} to ${QBS_AUTOTEST_BASE_PROFILE}"
        qbs run -p qbs_app ${BUILD_OPTIONS} -- config \
                ${RUN_OPTIONS} \
                profiles.${QBS_AUTOTEST_PROFILE}.baseProfile ${QBS_AUTOTEST_BASE_PROFILE}
    fi

    qbs run -p qbs_app ${BUILD_OPTIONS} -- config \
            ${RUN_OPTIONS} \
            --list

    # QBS_AUTOTEST_PROFILE has been added to the environment
    # which requires a resolve step
    qbs resolve ${BUILD_OPTIONS}
fi

#
# Run all autotests with QBS_AUTOTEST_PROFILE. Some test cases might run for
# over 10 minutes. Output an empty line every 9:50 minutes to prevent a 10min
# timeout on Travis CI.
#
(while true; do echo "" && sleep 590; done) &
trap "kill $!; wait $! 2>/dev/null || true; killall sleep || true" EXIT
qbs build -p "autotest-runner" ${BUILD_OPTIONS}
