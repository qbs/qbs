#include "include/../include/shared.h"
#include "touched.h"

int a()
{
    return sharedValue() + touchedValue();
}
