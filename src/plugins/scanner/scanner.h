/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
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

/**
  * Open a file that's going to be scanned.
  * The file path encoding is UTF-16 on all platforms.
  *
  * If the scanner is used for more than one type hint (e.g. C++ header / source)
  * the scanner can read the parameter fileTag which file type it is going to scan.
  *
  * Returns a scanner handle.
  */
typedef void *(*scanOpen_f)                 (const unsigned short *filePath, char **fileTags, int numFileTags);

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

struct ScannerPlugin
{
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
