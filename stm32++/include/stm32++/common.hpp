#ifndef STM32PP_COMMON_HPP
#define STM32PP_COMMON_HPP

// Define a class to check whether a class has a member
#define TYPE_SUPPORTS(ClassName, Expr)                                 \
template <typename C> struct ClassName {                               \
    template <typename T> static uint16_t check(decltype(Expr));       \
    template <typename> static uint8_t check(...);                     \
    static bool const value = sizeof(check<C>(0)) == sizeof(uint16_t); \
};

// Unspecialized template for peripheral info classes
// Peripheral headers specialize this (in the global namespace)
// for every peripheral and fill in various
// parameters as enums or constexprs - such as DMA controller,
// DMA rx and tx channels, etc
template <uint32_t id, bool remap=false>
struct PeriphInfo;


#define __STM32PP_PERIPH_INFO(periphId)               \
    template<bool Remap>                              \
    struct PeriphInfo<periphId, Remap>                \
    {                                                 \
        enum: uint32_t { kPeriphId = periphId };      \
        static constexpr bool kPinsRemapped = Remap;

#ifndef NDEBUG
    #define STM32PP_PERIPH_INFO(periphId) __STM32PP_PERIPH_INFO(periphId) \
        static constexpr const char* periphName() { return #periphId; }
#else
    #define STM32PP_PERIPH_INFO(periphId) __STM32PP_PERIPH_INFO(periphId);
#endif

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
