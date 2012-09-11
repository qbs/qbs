#include "lib4.h"

TestMe::TestMe()
{
}

void TestMe::hello1() const
{
    puts("lib4 says hello!");
}

void TestMe::hello2Impl() const
{
    puts("lib4 says hello inline!");
}
