/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
  *
  * Returns a scanner handle.
  */
typedef void *(*scanOpen_f) (const unsigned short *filePath, int flags);

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
    const char  *fileTag;
    scanOpen_f  open;
    scanClose_f close;
    scanNext_f  next;
    scanAdditionalFileTags_f additionalFileTags;
    int flags;
};

typedef ScannerPlugin **(*getScanners_f)();

#ifdef __cplusplus
}
#endif
#endif // SCANNER_H
