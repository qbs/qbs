#ifndef LIB4_H
#define LIB4_H
#include <stdio.h>

#ifdef TEST_LIB
#   if defined(_WIN32) || defined(WIN32)
#       define LIB_EXPORT __declspec(dllexport)
#       define LIB_NO_EXPORT
#   else
#       define LIB_EXPORT __attribute__((visibility("default")))
#   endif
#else
#   define LIB_EXPORT
#endif

class LIB_EXPORT TestMe
{
public:
    TestMe();
    void hello1() const;
    inline void hello2() const { hello2Impl(); }
private:
    void hello2Impl() const;
};

#endif // LIB4_H
