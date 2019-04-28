/**
 * @author Alexander Vassilev
 * @copyright BSD License
 */

#ifndef _TPRINTF_H
#define _TPRINTF_H

#include "tsnprintf.hpp"
#include <assert.h>
#include <stdlib.h>
#include <alloca.h>

#ifndef STM32PP_TPRINTF_MAX_DYNAMIC_BUFSIZE
    #define STM32PP_TPRINTF_MAX_DYNAMIC_BUFSIZE 10240
#endif

struct IPrintSink
{
    virtual bool isAsync() const { return false; }
    virtual void print(const char* str, size_t len, int fd=1) = 0;
};
struct IAsyncPrintSink: IPrintSink
{
    virtual char* malloc(size_t size) = 0;
    virtual char* realloc(void* buf, size_t newSize) = 0;
    virtual bool isAsync() const { return true; }
};

extern IPrintSink* gPrintSink;

template <int BufSize, typename... Args>
typename std::enable_if<(BufSize > 0), size_t>::type
ftprintf(uint8_t fd, const char* fmtStr, Args... args)
{
    extern IPrintSink* gPrintSink;
    char sbuf[BufSize];
    char* ret = tsnprintf(sbuf, BufSize, fmtStr, args...);
    size_t size = ret-sbuf;
    gPrintSink->print(sbuf, size, fd);
    return size;
}

template <int BufSize=-64, typename... Args>
typename std::enable_if<(BufSize < 0), size_t>::type
ftprintf(uint8_t fd, const char* fmtStr, Args... args)
{
    enum: size_t { InitialBufSize = -BufSize };
    size_t bufsize = InitialBufSize;
    char* sbuf; // static buf
    char* buf; // dynamic buf
    bool sinkIsAsync = gPrintSink->isAsync();
    if (sinkIsAsync)
    {
        sbuf = nullptr;
        buf = (char*)malloc(InitialBufSize);
    }
    else
    {
        buf = sbuf = (char*)alloca(InitialBufSize);
    }
    char* ret;
    for(;;)
    {
        ret = tsnprintf(buf, bufsize, fmtStr, args...);
        if (ret)
        {
            break;
        }
        // tsnprintf() returned nullptr, have to increase buf size
        bufsize *= 2;
        if (bufsize > STM32PP_TPRINTF_MAX_DYNAMIC_BUFSIZE)
        {
            //too much, bail out
            if (buf != sbuf) // buffer is dynamic, free it
            {
                free(buf);
            }
            return 0;
        }
        buf = (buf == sbuf)
            ? (char*)malloc(bufsize)
            : (char*)realloc(buf, bufsize);
        if (!buf)
        {
            return 0;
        }
    }
    size_t size = ret-buf;
    gPrintSink->print(buf, size, fd);
    if ((buf != sbuf) && !sinkIsAsync)
    {
        free(buf);
    }
    return size;
}

template <int BufSize=-64, typename ...Args>
uint16_t tprintf(const char* fmtStr, Args... args)
{
    return ftprintf<BufSize>(1, fmtStr, args...);
}

static inline void puts(const char* str, uint16_t len)
{
    gPrintSink->print(str, len, 1);
}

#endif
