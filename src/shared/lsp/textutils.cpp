// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "textutils.h"

#include <tools/qbsassert.h>

#include <QRegularExpression>
#include <QtDebug>

namespace lsp::Utils::Text {

bool Position::operator==(const Position &other) const
{
    return line == other.line && column == other.column;
}

/*!
    Returns the text position of a \a fileName and sets the \a postfixPos if
    it can find a positional postfix.

    The following patterns are supported: \c {filepath.txt:19},
    \c{filepath.txt:19:12}, \c {filepath.txt+19},
    \c {filepath.txt+19+12}, and \c {filepath.txt(19)}.
*/

Position Position::fromFileName(QStringView fileName, int &postfixPos)
{
    static const auto regexp = QRegularExpression("[:+](\\d+)?([:+](\\d+)?)?$");
    // (10) MSVC-style
    static const auto vsRegexp = QRegularExpression("[(]((\\d+)[)]?)?$");
    const QRegularExpressionMatch match = regexp.match(fileName);
    Position pos;
    if (match.hasMatch()) {
        postfixPos = match.capturedStart(0);
        if (match.lastCapturedIndex() > 0) {
            pos.line = match.captured(1).toInt();
            if (match.lastCapturedIndex() > 2) // index 2 includes the + or : for the column number
                pos.column = match.captured(3).toInt() - 1; //column is 0 based, despite line being 1 based
        }
    } else {
        const QRegularExpressionMatch vsMatch = vsRegexp.match(fileName);
        postfixPos = vsMatch.capturedStart(0);
        if (vsMatch.lastCapturedIndex() > 1) // index 1 includes closing )
            pos.line = vsMatch.captured(2).toInt();
    }
    if (pos.line > 0 && pos.column < 0)
        pos.column = 0; // if we got a valid line make sure to return a valid TextPosition
    return pos;
}

int Range::length(const QString &text) const
{
    if (end.line < begin.line)
        return -1;

    if (begin.line == end.line)
        return end.column - begin.column;

    int index = 0;
    int currentLine = 1;
    while (currentLine < begin.line) {
        index = text.indexOf(QChar::LineFeed, index);
        if (index < 0)
            return -1;
        ++index;
        ++currentLine;
    }
    const int beginIndex = index + begin.column;
    while (currentLine < end.line) {
        index = text.indexOf(QChar::LineFeed, index);
        if (index < 0)
            return -1;
        ++index;
        ++currentLine;
    }
    return index + end.column - beginIndex;
}

bool Range::operator==(const Range &other) const
{
    return begin == other.begin && end == other.end;
}

QString utf16LineTextInUtf8Buffer(const QByteArray &utf8Buffer, int currentUtf8Offset)
{
    const int lineStartUtf8Offset = currentUtf8Offset
                                        ? (utf8Buffer.lastIndexOf('\n', currentUtf8Offset - 1) + 1)
                                        : 0;
    const int lineEndUtf8Offset = utf8Buffer.indexOf('\n', currentUtf8Offset);
    return QString::fromUtf8(
        utf8Buffer.mid(lineStartUtf8Offset, lineEndUtf8Offset - lineStartUtf8Offset));
}

static bool isByteOfMultiByteCodePoint(unsigned char byte)
{
    return byte & 0x80; // Check if most significant bit is set
}

bool utf8AdvanceCodePoint(const char *&current)
{
    if (Q_UNLIKELY(*current == '\0'))
        return false;

    // Process multi-byte UTF-8 code point (non-latin1)
    if (Q_UNLIKELY(isByteOfMultiByteCodePoint(*current))) {
        unsigned trailingBytesCurrentCodePoint = 1;
        for (unsigned char c = (*current) << 2; isByteOfMultiByteCodePoint(c); c <<= 1)
            ++trailingBytesCurrentCodePoint;
        current += trailingBytesCurrentCodePoint + 1;

    // Process single-byte UTF-8 code point (latin1)
    } else {
        ++current;
    }

    return true;
}

QDebug &operator<<(QDebug &stream, const Position &pos)
{
    stream << "line: " << pos.line << ", column: " << pos.column;
    return stream;
}

} // namespace Utils::Text
