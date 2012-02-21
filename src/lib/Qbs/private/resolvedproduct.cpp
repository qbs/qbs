/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/


#include "resolvedproduct.h"


#include <QtScript/QScriptEngine>

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
    } catch (qbs::Error &error) {
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
    } catch (qbs::Error &error) {
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
