#include <stm32++/printSink.hpp>
#ifdef STM32PP_LOG_VIA_SEMIHOSTING
    #include <stm32++/semihosting.hpp>
#else
    #include <unistd.h>
#endif

struct DefaultPrintSink: public IPrintSink
{
    IPrintSink::BufferInfo* waitReady() { return nullptr; }
    void print(const char* str, size_t len, int fd)
    {
#ifdef STM32PP_LOG_VIA_SEMIHOSTING
        shost::fputs(str, len, fd);
#else
        ::write(fd, str, len);
#endif
    }
};

DefaultPrintSink gDefaultPrintSink;
IPrintSink* gPrintSink = &gDefaultPrintSink;
