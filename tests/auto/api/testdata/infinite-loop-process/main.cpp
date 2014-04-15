#include <QThread>

class MyThread : public QThread
{
public:
    static void mySleep(unsigned long secs) { sleep(secs); } // sleep() is protected in Qt 4.
};

int main()
{
    MyThread::mySleep(60);
}
