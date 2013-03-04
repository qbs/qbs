#include "qttools.h"

QT_BEGIN_NAMESPACE
uint qHash(const QStringList &list)
{
    uint s = 0;
    foreach (const QString &n, list)
        s ^= qHash(n) + 0x9e3779b9 + (s << 6) + (s >> 2);
    return s;
}
QT_END_NAMESPACE
