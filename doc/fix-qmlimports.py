#!/usr/bin/env python3
#############################################################################
##
## Copyright (C) 2017 The Qt Company Ltd.
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
from __future__ import print_function
import os
import glob
import errno
import sys
import argparse
import shutil
import urllib
from subprocess import Popen, PIPE
import re
from collections import Counter

import platform
useShell = (platform.system() == 'Windows')

gotSoup = True
try:
    from bs4 import BeautifulSoup
except ImportError:
    print('Warning: Failed to import BeautifulSoup. Some functionality is disabled.',
        file=sys.stderr)
    gotSoup = False

qmlTypeString = ' QML Type'

# Modifies a QML Type reference page to look like a generic
# JavaScript reference - Removes the 'QML Type' strings from
# the titles as well as the import statement information.
# This is used in the Installer Framework docs, which contain
# JS reference documentation generated using commands specific
# to documenting QML code.
#
# Parameters: a Beautiful Soup object constructed with an opened
# html file to process.
#
# Returns True if the element tree was modified, False otherwise
def modifyQMLReference(soup):
    pageTitle = soup.head.title.string
    soup.head.title.string = pageTitle.replace(qmlTypeString, '')
    for t in soup.find_all('h1', class_='title'):
        t.string = t.string.replace(qmlTypeString, '')

    for table in soup.find_all('table'):
        td = table.find('td')
        if td and td.string:
            if 'Import Statement:' in td.string:
                td.parent.extract()
                return True
    return False

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description = """Removes bogus import statements from the offline docs""")
    parser.add_argument('outputdir',
                        help = 'output directory of the generated html files')
    args = parser.parse_args()

    if not gotSoup:
        print('Error: This script requires the Beautiful Soup library.', file=sys.stderr)
        sys.exit(1)

    if not os.path.isdir(args.outputdir):
        print('Error: No such directory:', args.outputdir, file=sys.stderr)
        sys.exit(1)

    # compile a list of all html files in the outputdir
    htmlFiles = []
    for f in os.listdir(args.outputdir):
        fullPath = os.path.join(args.outputdir, f)
        if os.path.isdir(fullPath):
            continue
        if os.path.splitext(f)[1] == '.html':
            htmlFiles.append(fullPath)

    sys.stdout.flush()
    modified = {}
    pre_blocks = {}
    fileCount = 0
    progStep = max(16, len(htmlFiles)) / 16
    for html in htmlFiles:
        fileCount += 1
        if not (fileCount % progStep):
            print('.', end='')
            sys.stdout.flush()
        with open(html, 'r+', encoding='utf8') as file_:
            try:
                soup = BeautifulSoup(file_, 'lxml')
                actions = []
                val = 0
                if modifyQMLReference(soup):
                    actions.append('Removed QML type info')
                for a in actions:
                    modified[a] = modified.get(a, 0) + 1
                if actions:
                    file_.seek(0)
                    file_.write(str(soup))
                    file_.truncate()
                    file_.close()
            except (AttributeError, KeyError):
                print('\nFailed to parse', html, ':',
                    sys.exc_info()[0], file=sys.stderr)
            except IOError as e:
                print('\nError:', e, file=sys.stderr)
            except ValueError as e:
                print('\nError:', e, file=sys.stderr)
                if 'lxml' in str(e):
                    print('(If using pip, try \"pip install lxml\")', file=sys.stderr)
                    quit(1)
    for k, v in modified.items():
        print ('\n\t', k, 'in %d files' % v, end='')
        pb = pre_blocks.get(k, 0)
        if pb:
            print (' (', pb, ' <pre> blocks)', sep='', end='')
    print('\n')
