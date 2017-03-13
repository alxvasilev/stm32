#include <stm32++/snprint.h>

#ifndef NOT_EMBEDDED
    #include <stm32++/semihosting.hpp>
    PrintSinkFunc gPrintSinkFunc = shost::fputs;
#else
    #include <unistd.h>
    void standardPuts(int fd, const char* str, size_t len, void* userp)
    {
        write(fd, str, len);
    }
    PrintSinkFunc gPrintSinkFunc = standardPuts;
#endif

void* gPrintSinkUserp = nullptr;
uint8_t gPrintSinkFlags = 0;

void setPrintSink(PrintSinkFunc func, void* userp, uint8_t flags)
{
    gPrintSinkFunc = func;
    gPrintSinkUserp = userp;
    gPrintSinkFlags = flags;
}

char* tsnprintf(char* buf, size_t bufsize, const char* fmtStr)
{
    if (!buf)
        return nullptr;

    char* bufend = buf+bufsize-1;
    while (*fmtStr)
    {
        if (buf >= bufend)
        {
            assert(buf == bufend);
            *buf = 0;
            return nullptr;
        }
        *(buf++) = *(fmtStr++);
    }
    *buf = 0;
    return buf;
}
