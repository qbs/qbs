#!/usr/bin/env bash
set -eu

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

#
# The profile used to test Qbs. It is expected to be available in the
# environment. We use the same Qt installation for building and testing.
#
PROFILE="${PROFILE:-qt5-linux-x86_64}"
QMAKE_PATH="$(which qmake)"

#
# Additional build options
#
OPTIONS="\
    profile:${PROFILE} \
    modules.enableAddressSanitizer:true\
"

#
# Build all default products of Qbs
#
qbs build ${OPTIONS}

#
# Set up profiles for the freshly built Qbs
#
qbs run -p qbs_app ${OPTIONS} -- setup-toolchains --detect
qbs run -p qbs_app ${OPTIONS} -- setup-qt ${QMAKE_PATH} testprofile

#
# Run all autotests with QBS_AUTOTEST_PROFILE.
#
QBS_AUTOTEST_PROFILE="testprofile"
qbs resolve
qbs build -p "autotest-runner" ${OPTIONS}