/**
 * @author Alexander Vassilev
 * @copyright BSD license
 */
#ifndef _TIME_UTILS_H
#define _TIME_UTILS_H

#include <libopencm3/cm3/dwt.h>
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/stm32/rcc.h>
#include <type_traits>
class DwtCounter
{
public:
    typedef uint32_t Word;
    static volatile uint32_t get() { return (volatile uint32_t)DWT_CYCCNT; }
    static volatile uint32_t ticks() { return (volatile uint32_t)DWT_CYCCNT; }

    template <class W=int64_t>
    static W ticksToNs(W ticks)
    { return (ticks * 1000) / (rcc_ahb_frequency/1000000); }

    template <class W=uint32_t>
    static W ticksToUs(W ticks)
    { return ticks / (rcc_ahb_frequency/1000000); }

    template <class W=uint32_t>
    static W ticksToMs(W ticks)
    { return ticks / (rcc_ahb_frequency/1000); }

    template <class W=uint32_t>
    static W ticksTo100Ms(W ticks)
    { return ticks / (rcc_ahb_frequency/10); }

    template <class W=uint32_t>
    static W ticksTo10Ms(W ticks)
    { return ticks / (rcc_ahb_frequency/100); }

    template <uint32_t Div, int32_t Corr>
    static volatile void delay(uint32_t t)
    {
#ifndef NDEBUG
        enum { kCycleOverhead = 160 + Corr}; //in debug build, the func call overhead is quite high
#else
        enum { kCycleOverhead = 16 + Corr};
#endif
        uint32_t now = get();
        uint32_t ticks = t * (rcc_ahb_frequency/1000) / Div;
        if (ticks > kCycleOverhead)
            ticks -= kCycleOverhead;
        else
            ticks = 0;
        register uint32_t tsEnd = now + ticks;
        if (now > tsEnd) //will wrap
        {
            while(get() > tsEnd);
        }
        while(get() < tsEnd);
    }
#ifndef NO_DWT_CTR_START
protected:
    struct Initializer
    {
        Initializer() { dwt_enable_cycle_counter(); }
    };
    static Initializer mInitializer;
#endif
};

// Should never be actually instantiated with negative values,
// but satisfies compiler checks.
template <int32_t Count>
typename std::enable_if<(Count <= 0), void>::type nop()
{
}

template <int32_t Count>
typename std::enable_if<(Count == 1), void>::type nop()
{
    asm volatile("nop;");
}

// WARNING: for Count >=30, there is huge random overhead, probably result of
// cache pre-fetch, as the nop sequence does not fit in the  cache anymore
template <int32_t Count>
typename std::enable_if<(Count > 1), void>::type nop()
{
    asm volatile("nop;");
    nop<Count-1>();
}
/*
template <uint32_t Ns, uint32_t Freq>
typename std::enable_if<(Ns <= 100), void>::type nsDelay()
{
    nop<(Ns * (Freq / 1000)) / 1000000>();
}
*/
template <uint32_t Ns, uint32_t Freq>
void nsDelay()
{

//Loop takes 5 cycles if clock is 72 MHz, and 3 cycles if clock is 24 MHz
    enum: uint32_t
    {
        kOverhead = 2,
        kTicks = (Ns * (Freq / 1000)) / 1000000,
        kTicksPerLoop = (Freq > 24000000) ? 6 : 3
    };

    if (kTicks <= kOverhead)
        return;

    nop<(kTicks-kOverhead) % kTicksPerLoop>();
    enum: uint32_t { kLoops = (kTicks-kOverhead) / kTicksPerLoop };
    if (kLoops == 0)
        return;
    register uint32_t loops = kLoops;
    asm volatile("0:" "SUBS %[count], 1;" "BNE 0b;" :[count]"+r"(loops));
}

template<int32_t TickCorr=0>
void nsDelay(uint32_t ns) { DwtCounter::delay<1000000, TickCorr>(ns); }
template <int32_t TickCorr=0>
void usDelay(uint32_t us) { DwtCounter::delay<1000, TickCorr>(us); }
template <int32_t TickCorr=0>
static inline void msDelay(uint32_t ms) { DwtCounter::delay<1, TickCorr>(ms); }

/* 64-bit timestamp clock. Uses T as the actual time source, and implements
 * wrapping protection
 * @param Int - whether to implement concurrency guard if used in interrupts.
 * Necessary because we have internal state that may be updated in the get()
 * method
*/
template <bool Int, class T>
class TimeClockNoWrap;

template <class T>
class TimeClockNoWrap<false, T>: public T
{
    typename T::Word mHighWord = 0;
    typename T::Word mLastCount;
protected:
    using T::get; // hide the get() method if the original source
public:
    TimeClockNoWrap(): mLastCount(T::get()){}
    int64_t nanotime() { return T::ticksToNs(this->ticks()); }
    int64_t microtime() { return T::ticksToUs(this->ticks()); }
    int64_t millitime() { return T::ticksToMs(this->ticks()); }
    int64_t ticks()
    {
        typename T::Word now = T::get();
        if (now < mLastCount)
            mHighWord++;
        mLastCount = now;
        return ((int64_t)mHighWord << (sizeof(typename T::Word)*8)) | now;
    }
};

template <class T>
class TimeClockNoWrap<true, T>: public TimeClockNoWrap<false, T>
{
    typedef TimeClockNoWrap<false, T> Base;
public:
    int64_t ticks()
    {
        bool disabled = cm_is_masked_interrupts();
        if (disabled)
        {
            return Base::ticks();
        }

        cm_disable_interrupts();
        int64_t result = Base::ticks();
        cm_enable_interrupts();
        return result;
    }
};

/* Class to measure elapsed time
 * T is the timestamp source
 */
template <class T = TimeClockNoWrap<false, DwtCounter> >
class GenericElapsedTimer: public T
{
protected:
    volatile int64_t mStart;
public:
    GenericElapsedTimer(): mStart(T::ticks()){}
    volatile void reset() { mStart = T::ticks(); }
    int64_t tsStart() const { return mStart; }
    volatile int64_t ticksElapsed()
    { //compensate for our own one cycle overhead
        return T::ticks() - mStart - 18;
    }
    volatile int64_t nsElapsed() { return T::ticksToNs(ticksElapsed()); }
    volatile int64_t usElapsed() { return T::ticksToUs(ticksElapsed()); }
    volatile int64_t msElapsed() { return T::ticksToMs(ticksElapsed()); }
};
typedef GenericElapsedTimer<> ElapsedTimer;

#endif
