#include <stm32++/snprint.h>

#ifndef NOT_EMBEDDED
    void semihostingPuts(const char* str, uint16_t len, uint8_t fd, void* userp);
    PrintSinkFunc gPrintSinkFunc = semihostingPuts;
#else
    #include <unistd.h>
    void standardPuts(const char* str, uint16_t len, uint8_t fd, void* userp)
    {
        write(fd, str, len);
    }
    PrintSinkFunc gPrintSinkFunc = standardPuts;
#endif

void* gPrintSinkUserp = nullptr;

void setPrintSink(PrintSinkFunc func, void* arg)
{
    gPrintSinkFunc = func;
    gPrintSinkUserp = arg;
}

char* tsnprintf(char* buf, uint32_t bufsize, const char* fmtStr)
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

#ifndef NOT_EMBEDDED
void semihostingPuts(const char* str, uint16_t len, uint8_t fd, void* userp)
{
    struct Msg
    {
        uint32_t fd;
        const char* data;
        uint32_t size;
        Msg(uint32_t aFd, const char* aData, uint32_t aSize)
        : fd(aFd), data(aData), size(aSize){}
    };
    Msg msg(1, str, len);
    asm volatile(
       "mov r0, #0x05;"
       "mov r1, %[msg];"
       "bkpt #0xAB;"
       : : [msg] "r" (&msg)
       : "r0", "r1", "memory");
}
#endif
