/** @author Alexander Vassilev
 * @copyright BSD License
 */

#ifndef _USART_HPP_INCLUDED
#define _USART_HPP_INLCUDED

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
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
    static void printSink(const char* str, uint32_t len, int fd, void* userp)
    {
        auto bufend = str+len;
        for(; str<bufend; str++)
        {
            usart_send_blocking(Base::kUsartId, *str);
        }
    }
    void sendString(const char* str)
    {
        while(*str)
        {
            usart_send_blocking(Base::kUsartId, *str);
            str++;
        }
    }
};

template <class U>
class UsartRx: public U
{
protected:
    void enableInput()
    {
        gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT,
              this->kInputPin);
    }
public:
    static bool hasInput() { return true; }
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
        rcc_periph_clock_disable(this->kClockId);
    }
};


#endif
