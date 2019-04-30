#if !defined(SEMIHOSTING_HPP) && !defined(NOT_EMBEDDED)
#define SEMIHOSTING_HPP

#include <stdint.h>
#include <stddef.h> // for size_t

namespace shost
{
/** @brief Semihosting opcodes */
enum: uint8_t {
    SYS_WRITE = 0x05,
    SYS_READ = 0x06,
    SYS_READC = 0x07,
    SYS_TIME = 0x11
};

size_t bkpt(size_t cmd, size_t arg1);

void fputs(const char* str, size_t len, int fd);
void write(const void* buf, size_t bufsize, int fd=1);

/** @brief Reads a char from semihosting stdin and returns it. If the stdin
 * is not connected, -1 is returned (EOF).
 */
static inline int getchar() { return bkpt(SYS_READC, 0); }

/** @brief Read data from semihosting stdin.
 * @return Number of bytes read
 */
size_t read(void* buf, size_t bufsize, int fd=0);

/** @brief Returns the unix time from the host system */
static inline uint32_t time() { return bkpt(SYS_TIME, 0); }

};

#endif
