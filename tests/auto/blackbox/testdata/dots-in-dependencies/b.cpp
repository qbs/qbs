// Quoted include with embedded ".."; the scanner turns this into an absolute,
// unclean path. b.cpp must not be rebuilt when only touched.h changes.
#include "include/../include/shared.h"
#include "other.h"

int a();

int main()
{
    return a() + sharedValue() + otherValue();
}
