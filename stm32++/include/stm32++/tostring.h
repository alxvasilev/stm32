/**
 * @author Alexander Vassilev
 * @copyright BSD License
 */

#ifndef _TOSTRING_H
#define _TOSTRING_H

#include <type_traits>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static_assert(sizeof(size_t) == sizeof(void*));
static_assert(sizeof(size_t) == sizeof(ptrdiff_t));
static_assert(std::is_unsigned<size_t>::value);

enum: uint8_t { kNoFlags = 0, kLowerCase = 0x0, kUpperCase = 0x10,
                kDontNullTerminate = 0x20, kNoPrefix = 0x40};

typedef uint8_t Flags;

template <size_t base, Flags flags=kNoFlags>
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

template<size_t base=10, Flags flags=kNoFlags, typename Val>
typename std::enable_if<std::is_unsigned<Val>::value
                     && std::is_integral<Val>::value, char*>::type
toString(char* buf, size_t bufsize, Val val, uint8_t numDigits=0)
{
    assert(buf);
    assert(bufsize);

    if ((flags & kDontNullTerminate) == 0)
        bufsize--;

    DigitConverter<base, flags> digitConv;
    char stagingBuf[digitConv.digitsPerByte * sizeof(Val)];
    char* writePtr = stagingBuf;
    for (; val; val /= base)
    {
        Val digit = val % base;
        *(writePtr++) = digitConv.toDigit(digit);
    };

    size_t len = writePtr - stagingBuf;
    size_t padLen;
    if (!len && !numDigits)
        padLen = 1;
    else if (len < numDigits)
        padLen = numDigits - len;
    else
        padLen = 0;

    if (((flags & kNoPrefix) == 0) && digitConv.prefixLen)
    {
        if (bufsize < digitConv.prefixLen+padLen+len)
        {
            *buf = 0;
            return nullptr;
        }
        buf = digitConv.putPrefix(buf);
    }
    else
    {
        if (bufsize < padLen+len)
        {
            *buf = 0;
            return nullptr;
        }
    }
    for(;padLen; padLen--)
    {
        *(buf++) = '0';
    }
    for(; len; len--)
    {
        *(buf++) = *(--writePtr);
    }

    if ((flags & kDontNullTerminate) == 0)
        *buf = 0;
    return buf;
}

template<size_t base=10, Flags flags=kNoFlags, typename Val>
typename std::enable_if<std::is_integral<Val>::value &&
                        std::is_signed<Val>::value, char*>::type
