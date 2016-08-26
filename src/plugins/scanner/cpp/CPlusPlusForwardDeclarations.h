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

#ifndef CPLUSPLUS_CPLUSPLUSFORWARDDECLARATIONS_H
#define CPLUSPLUS_CPLUSPLUSFORWARDDECLARATIONS_H

#include <cstdlib>
#include <cstddef>

#ifndef CPLUSPLUS_WITHOUT_QT
#  include <QtCore/qglobal.h>

//#  if defined(CPLUSPLUS_BUILD_LIB)
//#    define CPLUSPLUS_EXPORT Q_DECL_EXPORT
//#  elif defined(CPLUSPLUS_BUILD_STATIC_LIB)
//#    define CPLUSPLUS_EXPORT
//#  else
//#    define CPLUSPLUS_EXPORT Q_DECL_IMPORT
//#  endif
//#else
#  define CPLUSPLUS_EXPORT
#endif

namespace CPlusPlus {

class TranslationUnit;
class Control;
class MemoryPool;
class DiagnosticClient;

class Identifier;
class Literal;
class StringLiteral;
class NumericLiteral;

class SymbolTable;

// names
class NameVisitor;
class Name;
class Identifier;
class TemplateNameId;
class DestructorNameId;
class OperatorNameId;
class ConversionNameId;
class QualifiedNameId;
class SelectorNameId;

// types
class TypeMatcher;
class FullySpecifiedType;
class TypeVisitor;
class Type;
class UndefinedType;
class VoidType;
class IntegerType;
class FloatType;
class PointerToMemberType;
class PointerType;
class ReferenceType;
class ArrayType;
class NamedType;

// symbols
class SymbolVisitor;
class Symbol;
class Scope;
class UsingNamespaceDirective;
class UsingDeclaration;
class Declaration;
class Argument;
class TypenameArgument;
class Function;
class Namespace;
class NamespaceAlias;
class Template;
class BaseClass;
class Block;
class Class;
class Enum;
class ForwardClassDeclaration;

class Token;

// Objective-C symbols
class ObjCBaseClass;
class ObjCBaseProtocol;
class ObjCClass;
class ObjCForwardClassDeclaration;
class ObjCProtocol;
class ObjCForwardProtocolDeclaration;
class ObjCMethod;
class ObjCPropertyDeclaration;

} // end of namespace CPlusPlus

#endif // CPLUSPLUS_CPLUSPLUSFORWARDDECLARATIONS_H
