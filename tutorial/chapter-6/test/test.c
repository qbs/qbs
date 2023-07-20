#include "lib.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    if (argc > 2) {
        printf("usage: test [value]\n");
        return 1;
    }
    const char *expected = argc == 2 ? argv[1] : "Hello from library";
    if (strcmp(get_string(), expected) != 0) {
        printf("text differs\n");
        return 1;
    }
    return 0;
}
