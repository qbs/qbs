#ifndef SYMBOLSTEST_H
#define SYMBOLSTEST_H

#include <QtCore/qglobal.h>

#if defined(SYMBOLSTEST_LIBRARY)
#  define SYMBOLSTEST_EXPORT Q_DECL_EXPORT
#else
#  define SYMBOLSTEST_EXPORT Q_DECL_IMPORT
#endif

class SYMBOLSTEST_EXPORT SymbolsTest
{
  public:
    SymbolsTest();
};

#endif // SYMBOLSTEST_H
