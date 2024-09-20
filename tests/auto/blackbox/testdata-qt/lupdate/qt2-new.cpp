#include <QObject>

class N : public QObject
{
    Q_OBJECT

public:
    N() { auto s = tr("N message"); }
};
