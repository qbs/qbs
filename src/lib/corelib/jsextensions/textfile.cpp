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

#include "jsextension.h"

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/hostosinfo.h>

#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qtextstream.h>
#include <QtCore/qvariant.h>

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#include <QtCore5Compat/qtextcodec.h>
#else
#include <QtCore/qtextcodec.h>
#endif

namespace qbs {
namespace Internal {

class TextFile : public JsExtension<TextFile>
{
    friend class JsExtension<TextFile>;
public:
    enum OpenMode
    {
        ReadOnly = 1,
        WriteOnly = 2,
        ReadWrite = ReadOnly | WriteOnly,
        Append = 4
    };

    static const char *name() { return "TextFile"; }
    static void declareEnums(JSContext *ctx, JSValue classObj);
    static JSValue ctor(JSContext *ctx, JSValueConst, JSValueConst,
                        int argc, JSValueConst *argv, int);
    static void setupMethods(JSContext *ctx, JSValue obj);

private:
    DEFINE_JS_FORWARDER(jsClose, &TextFile::close, "TextFile.close")
    DEFINE_JS_FORWARDER(jsFilePath, &TextFile::filePath, "TextFile.filePath")
    DEFINE_JS_FORWARDER(jsSetCodec, &TextFile::setCodec, "TextFile.setCodec")
    DEFINE_JS_FORWARDER(jsReadLine, &TextFile::readLine, "TextFile.readLine")
    DEFINE_JS_FORWARDER(jsReadAll, &TextFile::readAll, "TextFile.readAll")
    DEFINE_JS_FORWARDER(jsAtEof, &TextFile::atEof, "TextFile.atEof")
    DEFINE_JS_FORWARDER(jsTruncate, &TextFile::truncate, "TextFile.truncate")
    DEFINE_JS_FORWARDER(jsWrite, &TextFile::write, "TextFile.write")
    DEFINE_JS_FORWARDER(jsWriteLine, &TextFile::writeLine, "TextFile.writeLine")

    void close();
    QString filePath();
    void setCodec(const QString &codec);
    QString readLine();
    QString readAll();
    bool atEof() const;
    void truncate();
    void write(const QString &str);
    void writeLine(const QString &str);

    TextFile(JSContext *, const QString &filePath, OpenMode mode, const QString &codec);

    void checkForClosed() const;

    std::unique_ptr<QFile> m_file;
    QTextCodec *m_codec = nullptr;
};

void TextFile::declareEnums(JSContext *ctx, JSValue classObj)
{
    DECLARE_ENUM(ctx, classObj, ReadOnly);
    DECLARE_ENUM(ctx, classObj, WriteOnly);
    DECLARE_ENUM(ctx, classObj, ReadWrite);
    DECLARE_ENUM(ctx, classObj, Append);
}

JSValue TextFile::ctor(JSContext *ctx, JSValueConst, JSValueConst,
                       int argc, JSValueConst *argv, int)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "TextFile constructor", argc, argv);
        OpenMode mode = ReadOnly;
        QString codec = QLatin1String("UTF-8");
        if (argc > 1) {
            mode = static_cast<OpenMode>
                    (fromArg<qint32>(ctx, "TextFile constructor", 2, argv[1]));
        }
        if (argc > 2) {
            codec = fromArg<QString>(ctx, "TextFile constructor", 3, argv[2]);
        }
        ScopedJsValue obj(ctx, createObject(ctx, filePath, mode, codec));

        const auto se = ScriptEngine::engineForContext(ctx);
        const DubiousContextList dubiousContexts {
            DubiousContext(EvalContext::PropertyEvaluation, DubiousContext::SuggestMoving)
        };
        se->checkContext(QStringLiteral("qbs.TextFile"), dubiousContexts);
        se->setUsesIo();
        return obj.release();
    } catch (const QString &error) { return throwError(ctx, error); }
}

void TextFile::setupMethods(JSContext *ctx, JSValue obj)
{
    setupMethod(ctx, obj, "close", &jsClose, 0);
    setupMethod(ctx, obj, "filePath", &jsFilePath, 0);
    setupMethod(ctx, obj, "atEof", &jsAtEof, 0);
    setupMethod(ctx, obj, "setCodec", &jsSetCodec, 1);
    setupMethod(ctx, obj, "readLine", &jsReadLine, 0);
    setupMethod(ctx, obj, "readAll", &jsReadAll, 0);
    setupMethod(ctx, obj, "truncate", &jsTruncate, 0);
    setupMethod(ctx, obj, "write", &jsWrite, 1);
    setupMethod(ctx, obj, "writeLine", &jsWriteLine, 1);
}

TextFile::TextFile(JSContext *, const QString &filePath, OpenMode mode, const QString &codec)
{
    auto file = std::make_unique<QFile>(filePath);
    const auto newCodec = QTextCodec::codecForName(qPrintable(codec));
    m_codec = newCodec ? newCodec : QTextCodec::codecForName("UTF-8");
    QIODevice::OpenMode m = QIODevice::NotOpen;
    if (mode & ReadOnly)
        m |= QIODevice::ReadOnly;
    if (mode & WriteOnly)
        m |= QIODevice::WriteOnly;
    if (mode & Append)
        m |= QIODevice::Append;
    m |= QIODevice::Text;
    if (Q_UNLIKELY(!file->open(m)))
        throw Tr::tr("Unable to open file '%1': %2").arg(filePath, file->errorString());
    m_file = std::move(file);
}

void TextFile::close()
{
    checkForClosed();
    m_file->close();
    m_file.reset();
}

QString TextFile::filePath()
{
    checkForClosed();
    return QFileInfo(*m_file).absoluteFilePath();
}

void TextFile::setCodec(const QString &codec)
{
    checkForClosed();
    const auto newCodec = QTextCodec::codecForName(qPrintable(codec));
    if (newCodec)
        m_codec = newCodec;
}

QString TextFile::readLine()
{
    checkForClosed();
    auto result = m_codec->toUnicode(m_file->readLine());
    if (!result.isEmpty() && result.back() == QLatin1Char('\n'))
        result.chop(1);
    return result;
}

QString TextFile::readAll()
{
    checkForClosed();
    return m_codec->toUnicode(m_file->readAll());
}

bool TextFile::atEof() const
{
    checkForClosed();
    return m_file->atEnd();
}

void TextFile::truncate()
{
    checkForClosed();
    m_file->resize(0);
}

void TextFile::write(const QString &str)
{
    checkForClosed();
    m_file->write(m_codec->fromUnicode(str));
}

void TextFile::writeLine(const QString &str)
{
    checkForClosed();
    m_file->write(m_codec->fromUnicode(str));
    m_file->putChar('\n');
}

void TextFile::checkForClosed() const
{
    if (!m_file)
        throw Tr::tr("Access to TextFile object that was already closed.");
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionTextFile(qbs::Internal::ScriptEngine *engine, JSValue extensionObject)
{
    qbs::Internal::TextFile::registerClass(engine, extensionObject);
}
