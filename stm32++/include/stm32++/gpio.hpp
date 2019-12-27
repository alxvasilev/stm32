#ifndef STM32PP_GPIO_HPP
#define STM32PP_GPIO_HPP

/**
  GPIO classes and definitions for the STM32++ library
  @author Alexander Vassilev
  @copyright BSD License
*/

#include <stdint.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include "exti.hpp"

namespace nsgpio
{
template <uint32_t aPort, uint16_t aPin>
struct Pin
{
    enum: uint32_t { kPort = aPort };
    enum: uint16_t { kPin = aPin };
    static const auto kClockId = PeriphInfo<kPort>::kClockId;
    static void enableClock() { ::rcc_periph_clock_enable(kClockId); }

    template <bool StartClock=true, typename M, typename C>
    static void setMode(M mode, C config)
    {
        if (StartClock) {
            enableClock();
        }
        gpio_set_mode(kPort, mode, config, kPin);
    }
    template <bool StartClock=true>
    static void setModeInputPuPd(bool pullUp)
    {
        setMode<StartClock>(GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN);
        if (pullUp)
        {
            set();
        }
        else
        {
            clear();
        }
    }
    static void set() { gpio_set(kPort, kPin); }
    static void clear() { gpio_clear(kPort, kPin); }
    static void toggle() { gpio_toggle(kPort, kPin); }
    static uint16_t get() { return GPIO_IDR(kPort) & kPin; }
    static void configInterrupt(enum exti_trigger_type trigger)
    {
        rcc_periph_clock_enable(RCC_AFIO);
        exti_select_source(kPin, kPort);
        exti_set_trigger(kPin, trigger);
    }
    static const auto irqn() { return PeriphInfo<kPin>::kIrqn; }
    static void enableInterrupt()
    {
        exti_enable_request(kPin);
    }
    static void disableInterrupt()
    {
        exti_disable_request(kPin);
    }
};
}

STM32PP_PERIPH_INFO(GPIOA)
    static constexpr rcc_periph_clken kClockId = RCC_GPIOA;
};

STM32PP_PERIPH_INFO(GPIOB)
    static constexpr rcc_periph_clken kClockId = RCC_GPIOB;
};

STM32PP_PERIPH_INFO(GPIOC)
    static constexpr rcc_periph_clken kClockId = RCC_GPIOC;
};

STM32PP_PERIPH_INFO(GPIOD)
    static constexpr rcc_periph_clken kClockId = RCC_GPIOD;
};

STM32PP_PERIPH_INFO(GPIOE)
    static constexpr rcc_periph_clken kClockId = RCC_GPIOE;
};

STM32PP_PERIPH_INFO(GPIOF)
    static constexpr rcc_periph_clken kClockId = RCC_GPIOF;
};

STM32PP_PERIPH_INFO(GPIOG)
    static constexpr rcc_periph_clken kClockId = RCC_GPIOG;
};


#endif // COMMON_HPP
