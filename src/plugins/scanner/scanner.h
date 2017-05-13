/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef SCANNER_H
#define SCANNER_H


#ifdef __cplusplus
extern "C" {
#endif

#define SC_LOCAL_INCLUDE_FLAG   0x1
#define SC_GLOBAL_INCLUDE_FLAG  0x2

enum OpenScannerFlags
{
    ScanForDependenciesFlag = 0x01,
    ScanForFileTagsFlag = 0x02
};

/**
  * Open a file that's going to be scanned.
  * The file path encoding is UTF-16 on all platforms.
  * The file tags are in CSV format.
  *
  * Returns a scanner handle.
  */
typedef void *(*scanOpen_f) (const unsigned short *filePath, const char *fileTags, int flags);

/**
  * Closes the given scanner handle.
  */
typedef void  (*scanClose_f)                (void *opaq);

/**
  * Return the next result (filename) of the scan.
  */
typedef const char *(*scanNext_f)           (void *opaq, int *size, int *flags);

/**
  * Returns a list of type hints for the scanned file.
  * May return null.
  *
  * Example: if a C++ header file contains Q_OBJECT,
  * the type hint 'moc_hpp' is returned.
  */
typedef const char** (*scanAdditionalFileTags_f) (void *opaq, int *size);

enum ScannerFlags
{
    NoScannerFlags = 0x00,
    ScannerUsesCppIncludePaths = 0x01,
    ScannerRecursiveDependencies = 0x02
};

class ScannerPlugin
{
public:
    const char  *name;
    const char  *fileTags; // CSV
    scanOpen_f  open;
    scanClose_f close;
    scanNext_f  next;
    scanAdditionalFileTags_f additionalFileTags;
    int flags;
};

#ifdef __cplusplus
}
#endif
#endif // SCANNER_H
