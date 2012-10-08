/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#include "resolvedproduct.h"


#include <QScriptEngine>

#include <language/language.h>

namespace Qbs {
namespace Private {

ResolvedProduct::ResolvedProduct(const QSharedPointer<qbs::ResolvedProduct> &internalResolvedBuildProduct)
    : m_internalResolvedBuildProduct(internalResolvedBuildProduct)
{
}

ResolvedProduct::~ResolvedProduct()
{
}

ResolvedProduct::ResolvedProduct(const ResolvedProduct &other)
    : m_internalResolvedBuildProduct(other.m_internalResolvedBuildProduct)
{
}

ResolvedProduct &ResolvedProduct::operator =(const ResolvedProduct &other)
{
    this->m_internalResolvedBuildProduct = other.m_internalResolvedBuildProduct;

    return *this;
}

int ResolvedProduct::setupBuildEnvironment() const
{
    try {
        QScriptEngine engine;
        m_internalResolvedBuildProduct->setupBuildEnvironment(&engine, QProcessEnvironment::systemEnvironment());
    } catch (const qbs::Error &error) {
        fprintf(stderr, "%s", qPrintable(error.toString()));
        return 1;
    }

    return 0;
}

int ResolvedProduct::setupRunEnvironment() const
{
    try {
        QScriptEngine engine;
        m_internalResolvedBuildProduct->setupRunEnvironment(&engine, QProcessEnvironment::systemEnvironment());
    } catch (const qbs::Error &error) {
        fprintf(stderr, "%s", qPrintable(error.toString()));
        return 1;
    }

    return 0;
}

QString ResolvedProduct::productId() const
{
    return m_internalResolvedBuildProduct->project->id + '/' + m_internalResolvedBuildProduct->name;
}

QStringList ResolvedProduct::buildEnvironmentStringList() const
{
    return m_internalResolvedBuildProduct->buildEnvironment.toStringList();
}

QProcessEnvironment ResolvedProduct::runEnvironment() const
{
    return m_internalResolvedBuildProduct->runEnvironment;
}

} // namespace Private
} // namespace Qbs
