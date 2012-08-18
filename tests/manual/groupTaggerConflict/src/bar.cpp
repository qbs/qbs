#include <qglobal.h>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include "bar_qt5.h"
#else
#include "bar_qt4.h"
#endif


Bar::Bar(QObject *parent) :
    QObject(parent)
{
}
