/**
 * @author Alexander Vassilev
 * @copyright BSD License
 */

#ifndef _TSNPRINTF_H
#define _TSNPRINTF_H

#include "tostring.hpp"
#include <assert.h>
#include <stdlib.h>
#include <alloca.h>
char* tsnprintf(char* buf, size_t bufsize, const char* fmtStr);

// Returns the address of the terminating null of the written string
template <typename Val, typename ...Args>
char* tsnprintf(char* buf, size_t bufsize, const char* fmtStr, Val val, Args... args)
{
    assert(buf);
    assert(bufsize);

    char* bufend = buf+bufsize-1; //point to last char
    do
    {
        char ch = *fmtStr++;
        if (ch == '%')
        {
            // toString returns:
            // - null if it didn't manage to write everything
            // - the address of the char after the last written,
            // if it managed to write everything. However, that returned
            // address may be past the end of the buffer, so we need to check
            buf = toString<kDontNullTerminate>(buf, bufend-buf+1, val);
            if (!buf)
            {
                *bufend = 0;
                return nullptr;
            }
            if (buf >= bufend)
            {
                *bufend = 0;
                if (buf == bufend)
                {
                    return bufend;
                }
                // toString() just managed to fit everything, without the terminating zero
                // but now we don't have space for the terminator
                assert(buf - bufend == 1); // make sure we haven't gone past the end of the buffer
                // replace last char with terminator, and return null, to signal
                // that we didn't have enough space
                return nullptr;
            }
            return tsnprintf(buf, bufend-buf+1, fmtStr, args...);
        }
        else if (ch == 0)
        {
            *buf = 0;
            return buf;
        }
        else
        {
            *(buf++) = ch;
        }
    }
    while (buf < bufend);
    // we have copied char by char from the format string till we reached the end
    // of the buffer wihtout reaching the terminating zero of the fmtString.
    // Terminate the string and return nullptr - not enough buffer space
    assert(buf == bufend);
    *bufend = 0;
    return nullptr;
}

#endif
