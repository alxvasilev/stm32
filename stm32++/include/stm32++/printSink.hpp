/**
 * @author Alexander Vassilev
 * @copyright BSD License
 */

#ifndef _PRINT_SINK_H
#define _PRINT_SINK_H

#include <stddef.h>
#include <malloc.h>

struct IPrintSink
{
    struct BufferInfo
    {
        const char* buf = nullptr;
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
    virtual BufferInfo* waitReady() = 0;
    /**
     * @brief print Outputs the specified string
     * @param str The string to print
     * @param len The length of the string to print
     * @param info If this is a synchronous print sink, \c info is the file descriptor number,
     * for semihosting support. If this is an async print sink, \c info is the size of
     * the buffer, that contains the string (\c len may be less than the buffer size)
     */
    virtual void print(const char* str, size_t len, int info) = 0;
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
                    free((void*)newSinkBufInfo->buf);
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

#endif
