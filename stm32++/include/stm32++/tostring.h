#ifndef _TOSTRING_H
#define _TOSTRING_H

#include <type_traits>
#include <assert.h>
#include <stdint.h>


enum: uint8_t { kNoFlags = 0, kLowerCase = 0x0, kUpperCase = 0x10,
                kDontNullTerminate = 0x20, kNoPrefix = 0x40};

typedef uint8_t Flags;

template <int base, Flags flags=kNoFlags>
struct DigitConverter;

template <Flags flags>
struct DigitConverter<10, flags>
{
    enum { digitsPerByte = 3, prefixLen = 0 };
    static char* putPrefix(char* buf) { return buf; }
    static char toDigit(uint8_t digit) { return '0'+digit; }
};

template <Flags flags>
struct DigitConverter<16, flags>
{
    enum { digitsPerByte = 2, prefixLen = 2 };
    static char* putPrefix(char* buf) { buf[0] = '0', buf[1] = 'x'; return buf+2; }
    static char toDigit(uint8_t digit)
    {
        if (digit < 10)
            return '0'+digit;
        else
            return (flags & kUpperCase ? 'A': 'a')+(digit-10);
    }
};

template <Flags flags>
struct DigitConverter<2, flags>
{
    enum { digitsPerByte = 8, prefixLen = 2 };
    static char* putPrefix(char *buf) { buf[0] = '0'; buf[1] = 'b'; return buf+2; }
    static char toDigit(uint8_t digit) { return '0'+digit; }
};

template<int base=10, Flags flags=kNoFlags, typename Val>
typename std::enable_if<std::is_unsigned<Val>::value, char*>::type
toString(char* buf, uint16_t bufsize, Val val)
{
    assert(buf);
    DigitConverter<base, flags> digitConv;
    char stagingBuf[digitConv.digitsPerByte * sizeof(Val)];
    char* writePtr = stagingBuf;
    for (; val; val /= base)
    {
        Val digit = val % base;
        *(writePtr++) = digitConv.toDigit(digit);
    } while (val);

    if (((flags & kNoPrefix) == 0) && digitConv.prefixLen)
    {
        assert(bufsize >= digitConv.prefixLen+1);
        buf = digitConv.putPrefix(buf);
    }
    if (writePtr == stagingBuf) //only zeroes
    {
        *(buf++) = '0';
        if ((flags & kDontNullTerminate) == 0)
            *buf = 0;
        return buf;
    }

    if (bufsize < writePtr - stagingBuf) //not enough space for output, don't write anything
    {
        *buf = 0;
        return nullptr;
    }

    for (auto rptr = writePtr-1; rptr >= stagingBuf; rptr--)
        *(buf++) = *rptr;
    if ((flags & kDontNullTerminate) == 0)
        *buf = 0;
    return buf;
}

template<int base=10, Flags flags=kNoFlags, typename Val>
typename std::enable_if<std::is_signed<Val>::value, char*>::type
toString(char* buf, uint16_t bufsize, Val val)
{
    typedef typename std::make_unsigned<Val>::type UVal;
    if (val < 0)
    {
        if (bufsize < 2)
            return nullptr;
        *buf = '-';
        return toString<base, flags, UVal>(buf+1, bufsize-1, -val);
    }
    else
    {
        return toString<base, flags, UVal>(buf, bufsize, val);
    }
}
template <class T, class Enabled=void>
struct is_char_ptr
{
    enum:bool {value = false};
};

template<class T>
struct is_char_ptr<T, typename
  std::enable_if<std::is_pointer<T>::value
    && std::is_same<
      typename std::remove_const<typename std::remove_pointer<T>::type>::type,char>::value
        ,void>::type>
{
    enum: bool { value = true };
};

template <class T, int Size=sizeof(T)>
struct UnsignedEquiv{ enum: bool {invalid = true}; };

template <class T>
struct UnsignedEquiv<T, 1> { typedef uint8_t type; };

template <class T>
struct UnsignedEquiv<T, 2> { typedef uint16_t type; };

template <class T>
struct UnsignedEquiv<T, 4> { typedef uint32_t type; };

template <class T>
struct UnsignedEquiv<T, 8> { typedef uint64_t type; };

template <typename T, uint8_t aBase, Flags aFlags=kNoFlags>
struct NumFmt
{
    typedef typename UnsignedEquiv<T>::type ScalarType;
    enum: uint8_t { base = aBase };
    static const Flags flags = aFlags;
    ScalarType value;
    explicit NumFmt(T aVal): value((ScalarType)(aVal)){}
    explicit NumFmt(ScalarType aVal): value(aVal){}

};

template <uint8_t base, Flags flags=kNoFlags, class T>
NumFmt<T, base, flags> fmtNum(T aVal) { return NumFmt<T, base, flags>(aVal); }

template <uint8_t base, Flags flags=kNoFlags, class T>
NumFmt<T, base, flags> fmtStruct(T aVal)
{
    typedef NumFmt<T, base, flags> Fmt;
    return Fmt(*((typename Fmt::ScalarType*)&aVal));
}

template <Flags flags=kNoFlags, class P>
typename std::enable_if<std::is_pointer<P>::value && !is_char_ptr<P>::value, char*>::type
toString(char *buf, uint16_t bufsize, P ptr)
{
    return toString(buf, bufsize, fmtNum<16, flags>(ptr));
}

template<uint8_t base, Flags flags=kNoFlags, typename Val>
char* toString(char *buf, uint16_t bufsize, NumFmt<Val, base, flags> num)
{
    return toString<num.base, num.flags>(buf, bufsize, num.value);
}

template<Flags flags=kNoFlags>
char* toString(char* buf, uint16_t bufsize, const char* val)
{
    auto bufend = buf+bufsize-1;
    while(*val && (buf < bufend))
    {
        *(buf++) = *(val++);
    }
    if ((flags & kDontNullTerminate) == 0)
        *buf = 0;
    return buf;
}
template<Flags flags=kNoFlags>
char* toString(char* buf, uint16_t bufsize, char val)
{
    if (flags & kDontNullTerminate)
    {
        if(!bufsize)
            return nullptr;
        *(buf++) = val;
        return buf;
    }
    else
    {
        if (bufsize < 2)
        {
            *buf = 0;
            return nullptr;
        }
        *(buf++) = val;
        return buf;
    }
}

#endif