toString(char* buf, size_t bufsize, Val val)
{
    typedef typename std::make_unsigned<Val>::type UVal;
    if (val < 0)
    {
        if (bufsize < 2)
        {
            if (bufsize)
            {
                *buf = (flags & kDontNullTerminate) ? '-' : 0;
            }
            return nullptr;
        }
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
    enum: bool {value = false};
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

template <class T, size_t Size=sizeof(T)>
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
struct IntFmt
{
    typedef typename UnsignedEquiv<T>::type ScalarType;
    enum: uint8_t { base = aBase };
    static constexpr Flags flags = aFlags;
    ScalarType value;
    uint8_t padding;
    explicit IntFmt(T aVal, uint8_t aPad=0): value((ScalarType)(aVal)), padding(aPad){}
    template <class U=T, class=typename std::enable_if<!std::is_same<ScalarType, U>::value, void>::type>
    explicit IntFmt(ScalarType aVal, uint8_t aPad=0): value(aVal), padding(aPad){}
};

template <uint8_t base, Flags flags=kNoFlags, class T>
IntFmt<T, base, flags> fmtNum(T aVal, uint8_t aPad=6)
{ return IntFmt<T, base, flags>(aVal, aPad); }

template <uint8_t base, Flags flags=kNoFlags, class T>
IntFmt<T, base, flags> fmtStruct(T aVal)
{
    typedef IntFmt<T, base, flags> Fmt;
    return Fmt(*((typename Fmt::ScalarType*)&aVal));
}

template <Flags flags=kNoFlags, class P>
typename std::enable_if<std::is_pointer<P>::value && !is_char_ptr<P>::value, char*>::type
toString(char *buf, size_t bufsize, P ptr)
{
    return toString(buf, bufsize, fmtNum<16, flags>(ptr));
}

template<uint8_t base, Flags flags=kNoFlags, typename Val>
char* toString(char *buf, size_t bufsize, IntFmt<Val, base, flags> num)
{
    return toString<num.base, num.flags>(buf, bufsize, num.value, num.padding);
}

template<Flags flags=kNoFlags>
typename std::enable_if<(flags & kDontNullTerminate) == 0, char*>::type
toString(char* buf, size_t bufsize, const char* val)
{
    if (!bufsize)
        return nullptr;
    auto bufend = buf+bufsize-1; //reserve space for the terminating null
    while(*val)
    {
        if(buf >= bufend)
        {
            assert(buf == bufend);
            *buf = 0;
            return nullptr;
        }
        *(buf++) = *(val++);
    }
    *buf = 0;
    return buf;
}

template<Flags flags=kNoFlags>
typename std::enable_if<(flags & kDontNullTerminate), char*>::type
toString(char* buf, size_t bufsize, const char* val)
{
    auto bufend = buf+bufsize;
    while(*val)
    {
        if(buf >= bufend)
            return nullptr;
        *(buf++) = *(val++);
    }
    return buf;
}

template<Flags flags=kNoFlags>
typename std::enable_if<flags & kDontNullTerminate, char*>::type
toString(char* buf, size_t bufsize, char val)
{
    if(!bufsize)
        return nullptr;
    *(buf++) = val;
    return buf;
}

template<typename Val, Flags flags=kNoFlags>
typename std::enable_if<std::is_same<Val, char>:: value
     && (flags & kDontNullTerminate) == 0, char*>::type
toString(char* buf, size_t bufsize, Val val)
{
    if (bufsize >= 2)
    {
        *(buf++) = val;
        *buf = 0;
        return buf;
    }
    else if (bufsize == 1)
    {
        *buf = 0;
        return nullptr;
    }
    else
    {
        return nullptr;
    }
}
template <size_t base, uint8_t p>
struct Pow
{  enum: size_t { value = base * Pow<base, p-1>::value  }; };

template <size_t base>
struct Pow<base, 1>
{ enum: size_t { value = base }; };

template<typename Val, size_t Prec=6, Flags flags=kNoFlags>
typename std::enable_if<std::is_floating_point<Val>::value, char*>::type
toString(char* buf, size_t bufsize, Val val, uint8_t padding=0)
{
    if (!bufsize)
        return nullptr;
    char* bufRealEnd = buf+bufsize;
    if ((flags & kDontNullTerminate) == 0)
        bufsize--;

    char* bufend = buf+bufsize;
    if (val < 0)
    {
        if (bufsize < 4) //at least '-0.0'
        {
            *buf = 0;
            return nullptr;
        }
        *(buf++) = '-';
        val = -val;
    }
    else
    {
        if (bufsize < 3)
        {
            *buf = 0;
            return nullptr;
        }
    }
    size_t whole = (size_t)(val);
    size_t decimal = (val-whole)*Pow<10, Prec>::value+0.5;

    //we have some minimum space for null termination even if buffer is not enough
    buf = toString<10, flags>(buf, bufRealEnd-buf, whole, padding);
    if (!buf)
    {
        assert(*(bufRealEnd-1)==0); //assert null termination
        return nullptr;
    }
    assert(buf < bufRealEnd);
    if (bufend-buf < 2) //must have space at least for '.0' and optional null terminator
    {
        *buf = 0;
        return nullptr;
    }
    *(buf++) = '.';
    return toString(buf, bufRealEnd-buf, decimal, Prec);
}

template <class T, uint8_t aPrec, Flags aFlags=kNoFlags>
struct FpFmt
{
    enum: uint8_t { prec = aPrec };
    constexpr static Flags flags = aFlags;
    T value;
    uint8_t padding;
    FpFmt(T aVal, uint8_t aPad): value(aVal), padding(aPad){}
};

template <uint8_t aPrec, Flags aFlags=kNoFlags, class T>
auto fmtFp(T val, uint8_t pad=0)
{
    return FpFmt<T, aPrec, aFlags>(val, pad);
}

template <typename Val, uint8_t aPrec, Flags aFlags>
char* toString(char *buf, size_t bufsize, FpFmt<Val, aPrec, aFlags> fp)
{
    return toString<Val, aPrec, aFlags>(buf, bufsize, fp.value, fp.padding);
}

#endif
