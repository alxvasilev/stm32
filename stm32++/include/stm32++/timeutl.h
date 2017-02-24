#include <libopencm3/cm3/dwt.h>
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/stm32/rcc.h>

class DwtCounter
{
public:
    typedef uint32_t Word;
    static volatile uint32_t get() { return DWT_CYCCNT; }
    template <class W=uint32_t>
    static W ticksToNs(W ticks)
    { return ((uint64_t)ticks * 1000000000) / rcc_ahb_frequency; }
    template <class W=uint32_t>
    static W ticksToUs(W ticks)
    { return ((uint64_t)ticks * 1000000) / rcc_ahb_frequency; }
    template <class W=uint32_t>
    static W ticksToMs(W ticks)
    { return ((uint64_t)ticks * 1000) / rcc_ahb_frequency; }
    template <uint32_t Div>
    static void delay(uint32_t t)
    {
#ifndef NDEBUG
        enum { kCycleOverhead = 160 }; //in debug build, the func call overhead is quite high
#else
        enum { kCycleOverhead = 9 };
#endif
        uint32_t now = get();
        uint64_t ticks = (((uint64_t)(t+1)) * rcc_ahb_frequency) / Div;
        if (ticks >= kCycleOverhead)
            ticks -= kCycleOverhead;
        register uint32_t tsEnd = now + ticks;
        if (now > tsEnd) //will wrap
        {
            while(get() > tsEnd);
        }
        while(get() < tsEnd);
    }
};

static inline void nsDelay(uint32_t ns) { DwtCounter::delay<1000000000>(ns); }
static inline void usDelay(uint32_t us) { DwtCounter::delay<1000000>(us); }
static inline void msDelay(uint32_t ms) { DwtCounter::delay<1000>(ms); }

template <class T = DwtCounter>
class ElapsedTimer: public T
{
protected:
    typename T::Word mStart;
public:
    ElapsedTimer(): mStart(T::get()){}
    void reset() { mStart = T::get(); }
    uint32_t tsStart() const { return mStart; }
    uint32_t ticksElapsed() const { return T::get() - mStart; }
    uint32_t nsElapsed() { return T::ticksToNs(ticksElapsed()); }
    uint32_t usElapsed() { return T::ticksToUs(ticksElapsed()); }
    uint32_t msElapsed() { return T::ticksToMs(ticksElapsed()); }
};

template <bool Int, class T>
class TimeClockImpl;

template <bool Int=false, class T=DwtCounter>
class TimeClock: public TimeClockImpl<Int, T>
{
protected:
    typedef TimeClockImpl<Int, T> Base;
    using T::get;
public:
    uint64_t nanotime() { return T::ticksToNs(this->ticks()); }
    uint64_t microtime() { return T::ticksToUs(this->ticks()); }
    uint64_t millitime() { return T::ticksToMs(this->ticks()); }
};

template <class T>
class TimeClockImpl<false, T>: public T
{
    typename T::Word mHighWord = 0;
    typename T::Word mLastCount;
protected:
    TimeClockImpl(): mLastCount(T::get()){}
public:
    uint64_t ticks()
    {
        typename T::Word now = T::get();
        if (now < mLastCount)
            mHighWord++;
        mLastCount = now;
        return ((uint64_t)mHighWord << (sizeof(typename T::Word)*8)) | now;
    }
};

template <class T>
class TimeClockImpl<true, T>: public TimeClockImpl<false, T>
{
    typedef TimeClockImpl<false, T> Base;
public:
    uint64_t ticks()
    {
        bool disabled = cm_is_masked_interrupts();
        if (disabled)
        {
            return Base::ticks();
        }

        cm_disable_interrupts();
        uint64_t result = Base::ticks();
        cm_enable_interrupts();
        return result;
    }
};
