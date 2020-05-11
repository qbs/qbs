#!/usr/bin/env bash
#############################################################################
##
## Copyright (C) 2019 Ivan Komissarov (abbapoh@gmail.com)
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

LLVM_INSTALL_DIR=${LLVM_INSTALL_DIR:-""}

# on Debian, it might be necessary to setup which version of clang-tidy and run-clang-tidy.py
# is desired:
# update-alternatives --install /usr/bin/run-clang-tidy.py run-clang-tidy.py /usr/bin/run-clang-tidy-4.0.py 1

CLANG_TIDY=`which clang-tidy`
RUN_CLANG_TIDY=`which run-clang-tidy.py`

if [ -z "$RUN_CLANG_TIDY" ] || [ -z "$CLANG_TIDY" ]; then
    if [ ! -z "$LLVM_INSTALL_DIR" ]; then
        CLANG_TIDY="$LLVM_INSTALL_DIR/bin/clang-tidy"
        RUN_CLANG_TIDY="$LLVM_INSTALL_DIR/share/clang/run-clang-tidy.py"
    else
        echo "Can't find clang-tidy and/or run-clang-tidy.py in PATH, try setting LLVM_INSTALL_DIR"
        exit 1
    fi
fi

NPROC=`which nproc`
SYSCTL=`which sysctl`
CPU_COUNT=2
if [ ! -z "$NPROC" ]; then # Linux
    CPU_COUNT=`$NPROC --all`
elif [ ! -z "$SYSCTL" ]; then  # macOS
    CPU_COUNT=`$SYSCTL -n hw.ncpu`
fi

BUILD_OPTIONS="\
    ${QBS_BUILD_PROFILE:+profile:${QBS_BUILD_PROFILE}} \
    modules.qbsbuildconfig.enableProjectFileUpdates:true \
    modules.cpp.treatWarningsAsErrors:true \
    modules.qbs.buildVariant:release \
    project.withTests:false \
    ${BUILD_OPTIONS} \
    config:analyzer
"

QBS_SRC_DIR=${QBS_SRC_DIR:-`pwd`}

if [ ! -f "$QBS_SRC_DIR/qbs.qbs" ]; then
    echo "Can't find qbs.qbs in $QBS_SRC_DIR, try setting QBS_SRC_DIR"
    exit 1
fi

set -e
set -o pipefail

qbs resolve -f "$QBS_SRC_DIR/qbs.qbs" $BUILD_OPTIONS
qbs build -f "$QBS_SRC_DIR/qbs.qbs" $BUILD_OPTIONS
qbs generate -g clangdb -f "$QBS_SRC_DIR/qbs.qbs" $BUILD_OPTIONS

SCRIPT="
import json
import os
import sys

dbFile = sys.argv[1]
blacklist = ['json.cpp']
seenFiles = set()
patched_db = []
with open(dbFile, 'r') as f:
   db = json.load(f)
   for item in db:
       file = item['file']
       if (os.path.basename(file) not in blacklist) and (file not in seenFiles):
            seenFiles.add(file)
            patched_db.append(item)

with open(dbFile, 'w') as f:
    f.write(json.dumps(patched_db, indent=2))
"
python3 -c "${SCRIPT}" analyzer/compile_commands.json

RUN_CLANG_TIDY+=" -p analyzer -clang-tidy-binary ${CLANG_TIDY} -j ${CPU_COUNT} -header-filter=\".*qbs.*\.h$\" -quiet"
${RUN_CLANG_TIDY} 2>/dev/null | tee results.txt
echo "$(grep -c 'warning:' results.txt) warnings in total"
