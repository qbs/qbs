#include <QFile>

#include <iostream>

int main()
{
    QFile resource1(":/thePrefix/resource1.txt");
    if (!resource1.open(QIODevice::ReadOnly))
        return 1;
    QFile resource2(":/thePrefix/resource2.txt");
    if (!resource2.open(QIODevice::ReadOnly))
        return 2;
    QFile resource3(":/theOtherPrefix/resource3.txt");
    if (!resource3.open(QIODevice::ReadOnly))
        return 3;
    std::cout << "resource data: " << resource1.readAll().constData()
              << resource2.readAll().constData() << resource3.readAll().constData() << std::endl;
}
