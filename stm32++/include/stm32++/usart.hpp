/** @author Alexander Vassilev
 * @copyright BSD License
 */

#ifndef _USART_HPP_INCLUDED
#define _USART_HPP_INLCUDED

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/dma.h>
#include "snprint.h"
#include <assert.h>

class UsartBase
{
protected:
public:
    static bool hasInput() { return false; }
    static bool hasOutput() { return false; }
    void enableInput() {}
    void enableOutput() {}
};

class Usart1: public UsartBase
{
protected:
    enum: uint32_t {
      kInputPin = GPIO_USART1_RX,
      kOutputPin = GPIO_USART1_TX
    };
    enum: uint8_t {
      kDmaChannelTx = DMA_CHANNEL4,
      kDmaChannelRx = DMA_CHANNEL5
    };
public:
    enum: uint32_t { kUsartId = USART1 };
    static const rcc_periph_clken kClockId = RCC_USART1;
    using UsartBase::UsartBase;
};

class Usart2: public UsartBase
{
    enum: uint32_t {
        kInputPin = GPIO_USART2_RX,
        kOutputPin = GPIO_USART2_TX
    };
    enum: uint8_t {
        kDmaChannelTx = DMA_CHANNEL7,
        kDmaChannelRx = DMA_CHANNEL6
    };

public:
    enum: uint32_t { kUsartId = USART2 };
    static const rcc_periph_clken kClockId = RCC_USART2;
};

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
    static bool hasOutput() { return true; }
    static void enableTxInterrupt()
    {
        USART_CR1(Base::kUsartId) |= USART_CR1_TXEIE;
    }
    static void printSink(const char* str, size_t len, int fd, void* userp)
    {
        auto bufend = str+len;
        for(; str<bufend; str++)
        {
            usart_send_blocking(Base::kUsartId, *str);
        }
    }
    void sendBlocking(const char* buf, size_t size)
    {
        const char* end = buf+size;
        while(buf < end)
        {
            usart_send_blocking(Base::kUsartId, *(buf++));
        }
    }
    void sendBlocking(const char* str)
    {
        while(*str)
        {
            usart_send_blocking(Base::kUsartId, *str);
            str++;
        }
    }
};

template<class Base, uint32_t Dma>
class UsartTxDma: public UsartTx<Base>
{
protected:
    typedef void(*FreeFunc)(void*);
    volatile bool mTxBusy = false;
    volatile const void* mTxBuf = nullptr;
    volatile FreeFunc mFreeFunc = nullptr;

    typedef UsartTxDma<Base, Dma> Self;
    static void dmaPrintSink(const char* str, size_t len, int fd, void* userp)
    {
        auto& self = *static_cast<Self*>(userp);
        self.dmaWrite((const void*)str, len, tprintf_free);
    }
public:
    volatile bool txBusy() const { return mTxBusy; }

