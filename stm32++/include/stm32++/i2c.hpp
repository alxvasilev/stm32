/**
 * original author:  Tilen Majerle<tilen@majerle.eu>
 * modification for STM32f10x: Alexander Lutsai<s.lyra@ya.ru>

   ----------------------------------------------------------------------
   	Copyright (C) Alexander Lutsai, 2016
    Copyright (C) Tilen Majerle, 2015

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
   ----------------------------------------------------------------------
 */
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
#include <stm32++/timeutl.h>
#include <stm32++/snprint.h>
#include <stm32++/semihosting.hpp>

/* Private definitions */
/** @brief Timeout for ACK of sent data. Used also for timeout for device
 * response in \c inDeviceConnected() */
enum { kTimeoutMs = 10 };

/* Private defines */
enum: bool { kTxMode = true, kRxMode = false,
             kAckEnable = true, kAckDisable = false };
template<uint32_t I2C>
struct I2CInfo;

template <uint32_t I2C>
class I2c
{
public:
void init(uint8_t ownAddr=0x15, bool fastMode=true)
{
    rcc_periph_clock_enable(RCC_GPIOB);
    I2CInfo<I2C> info;
    /* Set alternate functions for the SCL and SDA pins of I2C1. */
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
              GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,
              info.kPinSda | info.kPinScl);

    rcc_periph_clock_enable(info.clock());
    i2c_reset(I2C);
    /* Disable the I2C before changing any configuration. */
    i2c_peripheral_disable(I2C);
    i2c_set_clock_frequency(I2C, rcc_apb1_frequency / 1000000);

    /* 400KHz - I2C Fast Mode */
    if (fastMode)
    {
        i2c_set_fast_mode(I2C);
    /*
     * fclock for I2C is 36MHz APB2 -> cycle time 28ns, low time at 400kHz
     * incl trise -> Thigh = 1600ns; CCR = tlow/tcycle = 0x1C,9;
     * Datasheet suggests 0x1e.
     */
        i2c_set_ccr(I2C, 0x1e);
        i2c_set_dutycycle(I2C, I2C_CCR_DUTY_DIV2);
    /*
     * fclock for I2C is 36MHz -> cycle time 28ns, rise time for
     * 400kHz => 300ns and 100kHz => 1000ns; 300ns/28ns = 10;
     * Incremented by 1 -> 11.
     */
        i2c_set_trise(I2C, 11);
    }
    else //standard mode 100kHz
    {

        i2c_set_standard_mode(I2C);
        i2c_set_ccr(I2C1, 179);
        i2c_set_trise(I2C1, 36);
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

void sendBuf(uint8_t* data, uint16_t count)
{
    uint8_t* end = data+count;
    while (data < end)
    {
        sendByte(*(data++));
    }
    stop();
}

bool startWrite(uint8_t address, bool ack=false)
{
    return start(address, kTxMode, ack);
}
bool startRead(uint8_t address, bool ack=false)
{
    return start(address, kRxMode, ack);
}

/* Private functions */
bool start(uint8_t address, bool tx, bool ack)
{
	/* Generate I2C start pulse */
    i2c_send_start(I2C);
    /* Waiting for START to be sent and switched to master mode. */
    while (!(I2C_SR1(I2C) & I2C_SR1_SB));
    assert(I2C_SR2(I2C) & I2C_SR2_MSL);

    if (ack)
        i2c_enable_ack(I2C);
    else
        i2c_disable_ack(I2C);

    /* Send destination address. */
    i2c_send_7bit_address(I2C, address, tx ? I2C_WRITE : I2C_READ);

    ElapsedTimer timer;
    /* Waiting for address is transferred. */
    while (!(I2C_SR1(I2C) & I2C_SR1_ADDR))
    {
        if (timer.msElapsed() > kTimeoutMs)
            return false;
    }
    assert(!(I2C_SR1(I2C) & I2C_SR1_SB));

#ifndef NDEBUG
    if (tx)
        assert(I2C_SR2(I2C) & I2C_SR2_TRA);
    else
        assert(!(I2C_SR2(I2C) & I2C_SR2_TRA));
#endif

    assert((I2C_SR1(I2C) & I2C_SR1_ADDR) == 0);
    return true;
}

bool sendByte(uint8_t data)
{
//  tprintf("sendByte %\n", fmtNum<16>(data));
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
bool sendByte(uint8_t byte, Args... args)
{
    if (!sendByte(byte))
        return false;
    return sendByte(args...);
}
template <typename T>
bool send(T data)
{
//  tprintf("send %\n", fmtNum<16>(data));
    if (!(I2C_SR1(I2C) & I2C_SR1_TxE))
    {
        ElapsedTimer timer;
        while (!(I2C_SR1(I2C) & I2C_SR1_TxE))
        {
            if (timer.msElapsed() > kTimeoutMs)
                return false;
        }
    }
    uint8_t* end = ((uint8_t*)&data)+sizeof(data);
    for (uint8_t* ptr = (uint8_t*)&data; ptr<end; ptr++)
        i2c_send_data(I2C, *ptr);
    return true;
}

template <typename T, typename... Args>
bool send(T val, Args... args)
{
    bool ret = (sizeof(T) == 1) ? sendByte(val) : send(val);
    if (!ret)
        return false;
    return send(args...);
}

bool stop()
{
    //wait transfer complete
    while (!(I2C_SR1(I2C) & (I2C_SR1_BTF | I2C_SR1_TxE)));
    /* Send STOP condition. */
    i2c_send_stop(I2C);
    return 0;
}

bool isDeviceConnected(uint8_t address)
{
    /* Try to start, function will return 0 in case device will send ACK */
    bool connected = start(address, kTxMode, kAckEnable);
    stop();
    return connected;
}
};

template<>
struct I2CInfo<I2C1>
{
    enum: uint16_t { kPinScl = GPIO_I2C1_SCL, kPinSda = GPIO_I2C1_SDA };
    static constexpr rcc_periph_clken clock() { return RCC_I2C1; }
};
template<>
struct I2CInfo<I2C2>
{
    enum: uint16_t { kPinScl = GPIO_I2C2_SCL, kPinSda = GPIO_I2C2_SDA };
    static constexpr rcc_periph_clken clock() { return RCC_I2C2; }
};

