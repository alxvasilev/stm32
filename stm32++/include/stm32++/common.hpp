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
#endif // COMMON_HPP
