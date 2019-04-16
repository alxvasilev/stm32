#ifndef STM32PP_XASSERT_HPP
#define STM32PP_XASSERT_HPP
#include "snprint.hpp"

#ifndef NDEBUG
    #undef xassert
    #define xassert(expr, ...) (expr) ? (void)0 : __xassert_fail(#expr, __FILE__, __LINE__, #__VA_ARGS__)
    static inline void __xassert_fail(const char* expr, const char* file, int line, const char* msg=nullptr)
    {
        tprintf("========\nAssertion failed:\n");
        if (msg) {
            tprintf("% (%)\n", msg, expr);
        } else {
            tprintf("%\n", expr);
        }
        tprintf("at %:%\n========\n", expr, file, line);
        for(;;) asm ("wfi");
    }
//    #define dbg(fmtStr,...) tprintf(fmtStr, ##__VA_ARGS__)
#else
//    #define dbg(fmtStr,...)
      #define xassert(...)
#endif

#endif // XASSERT_HPP
