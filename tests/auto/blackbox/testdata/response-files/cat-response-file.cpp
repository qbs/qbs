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

#include <iostream>
#include <fstream>
#include <cstring>
#include <string>

using namespace std;

int main(int argc, char *argv[])
{
    if (argc < 3) {
        cerr << "cat-response-file: not enough arguments: " << argc - 1 << endl;
        return 1;
    }
    if (strlen(argv[2]) < 2) {
        cerr << "cat-response-file: second argument is too short: " << argv[2] << endl;
        return 2;
    }
    if (argv[2][0] != '@') {
        cerr << "cat-response-file: second argument does not start with @: " << argv[2] << endl;
        return 3;
    }
    ifstream inf(argv[2] + 1);
    if (!inf.is_open()) {
        cerr << "cat-response-file: cannot open input file " << argv[2] + 1 << endl;
        return 4;
    }
    ofstream ouf(argv[1]);
    if (!ouf.is_open()) {
        cerr << "cat-response-file: cannot open output file " << argv[1] << endl;
        return 5;
    }
    string line;
    while (getline(inf, line))
        ouf << line << endl;
    inf.close();
    ouf.close();
    return 0;
}

