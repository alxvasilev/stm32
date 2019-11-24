#ifndef STM32PP_XASSERT_HPP
#define STM32PP_XASSERT_HPP
#include "tprintf.hpp"

#ifndef NDEBUG
    #undef xassert
    #define xassert(expr, ...) (expr) ? (void)0 : __xassert_fail(#expr, __FILE__, __LINE__, ##__VA_ARGS__)
    static inline void __xassert_fail(const char* expr, const char* file, int line, const char* msg=nullptr)
    {
        tprintf("========\nAssertion failed: ");
        if (msg) {
            tprintf("% (%)\n", msg, expr);
        } else {
            tprintf("assert(%)\n", expr);
        }
        tprintf("at %:%\n========\n", file, line);
#ifndef STM32PP_NOT_EMBEDDED
        for(;;) asm ("wfi");
#endif
    }
//    #define dbg(fmtStr,...) tprintf(fmtStr, ##__VA_ARGS__)
#else
//    #define dbg(fmtStr,...)
      #define xassert(...)
#endif

#endif // XASSERT_HPP
