#ifndef STM32PP_XASSERT_HPP
#define STM32PP_XASSERT_HPP
#include "snprint.hpp"

#ifndef NDEBUG
    #undef xassert
    #define xassert(expr) (expr) ? (void)0 : __xassert_fail(#expr, __FILE__, __LINE__)
    static inline void __xassert_fail(const char* expr, const char* file, int line)
    {
        tprintf("========\nAssertion failed:\n%\nat %:%\n========\n", expr, file, line);
        for(;;);
    }
//    #define dbg(fmtStr,...) tprintf(fmtStr, ##__VA_ARGS__)
#else
//    #define dbg(fmtStr,...)
      #define xassert(...)
#endif

#endif // XASSERT_HPP
