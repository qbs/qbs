
#ifndef PROGRESSOBSERVER_H
#define PROGRESSOBSERVER_H

#include <QtGlobal>

QT_BEGIN_NAMESPACE
class QString;
QT_END_NAMESPACE

namespace qbs {

class ProgressObserver
{
public:
    virtual ~ProgressObserver() { }

    virtual void initialize(const QString &task, int maximum) = 0;
    virtual void setProgressValue(int value) = 0;
    virtual int progressValue() = 0;
    virtual void incrementProgressValue(int increment = 1)
    {
        setProgressValue(progressValue() + increment);
    }

    virtual bool canceled() const = 0;
};

} // namespace qbs

#endif // PROGRESSOBSERVER_H
