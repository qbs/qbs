#include "../dllexport.h"

DLL_IMPORT void plugin3_hello();
DLL_IMPORT void plugin4_hello();
DLL_IMPORT void helper_hello();

int main()
{
    plugin3_hello();
    plugin4_hello();
    helper_hello();
    return 0;
}
