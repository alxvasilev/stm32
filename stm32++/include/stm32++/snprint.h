/**
 * @author Alexander Vassilev
 * @copyright BSD License
 */

#ifndef _SNPRINT_H
#define _SNPRINT_H

#include "tostring.h"
#include <assert.h>
#include <stdlib.h>

typedef void(*PrintSinkFunc)(const char* str, uint32_t len, int fd, void* userp);

void setPrintSink(PrintSinkFunc func, void* arg);
PrintSinkFunc printSink();
void* printSinkUserp();

void semihostingPrintSink(const char* str, uint32_t len, int fd=1);
char* tsnprintf(char* buf, uint32_t bufsize, const char* fmtStr);

template <typename Val, typename ...Args>
char* tsnprintf(char* buf, uint32_t bufsize, const char* fmtStr, Val val, Args... args)
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
            buf = toString(buf, bufend-buf+1, val);
            return tsnprintf(buf, bufend-buf+1, fmtStr+1, args...);
        }
        *(buf++) = *(fmtStr++);
    }
    while (buf < bufend);
    return nullptr;
}

template <class T>
struct AutoFree
{
    operator T*() const { return mPtr; }
    operator bool() const { return mPtr != nullptr; }
    AutoFree(T* ptr): mPtr(ptr){}
    ~AutoFree() { if (mPtr) free(mPtr); }
    void reassign(T* ptr) { mPtr = ptr; }
    T* get() const { return mPtr; }
protected:
    T* mPtr;
};

template <uint32_t BufSize=64, typename ...Args>
uint32_t ftprintf(int fd, const char* fmtStr, Args... args)
{
    extern PrintSinkFunc gPrintSinkFunc;
    extern void* gPrintSinkUserp;

    uint32_t bufsize = BufSize;
    AutoFree<char> buf = (char*)malloc(BufSize);
    char* ret;
    for(;;)
    {
        ret = tsnprintf(buf, bufsize, fmtStr, args...);
        if (ret)
            break;

        bufsize *= 2;
        if (bufsize > 10240)
            return 0;
        buf.reassign((char*)realloc(buf, bufsize));
        if (!buf)
            return 0;
    }
    uint32_t size = ret-buf.get();
    gPrintSinkFunc(buf, size, fd, gPrintSinkUserp);
    return size;
}

template <uint32_t BufSize=64, typename ...Args>
uint32_t tprintf(const char* fmtStr, Args... args)
{
    return ftprintf<BufSize>(1, fmtStr, args...);
}

static inline void puts(const char* str, uint32_t len)
{
    extern PrintSinkFunc gPrintSinkFunc;
    extern void* gPrintSinkUserp;
    gPrintSinkFunc(str, len, 1, gPrintSinkUserp);
}

#endif
