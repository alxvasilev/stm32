#ifndef STM32PP_COMMON_HPP
#define STM32PP_COMMON_HPP

// Unspecialized template for peripheral info classes
// Peripheral headers specialize this (in the global namespace)
// for every peripheral and fill in various
// parameters as enums or constexprs - such as DMA controller,
// DMA rx and tx channels, etc
template <uint32_t id>
struct PeriphInfo;

// Define a class to check whether a class has a member
#define TYPE_SUPPORTS(ClassName, Expr)                                 \
template <typename C> struct ClassName {                               \
    template <typename T> static uint16_t check(decltype(Expr));       \
    template <typename> static uint8_t check(...);                     \
    static bool const value = sizeof(check<C>(0)) == sizeof(uint16_t); \
};

template<>
struct PeriphInfo<GPIOA> { static constexpr rcc_periph_clken kClockId = RCC_GPIOA; };
template<>
struct PeriphInfo<GPIOB> { static constexpr rcc_periph_clken kClockId = RCC_GPIOB; };
template<>
struct PeriphInfo<GPIOC> { static constexpr rcc_periph_clken kClockId = RCC_GPIOC; };
template<>
struct PeriphInfo<GPIOD> { static constexpr rcc_periph_clken kClockId = RCC_GPIOD; };
template<>
struct PeriphInfo<GPIOE> { static constexpr rcc_periph_clken kClockId = RCC_GPIOE; };
template<>
struct PeriphInfo<GPIOF> { static constexpr rcc_periph_clken kClockId = RCC_GPIOF; };
template<>
struct PeriphInfo<GPIOG> { static constexpr rcc_periph_clken kClockId = RCC_GPIOG; };

#endif // COMMON_HPP
