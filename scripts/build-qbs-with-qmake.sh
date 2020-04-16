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
# Build all default products of Qbs
#
qmake -r qbs.pro \
        CONFIG+=qbs_enable_unit_tests \
        CONFIG+=qbs_enable_project_file_updates \
        ${BUILD_OPTIONS}
make -j $(nproc --all)
make docs

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
            "${QMAKE_PATH:-$(which qmake)}" ${QBS_AUTOTEST_PROFILE}

    ./bin/qbs config \
            ${RUN_OPTIONS} \
            ${QBS_AUTOTEST_PROFILE}.baseProfile gcc

fi


make check -j $(nproc --all)
