/** @author Alexander Vassilev
 * @copyright BSD License
 */

#ifndef _USART_HPP_INCLUDED
#define _USART_HPP_INCLUDED

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/nvic.h>
#include "snprint.h"
#include "dma.hpp"
#include <assert.h>
namespace dma
{
template<>
struct DmaInfo<USART1>: public PeriphDmaInfo<USART1, DMA1, 4, 5, 8>
{
    static constexpr uint32_t dataRegister() { return (uint32_t)&USART1_DR; }
};
template<>
struct DmaInfo<USART2>: public PeriphDmaInfo<USART2, DMA1, 7, 6, 8>
{
    static constexpr uint32_t dataRegister() { return (uint32_t)&USART2_DR; }
};
}

namespace nsusart
{
template <uint32_t USART>
class UsartInfo;

template <>
struct UsartInfo<USART1>
{
    enum: uint32_t { kPeriphId = USART1 };
    static constexpr rcc_periph_clken kClockId = RCC_USART1;
    enum: uint32_t {
      kInputPin = GPIO_USART1_RX,
      kOutputPin = GPIO_USART1_TX
    };
};

template <>
struct UsartInfo<USART2>
{
    enum: uint32_t { kPeriphId = USART2 };
    static constexpr rcc_periph_clken kClockId = RCC_USART2;
    enum: uint32_t {
        kInputPin = GPIO_USART2_RX,
        kOutputPin = GPIO_USART2_TX
    };
};

template <uint32_t USART>
class UsartBase: public UsartInfo<USART>
{
public:
    enum: bool { hasInput = false };
    enum: bool { hasOutput = false };
    enum: uint32_t { periphId = USART };
    void enableInput() { assert(false && "There is no input"); }
    void enableOutput() { assert (false && "There is no output"); }
};

class Usart1: public UsartBase<USART1>{};
class Usart2: public UsartBase<USART2>{};

template <class Base>
class UsartTx: public Base
{
protected:
    void enableOutput()
    {
        gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
              GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, this->kOutputPin);
    }
public:
    using Base::Base;
    typedef Base Self;
    enum: bool { hasOutput = true };
    static void enableTxInterrupt()
    {
        USART_CR1(Base::periphId) |= USART_CR1_TXEIE;
    }
    static void blockingPrintSink(const char* str, size_t len, int fd, void* userp)
    {
        auto bufend = str+len;
        for(; str<bufend; str++)
        {
            usart_send_blocking(Self::periphId, *str);
        }
    }
    void setBlockingPrintSink()
    {
        setPrintSink(blockingPrintSink);
    }
    void sendBlocking(const char* buf, size_t size)
    {
        const char* end = buf+size;
        while(buf < end)
        {
            usart_send_blocking(Base::periphId, *(buf++));
        }
    }
    void sendBlocking(const char* str)
    {
        while(*str)
        {
            usart_send_blocking(Base::periphId, *str);
            str++;
        }
    }
};

template<class U, uint32_t Opts=dma::kDefaultOpts>
class UsartTxDma: public dma::Tx<UsartTx<U>, UsartTxDma<U, Opts>, Opts>
{
protected:
    typedef void(*FreeFunc)(void*);
    volatile bool mTxBusy = false;
    volatile const void* mTxBuf = nullptr;
    volatile FreeFunc mFreeFunc = nullptr;

    typedef UsartTxDma<U, Opts> Self;
    typedef dma::Tx<UsartTx<U>, Self, Opts> Base;
    static void dmaPrintSink(const char* str, size_t len, int fd, void* userp)
    {
        auto& self = *static_cast<Self*>(userp);
        self.dmaSend((const void*)str, len, tprintf_free);
    }
public:
    volatile bool txBusy() const { return mTxBusy; }
    void enableOutput()
    {
        Base::enableOutput();
        Self::dmaTxInit();
    }

