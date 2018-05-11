#include <cstdlib>

int main()
{
#ifdef WRONG_VARIANT
    return EXIT_FAILURE;
#endif
}
