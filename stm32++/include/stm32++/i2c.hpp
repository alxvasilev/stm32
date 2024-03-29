/**
 * @author Alexander Vassilev
 * @copyright BSD License
 */
#ifndef STM32PP_I2C_H
#define STM32PP_I2C_H

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/cm3/nvic.h>
#include <stm32++/timeutl.hpp>
#include <stm32++/tsnprintf.hpp>
#include <stm32++/semihosting.hpp>
#include <stm32++/dma.hpp>
namespace dma
{
}

namespace nsi2c
{
/* Private definitions */
/** @brief Timeout for ACK of sent data. Used also for timeout for device
 * response in \c inDeviceConnected() */
enum { kTimeoutMs = 10 };

/* Private defines */
enum: bool { kTxMode = true, kRxMode = false,
             kAckEnable = true, kAckDisable = false };

template <uint32_t I2C>
class I2c: public PeriphInfo<I2C>
{
public:
    void init(bool fastMode=true, uint8_t ownAddr=0x15)
    {
        rcc_periph_clock_enable(PeriphInfo<this->kPortId>::kClockId);
        /* Set alternate functions for the SCL and SDA pins of I2C1. */
        gpio_set_mode(this->kPortId, GPIO_MODE_OUTPUT_50_MHZ,
                      GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,
                      this->kPinSda | this->kPinScl);

        rcc_periph_clock_enable(this->kClockId);
        i2c_reset(I2C);
        /* Disable the I2C before changing any configuration. */
        i2c_peripheral_disable(I2C);
        i2c_set_clock_frequency(I2C, rcc_apb1_frequency / 1000000);
        /* 400KHz */
        if (fastMode)
        {
            // Datasheet suggests 0x1e.
            i2c_set_fast_mode(I2C);
            uint32_t clockRatio = rcc_apb1_frequency / 400000;
            i2c_set_ccr(I2C, (clockRatio*2+3)/6); //round clockRatio/3
            i2c_set_dutycycle(I2C, I2C_CCR_DUTY_DIV2);
            /*
         * rise time for 400kHz => 300ns and 100kHz => 1000ns; 300ns/28ns = 10;
         * Incremented by 1 -> 11.
         */
            uint32_t clocks = 300 / ((2000000000+rcc_apb1_frequency)/(rcc_apb1_frequency*2)) + 1;
            i2c_set_trise(I2C, clocks);
        }
        else
        {
            i2c_set_standard_mode(I2C);
            uint32_t clockRatio = rcc_apb1_frequency / 100000;
            i2c_set_ccr(I2C, clockRatio/2);
            uint32_t clocks = 1000 / ((2000000000+rcc_apb1_frequency)/(rcc_apb1_frequency*2)) + 1;
            i2c_set_trise(I2C, clocks);
        }

        /*
     * This is our slave address - needed only if we want to receive from
     * other masters.
     */
        i2c_set_own_7bit_slave_address(I2C, ownAddr);
        i2c_disable_ack(I2C);
        /* If everything is configured -> enable the peripheral. */
        i2c_peripheral_enable(I2C);
    }

void blockingSend(uint8_t* data, uint16_t count)
{
    uint8_t* end = data+count;
    while (data < end)
    {
        sendByte(*(data++));
    }
}

/** Dummy function to prevent compile errors of
 * \code if (hasDma) dmaSend(...)
 * even when the class has no DMA support. If an attempt is made to actually
 * call dmaSend(), this will result in a link error because there is no
 * implementation of \c dmaSend(), only a prototype
 */
void dmaSend(uint8_t* data, uint16_t count, void* freeFunc=nullptr);

bool startSend(uint8_t address, bool ack=false)
{
    return start(address, kTxMode, ack);
}
bool startRecv(uint8_t address, bool ack=false)
{
    return start(address, kRxMode, ack);
}

/* Private functions */
bool start(uint8_t address, bool tx, bool ack)
{
	/* Generate I2C start pulse */
    i2c_send_start(I2C);
    ElapsedTimer timer;

    /* Waiting for START to be sent and switched to master mode. */
    while (!(I2C_SR1(I2C) & I2C_SR1_SB))
    {
        if (timer.msElapsed() > kTimeoutMs)
            return false;
    }
    xassert(I2C_SR2(I2C) & I2C_SR2_MSL);

    if (ack)
        i2c_enable_ack(I2C);
    else
        i2c_disable_ack(I2C);

    /* Send destination address. */
    i2c_send_7bit_address(I2C, address, tx ? I2C_WRITE : I2C_READ);

    /* Waiting for address to be transferred. */
    while (!(I2C_SR1(I2C) & I2C_SR1_ADDR))
    {
        if (timer.msElapsed() > kTimeoutMs)
            return false;
    }
    xassert(!(I2C_SR1(I2C) & I2C_SR1_SB));
    (volatile uint32_t)I2C_SR2(I2C);

#ifndef NDEBUG
    uint32_t sr2 = I2C_SR2(I2C);
    if (tx)
        xassert(sr2 & I2C_SR2_TRA);
    else
        xassert(!(sr2 & I2C_SR2_TRA));
#endif

    xassert((I2C_SR1(I2C) & I2C_SR1_ADDR) == 0);
    return true;
}

bool sendByteTimeout(uint8_t data)
{
    if (!(I2C_SR1(I2C) & I2C_SR1_TxE))
    {
        ElapsedTimer timer;
        while (!(I2C_SR1(I2C) & I2C_SR1_TxE))
        {
            if (timer.msElapsed() > kTimeoutMs)
                return false;
        }
    }
    i2c_send_data(I2C, data);
    return true;
}

template <typename... Args>
bool sendByteTimeout(uint8_t byte, Args... args)
{
    if (!sendByteTimeout(byte))
        return false;
    return sendByteTimeout(args...);
}

void sendByte(uint8_t data)
{
    while (!(I2C_SR1(I2C) & I2C_SR1_TxE));
    i2c_send_data(I2C, data);

}

template <typename... Args>
void sendByte(uint8_t byte, Args... args)
{
    sendByte(byte);
    sendByte(args...);
}

template <typename T>
bool vsendTimeout(T data)
{
    uint8_t* end = ((uint8_t*)&data)+sizeof(data);
    for (uint8_t* ptr = (uint8_t*)&data; ptr<end; ptr++)
    {
        if (!sendByteTimeout(*ptr))
            return false;
    }
}

template <typename T, typename... Args>
bool vsendTimeout(T val, Args... args)
{
    bool ret = (sizeof(T) == 1)
        ? sendByteTimeout(val)
        : sendTimeout(val);
    if (!ret)
        return false;
    return sendTimeout(args...);
}
template <typename T>
void vsend(T data)
{
    uint8_t* end = ((uint8_t*)&data)+sizeof(data);
    for (uint8_t* ptr = (uint8_t*)&data; ptr<end; ptr++)
    {
        sendByte(*ptr);
    }
}

template <typename T, typename... Args>
void vsend(T val, Args... args)
{
    if (sizeof(T) == 1)
        sendByte(val);
    else
        send(val);
    send(args...);
}

uint16_t recvByteTimeout()
{
    ElapsedTimer timer;
    while((I2C_SR1(I2C) & I2C_SR1_RxNE) == 0)
    {
        if (timer.msElapsed() > kTimeoutMs)
            return 0xffff;
    }
    return I2C_DR(I2C);
}

uint8_t recvByte()
{
    while((I2C_SR1(I2C) & I2C_SR1_RxNE) == 0);
    return I2C_DR(I2C);

}

bool recvTimeout(uint8_t* buf, size_t count)
{
    uint8_t* end = buf+count;
    while(buf < end)
    {
        ElapsedTimer timer;
        while ((I2C_SR1(I2C) & I2C_SR1_RxNE) == 0)
        {
            if (timer.msElapsed() > kTimeoutMs)
                return false;
        }
        *(buf++) = I2C_DR(I2C);
    }
}

void recv(uint8_t* buf, size_t count)
{
    uint8_t* end = buf+count;
    while(buf < end)
    {
        while ((I2C_SR1(I2C) & I2C_SR1_RxNE) == 0);
        *(buf++) = I2C_DR(I2C);
    }
}

void stop()
{
    //wait transfer complete
#ifndef NDEBUG
    ElapsedTimer timer;
    while (!(I2C_SR1(I2C) & (I2C_SR1_BTF | I2C_SR1_TxE)))
    {
        if (timer.msElapsed() > kTimeoutMs)
        {
            xassert(false && "stop(): Timeout waiting for output flush");
            for(;;);
        }
    }
#else
    while (!(I2C_SR1(I2C) & (I2C_SR1_BTF | I2C_SR1_TxE)));
#endif
    /* Send STOP condition. */
    i2c_send_stop(I2C);
}

bool stopTimeout()
{
    //wait transfer complete
    ElapsedTimer timer;
    while (!(I2C_SR1(I2C) & (I2C_SR1_BTF | I2C_SR1_TxE)))
    {
        if (timer.msElapsed() > kTimeoutMs)
            return false;
    }
    i2c_send_stop(I2C);
    return true;
}

bool isDeviceConnected(uint8_t address)
{
    /* Try to start, function will return 0 in case device will send ACK */
    bool connected = start(address, kTxMode, kAckEnable);
    if (!connected)
        return false;
    stop();
    return true;
}

uint8_t findFirstDevice(uint8_t from=0)
{
    for (uint8_t i = from; i < 128; i++)
        if (isDeviceConnected(i))
            return i;
    return 0xff;
}
void dmaStartPeripheralTx() { i2c_enable_dma(I2C); }
void dmaStartPeripheralRx() { i2c_enable_dma(I2C); }
//WARNING: The dmaStopPerpheralXX can be called from an ISR
void dmaStopPeripheralTx()
{
    i2c_disable_dma(I2C);
    this->stop();
}
void dmaStopPeripheralRx()
{
    i2c_disable_dma(I2C);
    this->stop();
}
};
}
STM32PP_PERIPH_INFO(I2C1)
    enum: uint32_t { kPortId = GPIOB };
    enum: uint16_t { kPinScl = GPIO_I2C1_SCL, kPinSda = GPIO_I2C1_SDA };
    static constexpr rcc_periph_clken kClockId = RCC_I2C1;
    enum: uint32_t { kDmaTxId = DMA1, kDmaRxId = DMA1 };
    enum: uint8_t {
        kDmaTxChannel = DMA_CHANNEL6,
        kDmaRxChannel = DMA_CHANNEL7,
        kDmaWordSize = 1
    };
    static const uint32_t dmaRxDataRegister() { return (uint32_t)(&I2C1_DR); }
    static const uint32_t dmaTxDataRegister() { return (uint32_t)(&I2C1_DR); }
};

STM32PP_PERIPH_INFO(I2C2)
    enum: uint32_t { kPortId = GPIOB };
    enum: uint16_t { kPinScl = GPIO_I2C2_SCL, kPinSda = GPIO_I2C2_SDA };
    static constexpr rcc_periph_clken kClockId = RCC_I2C2;
    enum: uint32_t { kDmaTxId = DMA1, kDmaRxId = DMA1 };
    enum: uint8_t {
        kDmaTxChannel = DMA_CHANNEL4,
        kDmaRxChannel = DMA_CHANNEL5,
        kDmaWordSize = 1
    };
    static const uint32_t dmaTxDataRegister() { return (uint32_t)(&I2C2_DR); }
    static const uint32_t dmaRxDataRegister() { return (uint32_t)(&I2C2_DR); }
};

#endif
