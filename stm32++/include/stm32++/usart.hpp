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
#include "gpio.hpp"
#include "tprintf.hpp"
#include "dma.hpp"
#include <assert.h>
#ifdef STM32PP_USART_DEBUG
#define STM32PP_USART_LOG(fmtString,...) tprintf("%: " fmtString "\n", Self::periphName(), ##__VA_ARGS__)
#else
#define STM32PP_USART_LOG(fmtString,...)
#endif

#define STM32PP_LOG_DEBUG(fmtString,...) STM32PP_USART_LOG(fmtString, ##__VA_ARGS__)

STM32PP_PERIPH_INFO(USART1)
    enum: uint32_t { kPort = GPIOA };
    enum: uint16_t { kPinTx = GPIO_USART1_TX, kPinRx = GPIO_USART1_RX };
    static constexpr rcc_periph_clken kClockId = RCC_USART1;
    enum: uint32_t { kDmaTxId = DMA1, kDmaRxId = DMA1 };
    enum: uint8_t {
        kDmaTxChannel = DMA_CHANNEL4,
        kDmaRxChannel = DMA_CHANNEL5
    };
    static const uint8_t dmaWordSize() { return 1; }
    static const uint32_t dmaRxDataRegister() { return (uint32_t)(&USART1_DR); }
    static const uint32_t dmaTxDataRegister() { return (uint32_t)(&USART1_DR); }
};

STM32PP_PERIPH_INFO(USART2)
    enum: uint32_t { kPort = GPIOA };
    enum: uint16_t { kPinTx = GPIO_USART2_TX, kPinRx = GPIO_USART2_RX };
    static constexpr rcc_periph_clken kClockId = RCC_USART2;
    enum: uint32_t { kDmaTxId = DMA1, kDmaRxId = DMA1 };
    enum: uint8_t {
        kDmaTxChannel = DMA_CHANNEL7,
        kDmaRxChannel = DMA_CHANNEL6
    };
    static const uint8_t dmaWordSize() { return 1; }
    static const uint32_t dmaRxDataRegister() { return (uint32_t)(&USART2_DR); }
    static const uint32_t dmaTxDataRegister() { return (uint32_t)(&USART2_DR); }
};

STM32PP_PERIPH_INFO(USART3)
    enum: uint32_t { kPort = GPIOB };
    enum: uint16_t { kPinTx = GPIO_USART3_TX, kPinRx = GPIO_USART3_RX };
    static constexpr rcc_periph_clken kClockId = RCC_USART3;
    enum: uint32_t { kDmaTxId = DMA1, kDmaRxId = DMA1 };
    enum: uint8_t {
        kDmaTxChannel = DMA_CHANNEL2,
        kDmaRxChannel = DMA_CHANNEL3
    };
    static const uint8_t dmaWordSize() { return 1; }
    static const uint32_t dmaRxDataRegister() { return (uint32_t)(&USART3_DR); }
    static const uint32_t dmaTxDataRegister() { return (uint32_t)(&USART3_DR); }
};

namespace nsusart
{
enum: uint8_t
{
    kOptEnableRx = 1,
    kOptEnableTx = 2
};

template <uint32_t USART, bool Remap=false>
class Usart: public PeriphInfo<USART, Remap>
{
protected:
    typedef Usart<USART, Remap> Self;
    void enableTx()
    {
        gpio_set_mode(Self::kPort, GPIO_MODE_OUTPUT_50_MHZ,
            GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, Self::kPinTx);
        STM32PP_USART_LOG("Enabled TX pin");
    }
public:
    void enableTxInterrupt()
    {
        USART_CR1(Self::kPeriphId) |= USART_CR1_TXEIE;
        STM32PP_USART_LOG("Enabled TX interrupt");
    }
    void sendBlocking(const char* buf, size_t size)
    {
        const char* bufend = buf+size;
        for(; buf < bufend; buf++)
        {
            usart_send_blocking(Self::kPeriphId, *buf);
        }
    }
    void sendBlocking(const char* str)
    {
        while(*str)
        {
            usart_send_blocking(Self::kPeriphId, *str);
            str++;
        }
    }
protected:
    void dmaStartPeripheralTx()
    {
        usart_enable_tx_dma(Self::kPeriphId);
    }
    void dmaStopPeripheralTx()
    {
        usart_disable_tx_dma(Self::kPeriphId);
    }
    // Rx part
    void enableRx()
    {
        gpio_set_mode(Self::kPort, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT,
            Self::kPinRx);
        STM32PP_USART_LOG("Enabled TX pin");
    }
    void dmaStartPeripheralRx()
    {
        usart_enable_rx_dma(Self::kPeriphId);
    }
    void dmaStopPeripheralRx()
    {
        usart_disable_rx_dma(Self::kPeriphId);
    }
public:
    static void enableRxInterrupt()
    {
        USART_CR1(Self::kPeriphId) |= USART_CR1_RXNEIE;
        STM32PP_USART_LOG("Enabled RX interrupt");
    }
    void recvBlocking(char* buf, size_t bufsize)
    {
        void* end = buf+bufsize;
        while(buf < end)
        {
            *(buf++) = usart_recv_blocking(Self::kPeriphId);
        }
    }
    size_t recvLine(char* buf, size_t bufsize)
    {
        char* end = buf+bufsize-1;
        char* ptr = buf;
        while(ptr < end)
        {
            char ch = usart_recv_blocking(Self::kPeriphId);
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
    void init(uint8_t flags, uint32_t baudRate, uint32_t parity=USART_PARITY_NONE,
        uint32_t stopBits=USART_STOPBITS_1)
    {
        rcc_periph_clock_enable(PeriphInfo<Self::kPort>::kClockId);
        rcc_periph_clock_enable(Self::kClockId);
        usart_disable(USART);
        STM32PP_USART_LOG("Enabled clocks for port and USART peripheral");

        bool rx = (flags & kOptEnableRx);
        if (rx)
        {
            this->enableRx();
        }
        bool tx = (flags & kOptEnableTx);
        if (tx)
        {
            this->enableTx();
        }

        /* Setup UART parameters. */
        uint32_t mode = rx ? USART_MODE_RX : 0;
        if (tx)
        {
            mode |= USART_MODE_TX;
        }
        usart_set_mode(Self::kPeriphId, mode);
        usart_set_baudrate(Self::kPeriphId, baudRate);
        usart_set_databits(Self::kPeriphId, 8);
        usart_set_stopbits(Self::kPeriphId, stopBits);
        usart_set_parity(Self::kPeriphId, parity);
        usart_set_flow_control(Self::kPeriphId, USART_FLOWCONTROL_NONE);

        /* Finally enable the USART. */
        usart_enable(Self::kPeriphId);
        STM32PP_USART_LOG("Enabled with baudrate: %, databits: 8, stop bits: %, parity: %, no flow control",
            baudRate, stopBits, parity);
    }
    void powerOff()
    {
        usart_disable(Self::kPeriphId);
        rcc_periph_clock_disable(Self::kClockId);
    }
    /* This is only needed after powerOff() has been called.
     * init() powers on the peripheral on implicitly */
    void powerOn()
    {
        usart_enable(Self::kPeriphId);
        rcc_periph_clock_enable(Self::kClockId);
    }
};

template <class UsartDevice>
class PrintSink: public UsartDevice, public IPrintSink
{
    virtual IPrintSink::BufferInfo* waitReady() { return nullptr; }
    virtual void print(const char *str, size_t len, int info)
    {
        UsartDevice::sendBlocking(str, len);
    }
};
}

#endif
