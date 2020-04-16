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

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/hostosinfo.h>

#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qobject.h>
#include <QtCore/qvariant.h>

#include <QtScript/qscriptable.h>
#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptvalue.h>

namespace qbs {
namespace Internal {

class BinaryFile : public QObject, public QScriptable, public ResourceAcquiringScriptObject
{
    Q_OBJECT
    Q_ENUMS(OpenMode)
public:
    enum OpenMode {
        ReadOnly = 1,
        WriteOnly = 2,
        ReadWrite = ReadOnly | WriteOnly
    };

    static QScriptValue ctor(QScriptContext *context, QScriptEngine *engine);
    ~BinaryFile() override;

    Q_INVOKABLE void close();
    Q_INVOKABLE QString filePath();
    Q_INVOKABLE bool atEof() const;
    Q_INVOKABLE qint64 size() const;
    Q_INVOKABLE void resize(qint64 size);
    Q_INVOKABLE qint64 pos() const;
    Q_INVOKABLE void seek(qint64 pos);
    Q_INVOKABLE QVariantList read(qint64 size);
    Q_INVOKABLE void write(const QVariantList &data);

private:
    explicit BinaryFile(QScriptContext *context, const QString &filePath, OpenMode mode = ReadOnly);

    bool checkForClosed() const;

    // ResourceAcquiringScriptObject implementation
    void releaseResources() override;

    QFile *m_file = nullptr;
};

QScriptValue BinaryFile::ctor(QScriptContext *context, QScriptEngine *engine)
{
    BinaryFile *t = nullptr;
    switch (context->argumentCount()) {
    case 0:
        return context->throwError(Tr::tr("BinaryFile constructor needs "
                                          "path of file to be opened."));
    case 1:
        t = new BinaryFile(context, context->argument(0).toString());
        break;
    case 2:
        t = new BinaryFile(context,
                           context->argument(0).toString(),
                           static_cast<OpenMode>(context->argument(1).toInt32()));
        break;
    default:
        return context->throwError(Tr::tr("BinaryFile constructor takes at most two parameters."));
    }

    const auto se = static_cast<ScriptEngine *>(engine);
    se->addResourceAcquiringScriptObject(t);
    const DubiousContextList dubiousContexts {
        DubiousContext(EvalContext::PropertyEvaluation, DubiousContext::SuggestMoving)
    };
    se->checkContext(QStringLiteral("qbs.BinaryFile"), dubiousContexts);
    se->setUsesIo();

    return engine->newQObject(t, QScriptEngine::QtOwnership);
}

BinaryFile::~BinaryFile()
{
    delete m_file;
}

BinaryFile::BinaryFile(QScriptContext *context, const QString &filePath, OpenMode mode)
{
    Q_ASSERT(thisObject().engine() == engine());

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
        context->throwError(Tr::tr("Unable to open file '%1': Undefined mode '%2'")
                            .arg(filePath, mode));
        return;
    }

    m_file = new QFile(filePath);
    if (Q_UNLIKELY(!m_file->open(m))) {
        context->throwError(Tr::tr("Unable to open file '%1': %2")
                            .arg(filePath, m_file->errorString()));
        delete m_file;
        m_file = nullptr;
    }
}

void BinaryFile::close()
{
    if (checkForClosed())
        return;
    m_file->close();
    delete m_file;
    m_file = nullptr;
}

QString BinaryFile::filePath()
{
    if (checkForClosed())
        return {};
    return QFileInfo(*m_file).absoluteFilePath();
}

bool BinaryFile::atEof() const
{
    if (checkForClosed())
        return true;
    return m_file->atEnd();
}

qint64 BinaryFile::size() const
{
    if (checkForClosed())
        return -1;
    return m_file->size();
}

void BinaryFile::resize(qint64 size)
{
    if (checkForClosed())
        return;
    if (Q_UNLIKELY(!m_file->resize(size))) {
        context()->throwError(Tr::tr("Could not resize '%1': %2")
                              .arg(m_file->fileName(), m_file->errorString()));
    }
}

qint64 BinaryFile::pos() const
{
    if (checkForClosed())
        return -1;
    return m_file->pos();
}

void BinaryFile::seek(qint64 pos)
{
    if (checkForClosed())
        return;
    if (Q_UNLIKELY(!m_file->seek(pos))) {
        context()->throwError(Tr::tr("Could not seek '%1': %2")
                              .arg(m_file->fileName(), m_file->errorString()));
    }
}

QVariantList BinaryFile::read(qint64 size)
{
    if (checkForClosed())
        return {};
    const QByteArray bytes = m_file->read(size);
    if (Q_UNLIKELY(bytes.size() == 0 && m_file->error() != QFile::NoError)) {
        context()->throwError(Tr::tr("Could not read from '%1': %2")
                              .arg(m_file->fileName(), m_file->errorString()));
    }

    QVariantList data;
    std::for_each(bytes.constBegin(), bytes.constEnd(), [&data](const char &c) {
        data.append(c); });
    return data;
}

void BinaryFile::write(const QVariantList &data)
{
    if (checkForClosed())
        return;

    QByteArray bytes;
    std::for_each(data.constBegin(), data.constEnd(), [&bytes](const QVariant &v) {
        bytes.append(char(v.toUInt() & 0xFF)); });

    const qint64 size = m_file->write(bytes);
    if (Q_UNLIKELY(size == -1)) {
        context()->throwError(Tr::tr("Could not write to '%1': %2")
                              .arg(m_file->fileName(), m_file->errorString()));
    }
}

bool BinaryFile::checkForClosed() const
{
    if (m_file)
        return false;
    if (QScriptContext *ctx = context())
        ctx->throwError(Tr::tr("Access to BinaryFile object that was already closed."));
    return true;
}

void BinaryFile::releaseResources()
{
    close();
    deleteLater();
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionBinaryFile(QScriptValue extensionObject)
{
    using namespace qbs::Internal;
    QScriptEngine *engine = extensionObject.engine();
    const QScriptValue obj = engine->newQMetaObject(&BinaryFile::staticMetaObject,
                                                    engine->newFunction(&BinaryFile::ctor));
    extensionObject.setProperty(QStringLiteral("BinaryFile"), obj);
}

Q_DECLARE_METATYPE(qbs::Internal::BinaryFile *)

#include "binaryfile.moc"
