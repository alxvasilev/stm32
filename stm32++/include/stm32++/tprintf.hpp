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

#ifndef STM32PP_TPRINTF_ASYNC_EXPAND_STEP
    #define STM32PP_TPRINTF_ASYNC_EXPAND_STEP 64
#endif

#ifndef STM32PP_TPRINTF_SYNC_EXPAND_STEP
    #define STM32PP_TPRINTF_SYNC_EXPAND_STEP 128
#endif

struct IPrintSink
{
    /**
     * @brief waitReady Waits till the sink has completed the last print operation, if any
     * @return Whether the sink is async
     */
    virtual bool waitReady() const { return false; }
    virtual void print(const char* str, size_t len, int fd=1) = 0;
};

struct IAsyncPrintSink: public IPrintSink
{
    virtual char* detachBuffer(size_t& bufSize) = 0;
    virtual void attachBuffer(char* buf, size_t bufSize) = 0;
};

static inline IPrintSink* setPrintSink(IPrintSink* newSink)
{
    extern IPrintSink* gPrintSink;
    bool isAsync = gPrintSink->waitReady();
    if (isAsync)
    {
        size_t bufSize;
        char* buf = static_cast<IAsyncPrintSink*>(gPrintSink)->detachBuffer(bufSize);
        if (buf)
        {
            if (newSink->waitReady())
            { // newSink is async
                static_cast<IAsyncPrintSink*>(newSink)->attachBuffer(buf, bufSize);
            }
            else
            {
                free(buf);
            }
        }
    }
    auto old = gPrintSink;
    gPrintSink = newSink;
    return old;
}

template <int InitialBufSize=64, typename... Args>
size_t ftprintf(uint8_t fd, const char* fmtStr, Args... args)
{
    extern IPrintSink* gPrintSink;
    size_t bufsize = InitialBufSize;
    bool isAsync = gPrintSink->waitReady();
    char* staticBuf; // static buf
    char* buf;
    if (isAsync)
    {
        staticBuf = nullptr;
        buf = (char*)malloc(bufsize);
    }
    else
    {
        buf = staticBuf = (char*)alloca(bufsize);
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
        bufsize += isAsync ? STM32PP_TPRINTF_ASYNC_EXPAND_STEP : STM32PP_TPRINTF_SYNC_EXPAND_STEP;
        if (bufsize > STM32PP_TPRINTF_MAX_DYNAMIC_BUFSIZE)
        {
            //too much, bail out
            if ((buf != staticBuf) && !isAsync) // buffer is dynamic and synchronous, free it
            {
                free(buf);
            }
            return 0;
        }
        buf = (buf == staticBuf)
            ? (char*)malloc(bufsize)
            : (char*)realloc(buf, bufsize);
        if (!buf)
        {
            // If we are async, we did a realloc. When realloc fails,
            // it doesn't free the old buffer, so the sink's pointer remains valid
            return 0;
        }
    }
    size_t size = ret-buf;
    gPrintSink->print(buf, size, fd);
    if ((buf != staticBuf) && !isAsync)
    {
        free(buf);
    }
    return size;
}

template <int InitialBufSize=64, typename ...Args>
uint16_t tprintf(const char* fmtStr, Args... args)
{
    return ftprintf<InitialBufSize>(1, fmtStr, args...);
}

static inline void puts(const char* str, uint16_t len)
{
    extern IPrintSink* gPrintSink;
    gPrintSink->print(str, len, 1);
}

#endif
