#ifndef _SNPRINT_H
#define _SNPRINT_H

#include "tostring.h"
#include <assert.h>

char* snprint(char* buf, uint16_t bufsize, const char* fmtStr);

template <typename Val, typename ...Args>
char* snprint(char* buf, uint16_t bufsize, const char* fmtStr, Val val, Args... args)
{
    if (!buf)
        return nullptr;
    assert(bufsize);

    char* bufend = buf+bufsize-1; //point to last char
    do
    {
        char ch = *fmtStr;
        if (ch == 0)
            break;
        if (ch == '%')
        {
            buf = toString(buf, bufend-buf+1, val);
            return snprint(buf, bufend-buf+1, fmtStr+1, args...);
        }
        *(buf++) = *(fmtStr++);
    }
    while (buf < bufend);
    *buf = 0;
    return buf;
}

char* snprint(char* buf, uint16_t bufsize, const char* fmtStr)
{
    char* bufend = buf+bufsize-1;
    while (*fmtStr && (buf < bufend))
    {
        *(buf++) = *(fmtStr++);
    }
    *buf = 0;
    return buf;
}


#endif
