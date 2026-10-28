#include "string.h"

size_t kstrlen(const char *string)
{
    size_t length = 0;

    if (string == NULL) {
        return 0;
    }

    while (string[length] != '\0') {
        length++;
    }

    return length;
}