    /** @brief Initiates a DMA transfer of the buffer specified
     * by the \c data and \c size paremeters.
     * When the transfer is complete and the specified \c freeFunc
     * is not \c nullptr, that function will be called with the \c data
     * param to free it. It can be used also as a completion callback.
     * @note Note that \c freeFunc will be called from an interrupt.
     * If there is already a transfer in progress, \c dmaWrite() blocks until
     * the previous transfer completes (and the previous buffer is freed,
     * in case \c freeFunc was provided for the previous transfer).
     */
    void dmaSend(const void* data, uint16_t size, FreeFunc freeFunc)
    {
        Base::dmaTxRequest(data, size, freeFunc);
        usart_enable_tx_dma(Self::periphId);
    }
    void dmaTxStop()
    {
        usart_disable_tx_dma(Self::periphId);
        this->dmaTxDisable();
    }
    void setDmaPrintSink()
    {
        setPrintSink(dmaPrintSink, this, kPrintSinkLeaveBuffer);
    }
};

template <class Base>
class UsartRx: public Base
{
protected:
    void enableInput()
    {
        gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT,
              this->kInputPin);
    }
public:
    static bool hasInput() { return true; }
    static void enableRxInterrupt()
    {
        USART_CR1(Base::periphId) |= USART_CR1_RXNEIE;
    }
    void recvBlocking(char* buf, size_t bufsize)
    {
        void* end = buf+bufsize;
        while(buf < end)
        {
            *(buf++) = usart_recv_blocking(Base::periphId);
        }
    }
    size_t recvLine(char* buf, size_t bufsize)
    {
        char* end = buf+bufsize-1;
        char* ptr = buf;
        while(ptr < end)
        {
            char ch = usart_recv_blocking(Base::periphId);
            if ((ch == '\r') || (ch == '\n'))
            {
                *ptr = 0;
                return ptr-buf;
            }
            *(ptr++) = ch;
        }
        *ptr = 0;
        return (size_t)-1;
    }
};

template<class U, uint8_t Opts=dma::kDefaultOpts>
class UsartRxDma: public dma::Rx<UsartRx<U>, UsartRxDma<U, Opts>, Opts>
{
protected:
    typedef dma::Rx<UsartRx<U>, UsartRxDma, Opts> Base;
    typedef UsartRxDma<U, Opts> Self;
public:
    void enableInput()
    {
        Base::enableInput();
        Self::dmaRxEnable();
    }
    void dmaRecv(const char *data, uint16_t size)
    {
        Base::dmaRxRequest(data, size);
        usart_enable_rx_dma(Self::periphId);
    }
    void dmaRxStop()
    {
        usart_disable_rx_dma(Base::periphId);
        this->dmaRxDisable();
    }
};

template <class U>
class UsartRxTx: public UsartRx<UsartTx<U>>
{};

template <class U, uint32_t Dma, uint32_t Opts=0>
class UsartRxTxDma: public UsartRxDma<UsartTxDma<U, Opts>, Opts>
{};

enum: uint8_t { kEnableRecv = 1, kEnableSend = 2 };

template <class Base>
class Usart: public Base
{
public:
    void init(uint8_t flags, uint32_t baudRate)
    {
        rcc_periph_clock_enable(RCC_GPIOA);
        rcc_periph_clock_enable(Base::kClockId);
        bool out = ((flags & kEnableSend) && this->hasOutput);
        if (out)
            this->enableOutput();
        bool in = ((flags & kEnableRecv) && this->hasInput);
        if (in)
            this->enableInput();

        /* Setup UART parameters. */
        usart_set_baudrate(this->periphId, baudRate);
        usart_set_databits(this->periphId, 8);
        usart_set_stopbits(this->periphId, USART_STOPBITS_1);
        uint32_t mode = in ? USART_MODE_RX : 0;
        if (out)
        {
            mode |= USART_MODE_TX;
        }
        usart_set_mode(this->periphId, mode);
        usart_set_parity(this->periphId, USART_PARITY_NONE);
        usart_set_flow_control(this->periphId, USART_FLOWCONTROL_NONE);

        /* Finally enable the USART. */
        usart_enable(this->periphId);
    }
    void stop()
    {
        usart_disable(this->periphId);
        rcc_periph_clock_disable(this->kClockId);
    }
};
}

#endif
