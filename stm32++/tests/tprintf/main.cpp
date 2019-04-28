#include <stm32++/tprintf.hpp>
/*
 * template <typename V, typename... Args>
void checkOutput(const char* fmtString, const char* expected, Args... args)
{
    char buf[128];
    tsnprintf(
    */
int main()
{
    tprintf("this is a float: %\n"
            "this is a fmtFp<prec: 6>(minDigits: 4): %\n"
            "this is a hex8(127): %\n"
            "this is a hex16(32767): %\n"
            "this is a bin(127): %\n"
            "this is a string: '%'\n"
            "this is a dollar: %\n",
            123.4567, fmtFp<6>(123.4567, 4),
            fmtHex8(127), fmtHex16(32767),
            fmtBin(127), "test message", '$');
}