    /** @brief Initiates a DMA transfer of the buffer specified
     * by the \c data and \c size paremeters.
     * When the transfer is complete and the specified \c freeFunc
     * is not \c nullptr, that function will be called with the \c data
     * param to free it. It can be used also as a completion callback.
     * @note Note that \c freeFunc will be called from an interrupt.
     * If there is already a transfer in progress, \c dmaWrite blocks until
     * the previous transfer completes (and the previous buffer is freed,
     * in case \c freeFunc was provided for the previous transfer).
     */
    bool dmaWrite(const void* data, uint16_t size, FreeFunc freeFunc)
    {
        enum { chan = Base::kDmaChannelTx };

        while(mTxBusy);
        mTxBusy = true;
        mTxBuf = data;
        mFreeFunc = freeFunc;

        dma_channel_reset(Dma, chan);
        dma_set_peripheral_address(Dma, chan, (uint32_t)&(USART_DR(Base::kUsartId)));
        dma_set_memory_address(Dma, chan, (uint32_t)data);
        dma_set_number_of_data(Dma, chan, size);
        dma_set_read_from_memory(Dma, chan);
        dma_enable_memory_increment_mode(Dma, chan);
        dma_disable_peripheral_increment_mode(Dma, chan);
        dma_set_peripheral_size(Dma, chan, DMA_CCR_PSIZE_8BIT);
        dma_set_memory_size(Dma, chan, DMA_CCR_MSIZE_8BIT);
        dma_set_priority(Dma, chan, DMA_CCR_PL_VERY_HIGH);
        dma_enable_transfer_complete_interrupt(Dma, chan);

        dma_enable_channel(Dma, chan);
        usart_enable_tx_dma(Base::kUsartId);
        return true;
    }
    void dmaTxIsr()
    {
        enum { chan = Base::kDmaChannelTx };
        if ((DMA_ISR(Dma) & DMA_ISR_TCIF(chan)) == 0)
            return;

        DMA_IFCR(Dma) |= DMA_IFCR_CTCIF(chan);
        stopTxDma();
    }
    void stopTxDma()
    {
        enum { chan = Base::kDmaChannelTx };
        dma_disable_transfer_complete_interrupt(Dma, chan);
        usart_disable_tx_dma(Base::kUsartId);
        dma_disable_channel(Dma, chan);
        assert(mTxBuf);
        //FIXME: mFreeFunc may not be reentrant
        if (mFreeFunc)
            mFreeFunc((void*)mTxBuf);
        mTxBuf = nullptr;
        mTxBusy = false;
    }
    void sinkPrintOutput()
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
        USART_CR1(Base::kUsartId) |= USART_CR1_RXNEIE;
    }
    void recvBlocking(char* buf, size_t bufsize)
    {
        void* end = buf+bufsize;
        while(buf < end)
        {
            *(buf++) = usart_recv_blocking(Base::kUsartId);
        }
    }
    size_t recvLine(char* buf, size_t bufsize)
    {
        char* end = buf+bufsize-1;
        char* ptr = buf;
        while(ptr < end)
        {
            char ch = usart_recv_blocking(Base::kUsartId);
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

template<class Base, uint32_t Dma>
class UsartRxDma: public UsartRx<Base>
{
protected:
    volatile bool mRxBusy = false;
public:
    volatile bool rxBusy() const { return mRxBusy; }
    bool dmaRead(const char *data, uint16_t size)
    {
        enum { chan = Base::kDmaChannelRx };

        while(mRxBusy);
        mRxBusy = true;
        dma_channel_reset(Dma, chan);
        dma_set_peripheral_address(Dma, chan, (uint32_t)&(USART_DR(Base::kUsartId)));
        dma_set_memory_address(Dma, chan, (uint32_t)data);
        dma_set_number_of_data(Dma, chan, size);
        dma_set_read_from_peripheral(Dma, chan);
        dma_enable_memory_increment_mode(Dma, chan);
        dma_disable_peripheral_increment_mode(Dma, chan);
        dma_set_peripheral_size(Dma, chan, DMA_CCR_PSIZE_8BIT);
        dma_set_memory_size(Dma, chan, DMA_CCR_MSIZE_8BIT);
        dma_set_priority(Dma, chan, DMA_CCR_PL_VERY_HIGH);
        dma_enable_transfer_complete_interrupt(Dma, chan);

        dma_enable_channel(Dma, chan);
        usart_enable_rx_dma(Base::kUsartId);
        return true;
    }
    void dmaRxIsr()
    {
        if ((DMA_ISR(Dma) & DMA_ISR_TCIF(Base::kDmaChannelRx)) == 0)
            return;

        DMA_IFCR(Dma) |= DMA_IFCR_CTCIF(Base::kDmaChannelRx);
        stopRxDma();
    }
    void stopRxDma()
    {
        dma_disable_transfer_complete_interrupt(Dma, Base::kDmaChannelRx);
        usart_disable_rx_dma(Base::kUsartId);
        dma_disable_channel(Dma, Base::kDmaChannelRx);
        mRxBusy = false;
    }
};

template <class U>
class UsartRxTx: public UsartRx<UsartTx<U>>
{
    typedef UsartRx<UsartTx<U>> Base;
    using Base::Base;
};

enum: uint8_t { kEnableInput = 1, kEnableOutput = 2 };

template <class Base>
class Usart: public Base
{
public:
    void init(uint8_t flags, uint32_t baudRate)
    {
        rcc_periph_clock_enable(RCC_GPIOA);
        rcc_periph_clock_enable(this->kClockId);
        bool out = ((flags & kEnableOutput) && this->hasOutput());
        if (out)
            this->enableOutput();
        bool in = ((flags & kEnableInput) && this->hasInput());
        if (in)
            this->enableInput();

        /* Setup UART parameters. */
        usart_set_baudrate(this->kUsartId, baudRate);
        usart_set_databits(this->kUsartId, 8);
        usart_set_stopbits(this->kUsartId, USART_STOPBITS_1);
        uint32_t mode = in ? USART_MODE_RX : 0;
        if (out)
        {
            mode |= USART_MODE_TX;
        }
        usart_set_mode(this->kUsartId, mode);
        usart_set_parity(this->kUsartId, USART_PARITY_NONE);
        usart_set_flow_control(this->kUsartId, USART_FLOWCONTROL_NONE);

        /* Finally enable the USART. */
        usart_enable(this->kUsartId);
    }
    void stop()
    {
        usart_disable(this->kUsartId);
        rcc_periph_clock_disable(this->kClockId);
    }
};


#endif
