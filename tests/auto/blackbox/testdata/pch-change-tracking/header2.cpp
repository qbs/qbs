#include "header2.h"
// header1 is forced-included via pch.

void printPersonalGreeting()
{
    printGreeting();
    std::cout << "Was geht, Rumpelstilzchen?" << std::endl;
}
