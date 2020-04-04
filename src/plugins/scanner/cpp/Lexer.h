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

// Copyright (c) 2008 Roberto Raggi <roberto.raggi@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef CPLUSPLUS_LEXER_H
#define CPLUSPLUS_LEXER_H

#include "CPlusPlusForwardDeclarations.h"
#include "Token.h"


namespace CPlusPlus {

class CPLUSPLUS_EXPORT Lexer
{
    Lexer(const Lexer &other);
    void operator =(const Lexer &other);

public:
    enum State {
        State_Default,
        State_MultiLineComment,
        State_MultiLineDoxyComment
    };

    Lexer(const char *firstChar, const char *lastChar);
    ~Lexer();

    bool qtMocRunEnabled() const;
    void setQtMocRunEnabled(bool onoff);

    bool cxx0xEnabled() const;
    void setCxxOxEnabled(bool onoff);

    bool objCEnabled() const;
    void setObjCEnabled(bool onoff);

    void scan(Token *tok);

    inline void operator()(Token *tok)
    { scan(tok); }

    unsigned tokenOffset() const;
    unsigned tokenLength() const;
    const char *tokenBegin() const;
    const char *tokenEnd() const;
    unsigned currentLine() const;

    bool scanCommentTokens() const;
    void setScanCommentTokens(bool onoff);

    bool scanAngleStringLiteralTokens() const;
    void setScanAngleStringLiteralTokens(bool onoff);

    void setStartWithNewline(bool enabled);

    int state() const;
    void setState(int state);

    bool isIncremental() const;
    void setIncremental(bool isIncremental);

private:
    void scan_helper(Token *tok);
    void setSource(const char *firstChar, const char *lastChar);
    static int classify(const char *string, int length, bool q, bool cxx0x);
    static int classifyObjCAtKeyword(const char *s, int n);
    static int classifyOperator(const char *string, int length);

    inline void yyinp()
    {
        if (++_currentChar == _lastChar)
            _yychar = 0;
        else {
            _yychar = *_currentChar;
            if (_yychar == '\n')
                pushLineStartOffset();
        }
    }

    void pushLineStartOffset();

private:
    struct Flags {
        unsigned _isIncremental: 1;
        unsigned _scanCommentTokens: 1;
        unsigned _scanAngleStringLiteralTokens: 1;
        unsigned _qtMocRunEnabled: 1;
        unsigned _cxx0xEnabled: 1;
        unsigned _objCEnabled: 1;
    };

    const char *_firstChar = nullptr;
    const char *_currentChar = nullptr;
    const char *_lastChar = nullptr;
    const char *_tokenStart = nullptr;
    unsigned char _yychar = 0;
    int _state = 0;
    Flags f{};
    unsigned _currentLine = 0;
};

} // end of namespace CPlusPlus


#endif // CPLUSPLUS_LEXER_H
