/** @author Alexander Vassilev
 * @copyright BSD License
 */

#ifndef BUTTON_HPP_INCLUDED
#define BUTTON_HPP_INCLUDED

#ifndef STM32PP_NOT_EMBEDDED
  #include <libopencm3/stm32/rcc.h>
  #include <libopencm3/stm32/gpio.h>
  #include <libopencm3/stm32/exti.h>
  #include <libopencm3/cm3/nvic.h>
  #include <libopencm3/cm3/systick.h>

  #include <stm32++/timeutl.hpp>
#endif

#include <stm32++/utils.hpp>
#include <stm32++/tprintf.hpp>

namespace btn
{
/** @brief Button event type */
enum: uint8_t
{
    kEventRelease=0, //< Button up
    kEventPress=1,   //< Button down
    kEventHold = 2,  //< Button held
    kEventRepeat = 3 //< Button repeat, generated when the button is held pressed
};
/** @brief Option flags. They apply to all pins that the Button
 *  class manages (the APins mask)
 */
enum: uint8_t
{
/** The button is pressed when the pin is low. If internal pull
 * is enabled, this flag determines which one of the pull modes
 * (pull-up or pull-down) is set on the button pins.
 */
    kOptActiveLow = 1,
/** Don't activate internal pull-up/pull-down for button pins */
    kOptNoInternalPuPd = 2
};

/** @brief Button handler callback.
 * Called when a button event occurs
 * @param btn - The bit position of the button pin within the GPIO port,
 * i.e. 0 for GPIO0, 1 for GPIO1 etc
 * @param event - The type of event that occurred for the button
 * @param arg - The user pointer that was passed to the Buttons instance
 * at construction or set via \c setUserp()
 */
typedef void(*EventCb)(uint16_t btn, uint8_t event, void* userp);

class HwDriver;

/** @brief Button handler class
 * There is one methods that has to be called periodically on the
 * Buttons object - \c poll(). It polls the state of the pins,
 * handles debounce delays and queues events. This method is suitable
 * for calling from an ISR that is executed periodically, i.e. the systick
 * interrupt or a timer. The queued events are processed when the \c process()
 * method is called. This is normally done in the \c main() loop.
 * @param APort The GPIO port of the button pins
 * @param APins Mask of the pins of the port that are connected to buttons
 * @param ARpt Mask of the button pins that have repeat enabled. Must be
 * a subset of APins
 * @param AFlags Option flags
 * @param APollIrqN The IRQ number of an interrupt that calls the poll()
 * function. This is needed to temporarily disable the interrupt that
 * does the polling, to prevent re-entrancy. If polling is done in main(),
 * this should be set to 127.
 * @param ADebounceDly Debounce interval in milliseconds. If the pin maintains the
 * same state within at least this period, then its state is considered
 * stable.
*/
template <uint32_t APort, uint16_t APins, uint16_t ARpt, uint8_t AFlags=0,
          uint8_t APollIrqN=127, uint8_t ADebounceDly=10, class Driver=HwDriver>
class Buttons
{
public:
    enum: uint32_t { Port = APort };
    enum: uint16_t { Pins = APins, RepeatPins = ARpt };
    enum: uint8_t  { Flags = AFlags, DebounceMs = ADebounceDly };
protected:
    //these are accessed only in the isr, via poll()
    volatile uint16_t mDebouncing = 0;
    volatile uint32_t mDebounceStartTs = 0;
    volatile uint16_t mLastPollState;
    //mState and mChanged are shared between the isr and the main thread
    volatile uint16_t mState;
    volatile uint16_t mChanged = 0;
    volatile uint32_t mLastObtainedTs;
    //===
    EventCb mHandler;
    void* mHandlerUserp;
    //repeat stuff
    enum { kRptCount = CountOnes<RepeatPins>::value,
           kRptShift = Right0Count<RepeatPins>::value
         };
    struct RepeatState
    {
        enum { kDelayRepeatInitialMs10 = 40 }; //x10 milliseconds
        uint32_t mLastTs;
        uint8_t mRptStartDelayMs10 = 100; // 1 second by default, can be configured per button
        uint8_t mTimeToNextMs10;
        uint8_t mRepeatCnt;
    };
    RepeatState mRptStates[kRptCount];
public:
    Buttons() {}
    void init(EventCb aCb, void* aUserp)
    {
        mLastObtainedTs = Driver::now();
        mHandler = aCb;
        mHandlerUserp = aUserp;
    //===
        static_assert((RepeatPins & ~Pins) == 0, "RepeatPins specifies pins that are not in Pins");
        if ((Flags & kOptNoInternalPuPd) == 0)
        {
            Driver::gpioSetPuPdInput(Port, Pins, Flags & kOptActiveLow);
        }
        else
        {
            Driver::gpioSetFloatInput(Port, Pins);
        }
        mState = Driver::gpioRead(Port);
        mLastPollState = mState;
    }
    /** @brief Polls the state of the button pins and queues events
     * for processing by process(). \c poll() is suitable for calling
     * from a periodic ISR
     */
    void poll()
    {
        // may be called from an ISR
        // scan pins and detect changes
        uint16_t newState = Driver::gpioRead(Port);
        uint16_t changedPins = (mLastPollState ^ newState) & Pins;
        mLastPollState = newState;

        if (changedPins)
        {
            mDebounceStartTs = Driver::now(); //reset debounce timer
            mDebouncing |= changedPins; //add pins to the ones currently debounced
        }
        //check for end of debounce period
        if (mDebouncing
        && (Driver::ticksToMs(Driver::now() - mDebounceStartTs) >= DebounceMs))
        {
            // debounce ended, read pins
            if (Flags & kOptActiveLow)
                newState = ~newState;
            // mask with mDebouncing rather than with Pins
            mChanged |= ((mState ^ newState) & mDebouncing);
            // update the state with only the debounced pins
            mState = (mState & ~mDebouncing) | (newState & mDebouncing);
            mDebouncing = 0;
        }
    }
    /** @brief Processes queued button events, by calling the user-supplied
     * event handler callback
     */
    void process()
    {
        uint32_t now = Driver::now();
        // atomically make a snapshot of the current button state and change flags
        bool intsWereEnabled;
        if (APollIrqN != 127)
        {
            intsWereEnabled = Driver::isIrqEnabled(APollIrqN);
            if (intsWereEnabled)
                Driver::disableIrq(APollIrqN);
        }
        uint16_t state = mState;
        uint16_t changed = mChanged;
        mChanged = 0;
        if ((APollIrqN != 127) && intsWereEnabled)
        {
            Driver::enableIrq(APollIrqN);
        }
        for (uint8_t idx=Right0Count<Pins>::value; idx < HighestBitIdx<Pins>::value; idx++)
        {
            uint16_t mask = 1 << idx;
            uint16_t pinState = state & mask;
            if (changed & mask)
            {
                uint8_t event;
                if (pinState) //just pressed
                {
                    event = kEventPress;
                    if (kRptCount && (pinState & RepeatPins))
                    {
                        // record timestamp for newly pressed repeatable buttons
                        auto& rptState = mRptStates[idx-kRptShift];
                        rptState.mLastTs = now;
                        rptState.mTimeToNextMs10 = rptState.mRptStartDelayMs10;
                        rptState.mRepeatCnt = 0;
                    }
                }
                else
                {
                    event = kEventRelease;
                }
                mHandler(mask, event, mHandlerUserp);
            }
            else // button state didn't change
            {
                if (!kRptCount) //repeat not enabled for any button
                    continue;
                if ((pinState & RepeatPins) == 0) //repeat not enabled for this button, or button not pressed
                    continue;

                auto& rptState = mRptStates[idx-kRptShift];
                // must be called at leat once per ~40 seconds in case of cpu frequency <= 100 MHz
                uint32_t ms10 = Driver::ms10ElapsedSince(rptState.mLastTs);
                if (ms10 < rptState.mTimeToNextMs10)
                {
                    continue; //too early for repeat
                }
                rptState.mLastTs = now;
                if (rptState.mTimeToNextMs10 == rptState.mRptStartDelayMs10)
                {
                    // we just completed the initial delay, switch to repeat
                    // by setting the repeat delay as the new target
                    rptState.mTimeToNextMs10 = RepeatState::kDelayRepeatInitialMs10;
                    rptState.mRepeatCnt = 0;
                    mHandler(mask, kEventHold, mHandlerUserp);
                }
                else // repeat mode
                {
                    // Calculate how much time we have spent at this repeat frequency
                    uint16_t dur = (++rptState.mRepeatCnt) * rptState.mTimeToNextMs10;
                    if (dur > 150)
                    {
                        if (rptState.mTimeToNextMs10 > 2)
                        {
                           rptState.mTimeToNextMs10 -= 1;
                           rptState.mRepeatCnt = 0;
                        }
                    }
                    else if (dur > 70)
                    {
                        if (rptState.mTimeToNextMs10 >= 10)
                        {
                           rptState.mTimeToNextMs10 >>= 1;
                           rptState.mRepeatCnt = 0;
                        }
                    }
                    mHandler(mask, kEventRepeat, mHandlerUserp);
                }
            }
        }
    }
    /** @brief Sets the user button event handler and its user pointer */
    void setHandler(EventCb h, void* userp)
    {
        mHandler = h;
        mHandlerUserp = userp;
    }
    void setHoldDelayFor(uint16_t pin, uint16_t timeMs)
    {
        assert(pin & ARpt);
        uint8_t idx = 0;
        for (; idx < 16; idx++)
        {
            if ((1 << idx) == pin)
                break;
        }
        idx -= kRptShift;
        auto& state = mRptStates[idx];
        state.mRptStartDelayMs10 = (timeMs + 5) / 10;
    }
    /** @brief Sets the user pointer that is passed to the event handler */
    void setHandlerUserp(void* userp) { mHandlerUserp = userp; }
};

#ifndef STM32PP_NOT_EMBEDDED
class HwDriver
{
protected:
    typedef HwDriver Self;
public:
    static uint32_t now()
    {
        return DwtCounter::get();
    }
    static uint32_t ms10ElapsedSince(uint32_t sinceTicks)
    {
        auto now = Self::now();
        if (now > sinceTicks)
        {
            return (now - sinceTicks) / (rcc_ahb_frequency / 100);
        }
        else // DwtCounter wrap
        {
            return (((uint64_t)now + 0xffffffff) - sinceTicks) / (rcc_ahb_frequency / 100);
        }
    }
    static uint32_t ticksToMs(uint32_t ticks) { return DwtCounter::ticksToMs(ticks); }
    static bool isIrqEnabled(uint8_t irqn) { return nvic_get_irq_enabled(irqn); }
    static void enableIrq(uint8_t irqn) { nvic_enable_irq(irqn); }
    static void disableIrq(uint8_t irqn) { nvic_disable_irq(irqn); }
    static void gpioSetPuPdInput(uint32_t port, uint16_t pins, int pullUp)
    {
        gpio_set_mode(port, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, pins);
        if (pullUp)
        {
            gpio_set(port, pins);
        }
        else
        {
            gpio_clear(port, pins);
        }
    }
    static void gpioSetFloatInput(uint32_t port, uint16_t pins)
    {
        gpio_set_mode(port, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, pins);
    }
    static uint16_t gpioRead(uint32_t port)
    {
        return GPIO_IDR(port);
    }
};
#endif
}

/*
Usage:
void handler(uint8_t button, uint8_t event, void* arg)
{
    gpio_toggle(GPIOC, GPIO13);
}

Buttons<GPIOA, GPIO0|GPIO1, GPIO0|GPIO1, kOptActiveLow, NVIC_SYSTICK_IRQ>
buttons(handler, nullptr);

extern "C" void sys_tick_handler()
{
    buttons.poll();
}

int main(void)
{
    rcc_clock_setup_in_hse_8mhz_out_72mhz();
    dwt_enable_cycle_counter();

    cm_enable_interrupts();
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
    GPIOC_ODR = 0;
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
    systick_set_reload(89999);

    systick_interrupt_enable();

    // Start counting.
    systick_counter_enable();
    for (;;)
    {
        buttons.process();
        msDelay(20);
    }
    return 0;
}
*/
#endif
