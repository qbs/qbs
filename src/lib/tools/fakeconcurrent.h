/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#ifndef FAKECONCURRENT_H
#define FAKECONCURRENT_H

#include <qtconcurrent/runextensions.h>

namespace qbs {
namespace FakeConcurrent {

template <typename Class, typename T>
QFuture<T> run(void (Class::*fn)(QFutureInterface<T> &), Class *object)
{
    QFuture<T> result;
    QFutureInterface<T> futureInterface;
    (object->*fn)(futureInterface);
    return result;
}

template <typename Class, typename T, typename Arg1>
QFuture<T> run(void (Class::*fn)(QFutureInterface<T> &, Arg1), Class *object, const Arg1 &arg1)
{
    QFuture<T> result;
    QFutureInterface<T> futureInterface;
    (object->*fn)(futureInterface, arg1);
    return result;
}

template <typename Class, typename T, typename Arg1, typename Arg2>
QFuture<T> run(void (Class::*fn)(QFutureInterface<T> &, Arg1, Arg2), Class *object, const Arg1 &arg1, const Arg2 &arg2)
{
    QFuture<T> result;
    QFutureInterface<T> futureInterface;
    (object->*fn)(futureInterface, arg1, arg2);
    return result;
}

template <typename Class, typename T, typename Arg1, typename Arg2, typename Arg3>
QFuture<T> run(void (Class::*fn)(QFutureInterface<T> &, Arg1, Arg2, Arg3), Class *object, const Arg1 &arg1, const Arg2 &arg2, const Arg3 &arg3)
{
    QFuture<T> result;
    QFutureInterface<T> futureInterface;
    (object->*fn)(futureInterface, arg1, arg2, arg3);
    return result;
}

template <typename Class, typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4>
QFuture<T> run(void (Class::*fn)(QFutureInterface<T> &, Arg1, Arg2, Arg3, Arg4), Class *object, const Arg1 &arg1, const Arg2 &arg2, const Arg3 &arg3, const Arg4 &arg4)
{
    QFuture<T> result;
    QFutureInterface<T> futureInterface;
    (object->*fn)(futureInterface, arg1, arg2, arg3, arg4);
    return result;
}

template <typename Class, typename T, typename Arg1, typename Arg2, typename Arg3, typename Arg4, typename Arg5>
QFuture<T> run(void (Class::*fn)(QFutureInterface<T> &, Arg1, Arg2, Arg3, Arg4, Arg5), Class *object, const Arg1 &arg1, const Arg2 &arg2, const Arg3 &arg3, const Arg4 &arg4, const Arg5 &arg5)
{
    QFuture<T> result;
    QFutureInterface<T> futureInterface;
    (object->*fn)(futureInterface, arg1, arg2, arg3, arg4, arg5);
    return result;
}

} // namespace FakeConcurrent
} // namespace qbs

#endif // FAKECONCURRENT_H
