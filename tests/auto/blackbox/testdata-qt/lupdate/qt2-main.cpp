#include <QObject>

class M : public QObject
{
    Q_OBJECT

public:
    M()
    {
        auto s = tr("M message");
        // auto s2 = tr("M message 2");
    }
};

int main() {}
