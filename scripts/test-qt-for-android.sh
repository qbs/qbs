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

export PATH="$1:$PATH"

#
# These are set outside of this script, for instance in the Docker image
#
QT_INSTALL_DIR=/opt/Qt/${QT_VERSION}
echo "Android SDK installed at ${ANDROID_SDK_ROOT}"
echo "Android NDK installed at ${ANDROID_NDK_ROOT}"
echo "Qt installed at ${QT_INSTALL_DIR}"

# Cleaning profiles
qbs config --unset profiles.qbs_autotests-android
qbs config --unset profiles.qbs_autotests-android-qt

# Setting auto test profiles
qbs setup-android --ndk-dir ${ANDROID_HOME}/ndk-bundle --sdk-dir ${ANDROID_HOME} qbs_autotests-android
qbs setup-android --ndk-dir ${ANDROID_HOME}/ndk-bundle --sdk-dir ${ANDROID_HOME} --qt-dir ${QT_INSTALL_DIR} qbs_autotests-android-qt

export QBS_AUTOTEST_PROFILE=qbs_autotests-android
export QBS_AUTOTEST_ALWAYS_LOG_STDERR=true

if [ ! "${QT_VERSION}" \< "5.14.0" ]; then
    echo "Using multi-arch data sets for qml tests (only for qt version >= 5.14) with all architectures"
    qbs config --list
    tst_blackbox-android

    echo "Using multi-arch data sets for qml tests (only for qt version >= 5.14) with only armv7a and x86_64"
    qbs config profiles.qbs_autotests-android-qt.qbs.architectures '["armv7a","x86_64"]'
    qbs config --list
    tst_blackbox-android
fi;

echo "Using single-arch (armv7a) data sets for qml tests"
qbs config --unset profiles.qbs_autotests-android-qt.qbs.architectures
qbs config profiles.qbs_autotests-android-qt.qbs.architecture armv7a

qbs config --list

tst_blackbox-android

