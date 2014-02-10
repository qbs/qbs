/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QBS_QTMOCSCANNER_H
#define QBS_QTMOCSCANNER_H

#include <language/language.h>
#include <logging/logger.h>

#include <QHash>
#include <QScriptValue>
#include <QString>

QT_BEGIN_NAMESPACE
class QScriptContext;
QT_END_NAMESPACE

class ScannerPlugin;

namespace qbs {
namespace Internal {

class Artifact;
class ScanResultCache;

class QtMocScanner
{
public:
    explicit QtMocScanner(const ResolvedProductPtr &product, QScriptValue targetScriptValue,
            const Logger &logger);
    ~QtMocScanner();

private:
    void findIncludedMocCppFiles();
    static QScriptValue js_apply(QScriptContext *ctx, QScriptEngine *engine, void *data);
    QScriptValue apply(QScriptEngine *engine, const Artifact *artifact);

    const ResolvedProductPtr &m_product;
    QScriptValue m_targetScriptValue;
    const Logger &m_logger;
    ScanResultCache *m_scanResultCache;
    QHash<QString, QString> m_includedMocCppFiles;
    ScannerPlugin *m_cppScanner;
    ScannerPlugin *m_hppScanner;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_QTMOCSCANNER_H
