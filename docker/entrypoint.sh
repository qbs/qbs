#!/usr/bin/env bash
set -e

#############################################################################
##
## Copyright (C) 2019 Richard Weickelt <richard@weickelt.de>
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
# Entrypoint script when starting the container. The script checks the current
# working directory and changes the uid/gid of developer/users to match whatever
# is found in the working directory. This is useful to match the user and group
# of mounted volumes into the container

#
# If not root, re-run script as root to fix ids
#
if [ "$(id -u)" != "0" ]; then
    exec gosu root /sbin/entrypoint.sh "$@"
fi

#
# Try to determine the uid of the working directory and adjust the current
# user's uid/gid accordingly.
#
USER_GID=${USER_GID:-$(stat -c "%g" .)}
USER_UID=${USER_UID:-$(stat -c "%u" .)}
USER_NAME=${USER_NAME:-devel}
USER_GROUP=${USER_GROUP:-devel}
EXEC=""
export HOME=/home/${USER_NAME}

#
# This is a problem on Linux hosts when we mount a folder from the
# user file system and write artifacts into that. Thus, we downgrade
# the current user and make sure that the uid and gid matches the one
# of the mounted project folder.
#
# This work-around is not needed on Windows hosts as Windows doesn't
# have such a concept.
#
if [ "${USER_UID}" != "0" ]; then
    if [ "$(id -u ${USER_NAME})" != "${USER_UID}" ]; then
        usermod -o -u ${USER_UID} ${USER_NAME}
        # After changing the user's uid, all files in user's home directory
        # automatically get the new uid.
    fi
    current_gid=$(id -g ${USER_NAME})
    if [ "$(id -g ${USER_NAME})" != "${USER_GID}" ]; then
        groupmod -o -g ${USER_GID} ${USER_GROUP}
        # Set the new gid on all files in the home directory that still have the
        # old gid.
        find /home/${USER_NAME} -gid "${current_gid}" ! -type l -exec chgrp ${USER_GID} {} \;
    fi
fi
EXEC="exec gosu ${USER_NAME}:${USER_GROUP}"

if [ -z "$1" ]; then
    ${EXEC} bash -l
else
    ${EXEC} bash -l -c "$*"
fi
