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
# It might be desired to keep settings for Qbs testing
# in a separate folder.
#
export QBS_AUTOTEST_SETTINGS_DIR="${QBS_AUTOTEST_SETTINGS_DIR:-/tmp/qbs-settings}"

#
# Qbs is built with the address sanitizer enabled.
# Suppress findings in some parts of Qbs / dependencies.
#
export LSAN_OPTIONS="suppressions=$( cd "$(dirname "$0")" ; pwd -P )/address-sanitizer-suppressions.txt:print_suppressions=0"

#
# Additional build options
#
BUILD_OPTIONS="\
    ${QBS_BUILD_PROFILE:+profile:${QBS_BUILD_PROFILE}} \
    modules.qbsbuildconfig.enableAddressSanitizer:true \
    modules.qbsbuildconfig.enableProjectFileUpdates:true \
    modules.qbsbuildconfig.enableUnitTests:true \
"

#
# Build all default products of Qbs
#
qbs resolve ${BUILD_OPTIONS}
qbs build ${BUILD_OPTIONS}
qbs build -p "qbs documentation" ${BUILD_OPTIONS}

#
# Set up profiles for the freshly built Qbs if not
# explicitly specified otherwise
#
if [ -z "${QBS_AUTOTEST_PROFILE}" ]; then

    export QBS_AUTOTEST_PROFILE=autotestprofile

    RUN_OPTIONS="\
        --settings-dir ${QBS_AUTOTEST_SETTINGS_DIR} \
    "

    qbs run -p qbs_app ${BUILD_OPTIONS} -- setup-toolchains \
            ${RUN_OPTIONS} \
            --detect

    qbs run -p qbs_app ${BUILD_OPTIONS} -- setup-qt \
            ${RUN_OPTIONS} \
            "${QMAKE_PATH:-$(which qmake)}" ${QBS_AUTOTEST_PROFILE}

    qbs run -p qbs_app ${BUILD_OPTIONS} -- config \
            ${RUN_OPTIONS} \
            ${QBS_AUTOTEST_PROFILE}.baseProfile gcc

    # QBS_AUTOTEST_PROFILE has been added to the environment
    # which requires a resolve step
    qbs resolve ${BUILD_OPTIONS}
fi

#
# Run all autotests with QBS_AUTOTEST_PROFILE
#
qbs build -p "autotest-runner" ${BUILD_OPTIONS}
