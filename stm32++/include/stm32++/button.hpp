#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#include <stm32++/snprint.h>
#include <stm32++/timeutl.h>
#include <stm32++/utils.hpp>

namespace button
{
template <uint32_t Port>
rcc_periph_clken getPortClock();

/** @brief Button event type */
enum: uint8_t
{
 kEventRelease=0, //< Button was released
 kEventPress=1, //< Button was pressed
 kEventRepeat=2 //< Button repeat, generated when the button is held pressed
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
typedef void(*EventCb)(uint8_t btn, uint8_t event, void* userp);

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
          uint8_t APollIrqN=127, uint8_t ADebounceDly=10>
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
    volatile uint16_t mLastPollState = GPIO_ODR(Port);
    //mState and mChaged are shared between the isr and the main thread
    volatile uint16_t mState = GPIO_ODR(Port);
    volatile uint16_t mChanged = 0;
    //===
    EventCb mHandler;
    void* mHandlerUserp;
    //repeat stuff
    enum { kRptCount = CountOnes<RepeatPins>::value,
           kRptShift = Right0Count<RepeatPins>::value
         };
    struct RepeatState
    {
        enum { kDelayPauseInitial = 100, kDelayRepeatInitial = 40 }; //x10 milliseconds
        uint32_t mLastTs;
        uint8_t mDelayX10;
        uint8_t mRepeatCnt;
    };
    RepeatState mRptStates[kRptCount];
public:
    Buttons(EventCb aCb, void* aUserp)
    : mState(GPIO_IDR(Port)), mHandler(aCb), mHandlerUserp(aUserp)
    {
        static_assert((RepeatPins & ~Pins) == 0);
        rcc_periph_clock_enable(getPortClock<Port>());
        if ((Flags & kOptNoInternalPuPd) == 0)
        {
            gpio_set_mode(Port, GPIO_MODE_INPUT,
                      GPIO_CNF_INPUT_PULL_UPDOWN, Pins);
            if (Flags & kOptActiveLow)
                gpio_set(Port, Pins); //pull-up
            else
                gpio_clear(Port, Pins); //pull-down
        }
        else
        {
            gpio_set_mode(Port, GPIO_MODE_INPUT,
                      GPIO_CNF_INPUT_FLOAT, Pins);
        }
    }
    /** @brief Polls the state of the button pins and queues events
     * for processing by process(). \c poll() is suitable for calling
     * from a periodic ISR
     */
    void poll()
    {
        // scan pins and detect changes
        uint16_t newState = GPIO_IDR(Port);
        uint16_t changedPins = (mLastPollState ^ newState) & Pins;
        mLastPollState = newState;

        if (changedPins)
        {
            mDebounceStartTs = DwtCounter::get(); //reset debounce timer
            mDebouncing |= changedPins; //add pins to the ones currently debounced
        }
        //check for end of debounce period
        if (mDebouncing
        && (DwtCounter::ticksToMs(DwtCounter::get() - mDebounceStartTs) >= DebounceMs))
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
        uint32_t now = DwtCounter::get();
        // atomically make a snapshot of the current button state and change flags
        bool intsWereEnabled;
        if (APollIrqN != 127)
        {
            intsWereEnabled = nvic_get_irq_enabled(APollIrqN);
            if (intsWereEnabled)
                nvic_disable_irq(APollIrqN);
        }
        uint16_t state = mState;
        uint16_t changed = mChanged;
        mChanged = 0;
        if ((APollIrqN != 127) && intsWereEnabled)
        {
            nvic_enable_irq(APollIrqN);
        }
        for (uint8_t idx=Right0Count<Pins>::value; idx < HighestBitIdx<Pins>::value; idx++)
        {
            uint16_t mask = 1 << idx;
            if (changed & mask)
            {
                uint8_t event;
                uint16_t pinState = state & mask;
                if (pinState) //just pressed
                {
                    event = kEventPress;
                    if (kRptCount && (pinState & RepeatPins))
                    {
                        // record timestamp for newly pressed repeatable buttons
                        auto& rptState = mRptStates[idx-kRptShift];
                        rptState.mLastTs = now;
                        rptState.mDelayX10 = RepeatState::kDelayPauseInitial;
                        rptState.mRepeatCnt = 0;
                    }
                }
                else
                {
                    event = kEventRelease;
                }
                mHandler(idx, event, mHandlerUserp);
            }
            else
            {
                if (!kRptCount) //repeat not enabled
                    continue;
                if (!(state & mask & RepeatPins)) //button does not repeat or not pressed
                    continue;

                auto& rptState = mRptStates[idx-kRptShift];
                uint32_t ms10 = DwtCounter::ticksTo10Ms(now - rptState.mLastTs);
                if (ms10 < rptState.mDelayX10)
                    continue; //too early for repeat

                rptState.mLastTs = now;
                if (rptState.mDelayX10 == RepeatState::kDelayPauseInitial)
                {
                    rptState.mDelayX10 = RepeatState::kDelayRepeatInitial;
                    rptState.mRepeatCnt = 0;
                }
                else
                {
                    uint16_t dur = (++rptState.mRepeatCnt) * rptState.mDelayX10;
                    if (dur > 150)
                    {
                        if (rptState.mDelayX10 > 2)
                        {
                           rptState.mDelayX10 -= 1;
                           rptState.mRepeatCnt = 0;
                        }
                    }
                    else if (dur > 70)
                    {
                        if (rptState.mDelayX10 >= 10)
                        {
                           rptState.mDelayX10 >>= 1;
                           rptState.mRepeatCnt = 0;
                        }
                    }

                }
                mHandler(idx, kEventRepeat, mHandlerUserp);
            }
        }
    }
    /** @brief Sets the user button event handler and its user pointer */
    void setHandler(EventCb h, void* userp)
    {
        mHandler = h;
        mHandlerUserp = userp;
    }
    /** @brief Sets the user pointer that is passed to the event handler */
    void setHandlerUserp(void* userp) { mHandlerUserp = userp; }
};
template <> rcc_periph_clken getPortClock<GPIOA>() { return RCC_GPIOA; }
template <> rcc_periph_clken getPortClock<GPIOB>() { return RCC_GPIOB; }
template <> rcc_periph_clken getPortClock<GPIOC>() { return RCC_GPIOC; }
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
