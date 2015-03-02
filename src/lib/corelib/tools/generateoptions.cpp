/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2015 Jake Petroules.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/
#include "generateoptions.h"

#include <QSharedData>
#include <QString>

namespace qbs {
namespace Internal {

class GenerateOptionsPrivate : public QSharedData
{
public:
    GenerateOptionsPrivate()
        : generatorName()
    {}

    QString generatorName;
};

} // namespace Internal

/*!
 * \class GenerateOptions
 * \brief The \c GenerateOptions class comprises parameters that influence the behavior of
 * generate operations.
 */

GenerateOptions::GenerateOptions() : d(new Internal::GenerateOptionsPrivate)
{
}

GenerateOptions::GenerateOptions(const GenerateOptions &other) : d(other.d)
{
}

GenerateOptions &GenerateOptions::operator=(const GenerateOptions &other)
{
    d = other.d;
    return *this;
}

GenerateOptions::~GenerateOptions()
{
}

/*!
 * Returns the name of the generator used to create the external build system files.
 * The default is empty.
 */
QString GenerateOptions::generatorName() const
{
    return d->generatorName;
}

/*!
 * \brief Sets the name of the generator used to create the external build system files.
 */
void GenerateOptions::setGeneratorName(const QString &generatorName)
{
    d->generatorName = generatorName;
}

} // namespace qbs
