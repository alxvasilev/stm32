#include <stdio.h>
#include <tostring.hpp>
#include <unistd.h>
#include <string.h>

enum { kConsoleWidth = 80, kStatusWidth = 12 };

int gFails = 0;
const char* COLOR_GREEN = isatty(1) ? "\e[1;32m" : "";
const char* COLOR_RED = isatty(1) ? "\e[1;31m" : "";
const char* COLOR_NORMAL = isatty(1) ? "\e[0m" : "";
const char* spaces = "                                                                                                      ";
const char* border = "======================================================================================================";
int summary()
{
    auto borderLen = (kConsoleWidth - kStatusWidth - 17 + 2) / 2;
    if (gFails) {
        printf("%.*s %2d tests %sFAILED%s %.*s\n", borderLen, border, gFails, COLOR_RED, COLOR_NORMAL, borderLen, border);
        return 1;
    }
    else {
        printf("%.*s All test %sPASSED%s %.*s\n", borderLen, border, COLOR_GREEN, COLOR_NORMAL, borderLen, border);
        return 0;
    }
}

template <int flags=0, typename... Args>
void verify(const char* testName, const char* expected, Args... args)
{
    int lenExpected = strlen(expected);
    int lenOutBuf = std::max(lenExpected, 128) + 1;
    char* buf = (char*)alloca(lenOutBuf);
    int lineLen = lenExpected + strlen(testName) + 4;
    int numSpaces = kConsoleWidth - kStatusWidth - lineLen;
    if (numSpaces < 0) {
        numSpaces = 0;
    }
    printf("%s:%.*s\"%s\" ", testName, numSpaces, spaces, expected);
    auto result = toString<flags>(buf, lenOutBuf, args...);
    if (!result) {
        printf("%sNULL-return%s\n", COLOR_RED, COLOR_NORMAL);
        gFails++;
        return;
    }
    if (strcmp(buf, expected)) {
        printf("%sMISMATCH%s\n", COLOR_RED, COLOR_NORMAL);
        printf("%.*sinstead got: \"%s\"\n", (int)strlen(testName) + numSpaces + lenExpected - (int)strlen(buf)- 12, spaces, buf);
        gFails++;
    } else {
        printf("%sOK%s\n", COLOR_GREEN, COLOR_NORMAL);
    }
}

int main()
{
    verify("simple string", "Test message", "Test message");
    verify("char", "x", 'x');
    verify("repeat char", "xxxxxxxxxx", rptChar('x', 10));
    verify("repeat char once", "x", rptChar('x', 1));
    verify("repeat char zero times", "", rptChar('x', 0));
    verify("decimal+", "12345678", 12345678);
    verify("decimal-", "-567890", -567890);
    verify("bigDecimal+", "1234567890123456789", 1234567890123456789LL);
    verify("bigDecimal-", "-1234567890123456789", -1234567890123456789LL);

    verify("hex(+)", "12abcde", fmtHex(0x12abcde));
    verify("hex(+) w/prefix", "0x12abcde", fmtHex<kNumPrefix>(0x12abcde));
    verify("hex16(-1)", "ffff", fmtHex16(-1));
    verify("hex32(-1)", "ffffffff", fmtHex32(-1));
    verify("hex32(+)", "deadbeef", fmtHex32(0xdeadbeef));

    verify("bigHex(+)", "0xdeadbeefcafebabe", fmtHex<kNumPrefix>(0xdeadbeefcafebabeLL));
    verify("bigHex(-)", "-0x7eadbeefcafebabe", fmtHex<kNumPrefix>(-0x7eadbeefcafebabeLL));

    verify("bin(-1)", "-1", fmtBin(-1));
    verify("bin(-1) w/prefix", "-0b1", fmtBin<kNumPrefix>(-1));
    verify("bin8(-1)", "11111111", fmtBin8(-1));
    verify("bin16(-1)", "1111111111111111", fmtBin16(-1));
    verify("bin32(-1)", "11111111111111111111111111111111", fmtBin32(-1));
    verify("bin32(-1) w/prefix", "0b11111111111111111111111111111111", fmtBin<kNumPrefix>((uint32_t)-1));
    verify("bin32(+)", "10101110111101111010000001011001", fmtBin(0b10101110111101111010000001011001));
    verify("bin32(+) w/prefix", "0b10101110111101111010000001011001", fmtBin<kNumPrefix>(0b10101110111101111010000001011001));

    verify("float single dec", "44.9", fmtFp<1>(44.9f));
    verify("float round-up", "44.1", fmtFp<1>(44.09f));
    verify("float round-up", "45.0", fmtFp<1>(44.95f));
    verify("float1 round-down .4", "44.4", fmtFp<1>(44.44));
    verify("float6 round-down .4", "44.112233", fmtFp<6>(44.1122334));

    verify("float1 round-up 44.45", "44.5", fmtFp<1>(44.45));
    verify("float6 round-up 44.1122335", "44.112234", fmtFp<6>(44.1122335));

    // 4.1122345 is represented as 4.112234499999999, which causes down-rounding on .5
    // This happens also with printf("%.6f") with this value
    verify("float6 round-up 4.1122345", "4.112234", fmtFp<6>(4.1122345));
    // Doesn't occur on 1.112235
    verify("float6 round-up 1.1122345", "1.112235", fmtFp<6>(1.1122345));
    //====
    verify("float round-down", "44.9", fmtFp<1>(44.94f));
    verify("float round-up", "-45.0", fmtFp<1>(-44.95f));
    verify("float round-down", "-44.9", fmtFp<1>(-44.94f));
    return summary();
}
