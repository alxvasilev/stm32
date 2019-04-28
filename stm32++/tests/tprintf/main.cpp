#include <stm32++/tprintf.hpp>
#include <string.h>
#include <stdio.h>

struct MyPrintSink: public IPrintSink
{
    const char* expectedString = nullptr;
    void print(const char *str, size_t len, int fd)
    {
        if (strcmp(str, expectedString))
        {
            printf("ERROR: Output test mismatch: expected '%s', actual: '%s'\n", expectedString, str);
            exit(1);
        }
        else
        {
            printf("PASS: %s\n", str);
        }
    }
};

MyPrintSink myPrintSink;

template <typename... Args>
void expect(const char* expected, const char* fmtString, Args... args)
{
    auto savedSink = gPrintSink;
    gPrintSink = &myPrintSink;
    myPrintSink.expectedString = expected;
    tprintf(fmtString, args...);
    gPrintSink = savedSink;
}


int main()
{
    expect("this is a float: 123.456700", "this is a float: %", 123.4567);
    expect("this is a fmtFp<prec: 6>(minDigits: 4): 0123.456700",
           "this is a fmtFp<prec: 6>(minDigits: 4): %",
           fmtFp<6>(123.4567, 4));
    expect("this is an int: '  001234'", "this is an int: '%'", fmtInt(1234, 6, 8));
    expect("this is a hex8(127): 0x7f", "this is a hex8(127): %", fmtHex(127));
    expect("this is a hex16(32767): 0x7FFF", "this is a hex16(32767): %", fmtHex<kUpperCase>(32767));
    expect("this is a hex16(32767) no prefix: 7fff", "this is a hex16(32767) no prefix: %", fmtHex<kNoPrefix>(32767));

    expect("this is an octal: OCT4553207", "this is an octal: %", fmtInt<8>(1234567));
    expect("this is a bin(127): 0b01111111", "this is a bin(127): %", fmtBin(127));
    expect("this is a string: 'test message'", "this is a string: %", "'test message'");
    expect("this is a dollar: $", "this is a dollar: %", '$');

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
