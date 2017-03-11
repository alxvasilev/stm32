/**
 * @author Alexander Vassilev
 * @copyright BSD License
 */

#ifndef _SNPRINT_H
#define _SNPRINT_H

#include "tostring.h"
#include <assert.h>
#include <stdlib.h>
#include <utility>

typedef void(*PrintSinkFunc)(const char* str, uint16_t len, uint8_t fd, void* userp);

void setPrintSink(PrintSinkFunc func, void* arg);
PrintSinkFunc printSink();
void* printSinkUserp();

#ifndef NO_EMBEDDED
void semihostingPrintSink(const char* str, uint32_t len, int fd=1);
#else
void standardPrintSink(const char* str, uint32_t len, int fd=1);
#endif

char* tsnprintf(char* buf, uint32_t bufsize, const char* fmtStr);

template <typename Val>
std::pair<char*, const char*> toStringf(char* buf, uint32_t bufsize, const char* fmtStr, Val val)
{
    if (!buf || !fmtStr)
        return std::make_pair(nullptr, nullptr);
    assert(bufsize);

    char* bufend = buf+bufsize; //point to last char, used for remaining bufsize calculation
    char* buflast = bufend-1; //do the loop up to this point, reserve one char for terminating null
    do
    {
        char ch = *fmtStr;
        if (ch == 0)
        {
            *buf = 0;
            return std::make_pair(buf, (const char*)nullptr);
        }
        if (ch == '%')
        {
            buf = toString(buf, bufend-buf, val);
            return std::make_pair(buf, fmtStr+1);
        }
        *(buf++) = *(fmtStr++);
    }
    while (buf < buflast); //buf depleted(left one space of null terminator), but we still have what to print
    *buf = 0;
    return std::make_pair<char*, const char*>(nullptr, nullptr);
}

template <typename ...Args>
char* tsnprintf(char* buf, uint32_t bufsize, const char* fmtStr, Args... args)
{
    char* bufend = buf+bufsize;
    std::pair<char*, const char*> ret;
    std::initializer_list<char> list = {
        (ret = toStringf(buf, bufend-buf, fmtStr, args),
        buf=ret.first,
        fmtStr=ret.second,
        (char)0)...
    };
    (void)list; //silence unused var warning
    if (ret.second) //we still have format string contents to print
        buf = toString(buf, bufend-buf, fmtStr);

    if (buf)
        *buf = 0;
    return buf;
}

template <int32_t BufSize=64, typename ...Args>
uint16_t ftprintf(uint8_t fd, const char* fmtStr, Args... args)
{
    extern PrintSinkFunc gPrintSinkFunc;
    extern void* gPrintSinkUserp;

    uint16_t bufsize = BufSize & 0x7fffffff;
    char sbuf[BufSize & 0x7fffffff];
    char* buf = sbuf;
    char* ret;
    for(;;)
    {
        ret = tsnprintf(buf, bufsize, fmtStr, args...);
        if (ret)
            break;

        if (BufSize < 0) //no dynamic buffer allower
            return 0;

        //have to increase buf size
        bufsize *= 2;
        if (bufsize > 10240)
        {
            //too much, bail out
            if (buf != sbuf)
                free(buf);
            return 0;
        }
        buf = (buf == sbuf)
            ? (char*)malloc(bufsize)
            : (char*)realloc(buf, bufsize);
        if (!buf)
            return 0;
    }
    uint32_t size = ret-buf;
    gPrintSinkFunc(buf, size, fd, gPrintSinkUserp);
    if (buf != sbuf)
        free(buf);
    return size;
}

template <int32_t BufSize=64, typename ...Args>
uint16_t tprintf(const char* fmtStr, Args... args)
{
    return ftprintf<BufSize>(1, fmtStr, args...);
}

static inline void puts(const char* str, uint16_t len)
{
    extern PrintSinkFunc gPrintSinkFunc;
    extern void* gPrintSinkUserp;
    gPrintSinkFunc(str, len, 1, gPrintSinkUserp);
}

#endif
