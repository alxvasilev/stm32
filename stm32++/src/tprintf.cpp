#include <stm32++/tprintf.hpp>
#ifndef STM32PP_NOT_EMBEDDED
    #include <stm32++/semihosting.hpp>
#else
    #include <unistd.h>
#endif

struct DefaultPrintSink: public IPrintSink
{
    void print(const char* str, size_t len, int fd)
    {
#ifndef STM32PP_NOT_EMBEDDED
        shost::fputs(str, len, fd);
#else
        ::write(fd, str, len);
#endif
    }
};

DefaultPrintSink gDefaultPrintSink;
IPrintSink* gPrintSink = &gDefaultPrintSink;
