#ifndef DMA_PRINT_HPP
#define DMA_PRINT_HPP

#include "printSink.hpp"
namespace dma
{
template <class DmaDevice>
class PrintSink: public DmaDevice, public AsyncPrintSink
{
    virtual IPrintSink::BufferInfo* waitReady()
    {
        while(DmaDevice::txBusy());
        return &mPrintBuffer;
    }
    virtual void print(const char *str, size_t len, int bufSize)
    {
        assert(!DmaDevice::txBusy());
        mPrintBuffer.buf = str;
        mPrintBuffer.bufSize = bufSize;
        DmaDevice::dmaTxStart((const void*)str, len);
    }
};
}

#endif
