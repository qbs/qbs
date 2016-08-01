#include <stdlib.h>

int main()
{
    void *memory = malloc(8);
    free(memory);
    return 0;
}
