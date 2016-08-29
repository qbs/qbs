/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <string>
#include <iostream>
#include <fstream>

using namespace std;

bool displayTextFile(const string &dirPath, const string &fileName)
{
    string fullPath = dirPath + fileName;
    ifstream istream(fullPath.c_str());
    if (!istream.is_open()) {
        cout << "Cannot open " << fileName << endl;
        return false;
    }
    cout << "---" << fileName << "---" << endl;
    char buf[256];
    unsigned int i = 1;
    while (istream.good()) {
        istream.getline(buf, sizeof(buf));
        cout << i++ << ": " << buf << endl;
    }
    return true;
}

int main(int, char **argv)
{
    string appPath(argv[0]);
    size_t i = appPath.find_last_of('/');
    if (i == string::npos)
        i = appPath.find_last_of('\\');
    if (i == string::npos) // No path, plain executable was called
        appPath.clear();
    else
        appPath.resize(i + 1);
    if (!displayTextFile(appPath, "foo.txt"))
        return 1;
    if (!displayTextFile(appPath, "bar.txt"))
        return 2;
    cout << "-------------" << endl;
    return 0;
}

