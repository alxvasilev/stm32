/**
 * @author Alexander Vassilev
 * @copyright BSD License
 */

#ifndef _SNPRINT_H
#define _SNPRINT_H

#include "tostring.hpp"
#include <assert.h>
#include <stdlib.h>
#include <alloca.h>
#ifndef NOT_EMBEDDED
    #include "utils.hpp" //for interrupt safe stuff
    void semihostingPrintSink(const char* str, size_t len, int fd=1);
#else
    void standardPrintSink(const char* str, size_t len, int fd=1);
#endif

typedef void(*PrintSinkFunc)(const char* str, size_t len, int fd, void* userp);
enum: uint8_t { kPrintSinkLeaveBuffer = 1 };

void setPrintSink(PrintSinkFunc func, void* userp=nullptr, uint8_t flags=0);
PrintSinkFunc printSink();
void* printSinkUserp();

/** @brief This is an interrupt-safe wrapper around \c free().
 * It is used by this lib, and should be used to free the buffer passed to
 * the print sink in case the kPrintSinkLeaveBuffer flag is set.
 */
static inline void tprintf_free(void* ptr)
{
#ifndef NOT_EMBEDDED
    IntrDisable id;
#endif
    free(ptr);
}

char* tsnprintf(char* buf, size_t bufsize, const char* fmtStr);

template <typename Val, typename ...Args>
char* tsnprintf(char* buf, size_t bufsize, const char* fmtStr, Val val, Args... args)
{
    if (!buf)
        return nullptr;
    assert(bufsize);

    char* bufend = buf+bufsize-1; //point to last char
    do
    {
        char ch = *fmtStr;
        if (ch == 0)
        {
            *buf = 0;
            return buf;
        }
        if (ch == '%')
        {
            buf = toString<kDontNullTerminate>(buf, bufend-buf+1, val);
            return tsnprintf(buf, bufend-buf+1, fmtStr+1, args...);
        }
        *(buf++) = *(fmtStr++);
    }
    while (buf < bufend);
    return nullptr;
}

template <size_t BufSize, typename... Args>
typename std::enable_if<BufSize < 0, size_t>::type
ftprintf(uint8_t fd, const char* fmtStr, Args... args)
{
    extern PrintSinkFunc gPrintSinkFunc;
    extern void* gPrintSinkUserp;
    char sbuf[BufSize & 0x7fffffff];
    char* ret = tsnprintf(sbuf, BufSize, fmtStr, args...);
    size_t size = ret-sbuf;
    gPrintSinkFunc(sbuf, size, fd, gPrintSinkUserp);
    return size;
}

extern PrintSinkFunc gPrintSinkFunc;
extern void* gPrintSinkUserp;
extern uint8_t gPrintSinkFlags;

template <size_t BufSize=64, typename... Args>
typename std::enable_if<BufSize >= 0, size_t>::type
ftprintf(uint8_t fd, const char* fmtStr, Args... args)
{
    size_t bufsize = BufSize;

    char* sbuf;
    char* buf;
    if (gPrintSinkFlags & kPrintSinkLeaveBuffer)
    {
        sbuf = nullptr;
        buf = (char*)malloc(BufSize);
    }
    else
    {
        buf = sbuf = (char*)alloca(BufSize);
    }
    char* ret;
    for(;;)
    {
        ret = tsnprintf(buf, bufsize, fmtStr, args...);
        if (ret)
            break;

        //have to increase buf size
        bufsize *= 2;
        if (bufsize > 10240)
        {
            //too much, bail out
            if (buf != sbuf)
                tprintf_free(buf);
            return 0;
        }
        buf = (buf == sbuf)
            ? (char*)malloc(bufsize)
            : (char*)realloc(buf, bufsize);
        if (!buf)
            return 0;
    }
    size_t size = ret-buf;
    gPrintSinkFunc(buf, size, fd, gPrintSinkUserp);
    if ((buf != sbuf) && ((gPrintSinkFlags & kPrintSinkLeaveBuffer) == 0))
        tprintf_free(buf);
    return size;
}

template <size_t BufSize=64, typename ...Args>
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
