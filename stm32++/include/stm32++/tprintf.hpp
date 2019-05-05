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
    struct BufferInfo
    {
        char* buf = nullptr;
        size_t bufSize = 0;
        void clear()
        {
            buf = nullptr;
            bufSize = 0;
        }
    };
    /**
     * @brief waitReady Waits till the sink has completed the last print operation, if any
     * @return Pointer to the sink's buffer info, if the sink is async.
     * Null if the sink is synchronous
     */
    virtual BufferInfo* waitReady() const { return nullptr; }
    virtual void print(const char* str, size_t len, int fd=1) = 0;
};

struct AsyncPrintSink: public IPrintSink
{
protected:
    BufferInfo mPrintBuffer;
};

static inline IPrintSink* setPrintSink(IPrintSink* newSink)
{
    extern IPrintSink* gPrintSink;
    IPrintSink::BufferInfo* currSinkBufInfo = gPrintSink->waitReady();
    bool isAsync = (currSinkBufInfo != nullptr);
    if (isAsync)
    {
        if (currSinkBufInfo->buf)
        {
            auto newSinkBufInfo = newSink->waitReady();
            if (newSinkBufInfo) // newSink is async, move current async buffer to it
            {
                if (newSinkBufInfo->buf)
                { // newSink also has a buffer allocated, free it
                    free(newSinkBufInfo->buf);
                }
                *newSinkBufInfo = *currSinkBufInfo;
                currSinkBufInfo->clear();
            }
            else // newSink is synchronous, and we have an async buffer, free it
            {
                free(currSinkBufInfo);
                currSinkBufInfo->clear();
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
    char* staticBuf; // static buf
    char* buf;
    size_t bufsize;

    auto async = gPrintSink->waitReady();
    if (async)
    {
        staticBuf = nullptr;
        if (async->buf)
        {
            buf = async->buf;
            bufsize = async->bufSize;
        }
        else
        {
            buf = (char*)malloc(InitialBufSize);
            bufsize = InitialBufSize;
        }
    }
    else
    {
        buf = staticBuf = (char*)alloca(InitialBufSize);
        bufsize = InitialBufSize;
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
        bufsize += async ? STM32PP_TPRINTF_ASYNC_EXPAND_STEP : STM32PP_TPRINTF_SYNC_EXPAND_STEP;
        if (bufsize > STM32PP_TPRINTF_MAX_DYNAMIC_BUFSIZE)
        {
            //too much, bail out
            if ((buf != staticBuf) && !async) // buffer is dynamic and synchronous, free it
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
    if ((buf != staticBuf) && !async)
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
