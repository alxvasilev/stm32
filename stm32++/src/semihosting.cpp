//#include <stm32++/snprint.hpp>
#include <stm32++/semihosting.hpp>

#ifdef NOT_EMBEDDED
#error NOT_EMBEDDED is set:\n\
Semihosting can be used only when compiling for embedeed target
#endif

static_assert(sizeof(size_t) == sizeof(void*), "");

namespace shost
{

/** Single-argument wrapper for the BKPT instruction. Note that some commands
 * use a second argument in r2
 */
size_t bkpt(size_t cmd, size_t arg1)
{
    size_t ret;
    asm volatile(
       "mov r0, %[cmd];"
       "mov r1, %[arg1];"
       "bkpt #0xAB;"
       "mov %[ret], r0;"
       : [ret] "=r" (ret) //out
       : [cmd] "r" (cmd), [arg1] "r" (arg1) //in
       : "r0", "r1", "memory" //clobber
    );
    return ret;
}

void write(const void* buf, size_t bufsize, int fd)
{
    size_t msg[3] = { (size_t)fd, (size_t)buf, bufsize };
    bkpt(SYS_WRITE, (size_t)msg);
}

void fputs(const char* str, size_t len, int fd)
{
    write(str, len, fd);
}

size_t read(void* buf, size_t bufsize, int fd)
{
  size_t msg[3] = { (size_t)fd, (size_t)buf, bufsize };
  //SYS_READ returns the number of bytes remaining in the  buffer,
  //i.e. bufsize - actual_bytes_read
  size_t ret = bkpt(SYS_READ, (size_t)msg);
  return bufsize-ret;
}

}
