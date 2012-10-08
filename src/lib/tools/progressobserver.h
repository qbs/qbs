
#ifndef PROGRESSOBSERVER_H
#define PROGRESSOBSERVER_H

namespace qbs {

class ProgressObserver
{
public:
    virtual void setProgressRange(int minimum, int maximum) = 0;
    virtual void setProgressValue(int value) = 0;
    virtual int progressValue() = 0;
    virtual void incrementProgressValue(int increment = 1)
    {
        setProgressValue(progressValue() + increment);
    }
};

} // namespace qbs

#endif // PROGRESSOBSERVER_H
