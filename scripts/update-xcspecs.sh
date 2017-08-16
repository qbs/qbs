#!/bin/bash
set -e

#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
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

# Update build specs from Xcode - this script should be run when new Xcode releases are made.
specs_dir="$(xcrun --sdk macosx --show-sdk-platform-path)/Developer/Library/Xcode/Specifications"
specs_out_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../share/qbs/modules/bundle"
spec_files=("MacOSX Package Types.xcspec" "MacOSX Product Types.xcspec")
for spec_file in "${spec_files[@]}" ; do
    printf "%s\\n" "$(plutil -convert json -r -o - "$specs_dir/$spec_file")" > \
        "$specs_out_dir/${spec_file// /-}"
done
xcode_version="$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' \
    "$(xcode-select --print-path)/../Info.plist")"
echo "Updated build specs from Xcode $xcode_version"
