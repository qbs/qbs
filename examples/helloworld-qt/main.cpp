#include <QCoreApplication>
#include <QTextStream>

#include <cstdio>

int main()
{
    QTextStream(stdout) << QCoreApplication::translate("hello", "Hello, World!") << endl;
}
