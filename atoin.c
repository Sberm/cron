#include <ctype.h>

#include "atoin.h"

/* implemented without overflow checking */
int atoin(char *str, size_t n)
{
    int result = 0;
    for (size_t idx = 0; idx < n; idx++) {
        char *cur = str + idx;
        int num;

        if (!isdigit(*cur))
            return 0;

        num = *cur - '0';
        result *= 10;
        result += num;
    }
    return result;
}
