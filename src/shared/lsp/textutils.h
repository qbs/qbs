// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QMetaType>
#include <QString>

namespace lsp::Utils::Text {

class Position
{
public:
    int line = 0; // 1-based
    int column = -1; // 0-based

    bool operator<(const Position &other) const
    { return line < other.line || (line == other.line && column < other.column); }
    bool operator==(const Position &other) const;

    bool operator!=(const Position &other) const { return !(operator==(other)); }

    bool isValid() const { return line > 0 && column >= 0; }

    static Position fromFileName(QStringView fileName, int &postfixPos);
};

class Range
{
public:
    int length(const QString &text) const;

    Position begin;
    Position end;

    bool operator<(const Range &other) const { return begin < other.begin; }
    bool operator==(const Range &other) const;

    bool operator!=(const Range &other) const { return !(operator==(other)); }
};

QString utf16LineTextInUtf8Buffer(const QByteArray &utf8Buffer, int currentUtf8Offset);

QDebug &operator<<(QDebug &stream, const Position &pos);

} // Text
