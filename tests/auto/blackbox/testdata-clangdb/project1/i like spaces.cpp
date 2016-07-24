#include <iostream>

using namespace std;

#define _STR(x) #x
#define STR(x) _STR(x)

int main(int argc, char **argv)
{
    int garbage;
    int unused = garbage;
    cout << "SPACES=" << SPACES << "SPICES=" STR(SPICES) << "SLICES=" << SLICES << endl;
    return 0;
}
