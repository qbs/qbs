/****************************************************************************
**
** Copyright (C) 2017 Sergey Belyashov <sergey.belyashov@gmail.com>
** Copyright (C) 2017 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2017 The Qt Company Ltd.
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
#include <tools/stlutils.h>

#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>

namespace qbs {
namespace Internal {

class BinaryFile : public JsExtension<BinaryFile>
{
    friend class JsExtension<BinaryFile>;
public:
    enum OpenMode {
        ReadOnly = 1,
        WriteOnly = 2,
        ReadWrite = ReadOnly | WriteOnly
    };

    static const char *name() { return "BinaryFile"; }
    static void declareEnums(JSContext *ctx, JSValue classObj);
    static JSValue ctor(JSContext *ctx, JSValueConst, JSValueConst,
                        int argc, JSValueConst *argv, int);

private:
    static void setupMethods(JSContext *ctx, JSValue obj);

    DEFINE_JS_FORWARDER(jsClose, &BinaryFile::close, "BinaryFile.close")
    DEFINE_JS_FORWARDER(jsFilePath, &BinaryFile::filePath, "BinaryFile.filePath")
    DEFINE_JS_FORWARDER(jsAtEof, &BinaryFile::atEof, "BinaryFile.atEof")
    DEFINE_JS_FORWARDER(jsSize, &BinaryFile::size, "BinaryFile.size")
    DEFINE_JS_FORWARDER(jsResize, &BinaryFile::resize, "BinaryFile.resize")
    DEFINE_JS_FORWARDER(jsPos, &BinaryFile::pos, "BinaryFile.pos")
    DEFINE_JS_FORWARDER(jsSeek, &BinaryFile::seek, "BinaryFile.seek")
    DEFINE_JS_FORWARDER(jsRead, &BinaryFile::read, "BinaryFile.read")
    DEFINE_JS_FORWARDER(jsWrite, &BinaryFile::write, "BinaryFile.write")

    void close();
    QString filePath();
    bool atEof() const;
    qint64 size() const;
    void resize(qint64 size);
    qint64 pos() const;
    void seek(qint64 pos);
    QByteArray read(qint64 size);
    void write(const QByteArray &data);

    explicit BinaryFile(JSContext *, const QString &filePath, OpenMode mode);

    void checkForClosed() const;

    std::unique_ptr<QFile> m_file;
};

void BinaryFile::declareEnums(JSContext *ctx, JSValue classObj)
{
    DECLARE_ENUM(ctx, classObj, ReadOnly);
    DECLARE_ENUM(ctx, classObj, WriteOnly);
    DECLARE_ENUM(ctx, classObj, ReadWrite);
}

void BinaryFile::setupMethods(JSContext *ctx, JSValue obj)
{
    setupMethod(ctx, obj, "close", &jsClose, 0);
    setupMethod(ctx, obj, "filePath", &jsFilePath, 0);
    setupMethod(ctx, obj, "atEof", &jsAtEof, 0);
    setupMethod(ctx, obj, "size", &jsSize, 0);
    setupMethod(ctx, obj, "resize", &jsResize, 0);
    setupMethod(ctx, obj, "pos", &jsPos, 0);
    setupMethod(ctx, obj, "seek", &jsSeek, 0);
    setupMethod(ctx, obj, "read", &jsRead, 0);
    setupMethod(ctx, obj, "write", &jsWrite, 0);
}

JSValue BinaryFile::ctor(JSContext *ctx, JSValueConst, JSValueConst,
                         int argc, JSValueConst *argv, int)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "BinaryFile constructor", argc, argv);
        OpenMode mode = ReadOnly;
        if (argc > 1) {
            mode = static_cast<OpenMode>
                    (fromArg<qint32>(ctx, "BinaryFile constructor", 2, argv[1]));
        }
        const JSValue obj = createObject(ctx, filePath, mode);

        const auto se = ScriptEngine::engineForContext(ctx);
        const DubiousContextList dubiousContexts {
            DubiousContext(EvalContext::PropertyEvaluation, DubiousContext::SuggestMoving)
        };
        se->checkContext(QStringLiteral("qbs.BinaryFile"), dubiousContexts);
        se->setUsesIo();
        return obj;
    } catch (const QString &error) { return throwError(ctx, error); }
}

BinaryFile::BinaryFile(JSContext *, const QString &filePath, OpenMode mode)
{
    QIODevice::OpenMode m = QIODevice::NotOpen;
    switch (mode) {
    case ReadWrite:
        m = QIODevice::ReadWrite;
        break;
    case ReadOnly:
        m = QIODevice::ReadOnly;
        break;
    case WriteOnly:
        m = QIODevice::WriteOnly;
        break;
    default:
        throw Tr::tr("Unable to open file '%1': Undefined mode '%2'").arg(filePath).arg(mode);
    }

    auto file = std::make_unique<QFile>(filePath);
    if (Q_UNLIKELY(!file->open(m)))
        throw Tr::tr("Unable to open file '%1': %2").arg(filePath, file->errorString());
    m_file = std::move(file);
}

void BinaryFile::close()
{
    checkForClosed();
    m_file->close();
    m_file.reset();
}

QString BinaryFile::filePath()
{
    checkForClosed();
    return QFileInfo(*m_file).absoluteFilePath();
}

bool BinaryFile::atEof() const
{
    checkForClosed();
    return m_file->atEnd();
}

qint64 BinaryFile::size() const
{
    checkForClosed();
    return m_file->size();
}

void BinaryFile::resize(qint64 size)
{
    checkForClosed();
    if (Q_UNLIKELY(!m_file->resize(size)))
        throw Tr::tr("Could not resize '%1': %2").arg(m_file->fileName(), m_file->errorString());
}

qint64 BinaryFile::pos() const
{
    checkForClosed();
    return m_file->pos();
}

void BinaryFile::seek(qint64 pos)
{
    checkForClosed();
    if (Q_UNLIKELY(!m_file->seek(pos)))
        throw Tr::tr("Could not seek '%1': %2").arg(m_file->fileName(), m_file->errorString());
}

QByteArray BinaryFile::read(qint64 size)
{
    checkForClosed();
    QByteArray bytes = m_file->read(size);
    if (Q_UNLIKELY(bytes.size() == 0 && m_file->error() != QFile::NoError)) {
        throw (Tr::tr("Could not read from '%1': %2")
                .arg(m_file->fileName(), m_file->errorString()));
    }
    return bytes;
}

void BinaryFile::write(const QByteArray &data)
{
    checkForClosed();
    const qint64 size = m_file->write(data);
    if (Q_UNLIKELY(size == -1)) {
        throw Tr::tr("Could not write to '%1': %2")
                .arg(m_file->fileName(), m_file->errorString());
    }
}

void BinaryFile::checkForClosed() const
{
    if (!m_file)
        throw Tr::tr("Access to BinaryFile object that was already closed.");
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionBinaryFile(qbs::Internal::ScriptEngine *engine, JSValue extensionObject)
{
    qbs::Internal::BinaryFile::registerClass(engine, extensionObject);
}
