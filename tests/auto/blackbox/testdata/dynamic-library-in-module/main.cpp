#include "../dllexport.h"

DLL_IMPORT void theOtherLibFunc();
DLL_IMPORT void theFourthLibFunc();

void theThirdLibFunc() { }

int main()
{
    theOtherLibFunc();
    theThirdLibFunc();
    theFourthLibFunc();
}
