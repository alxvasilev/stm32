#include <stddef.h> // for size_t
#include <assert.h>

// Trivial case for the tsnprintf recursion.
char* tsnprintf(char* buf, size_t bufsize, const char* fmtStr)
{
    if (!buf)
        return nullptr;

    char* bufend = buf+bufsize-1;
    while (*fmtStr)
    {
        if (buf >= bufend)
        {
            assert(buf == bufend);
            *buf = 0;
            return nullptr;
        }
        *(buf++) = *(fmtStr++);
    }
    *buf = 0;
    return buf;
}
