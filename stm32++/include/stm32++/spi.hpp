/**
 * @author Alexander Vassilev
 * @copyright BSD License
 */

#ifndef STM32PP_SPI_H
#define STM32PP_SPI_H

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/spi.h>
#include<stm32++/common.hpp>
#include<stm32++/tprintf.hpp>
namespace nsspi
{
struct Baudrate
{
    uint32_t mRate;
    Baudrate(uint32_t rate): mRate(rate) {}
};

uint8_t clockRatioToCode(uint8_t ratio);
uint32_t clockPrescaler(Baudrate rate, uint32_t apbFreq)
{
    return clockRatioToCode((apbFreq + rate.mRate - 1) / rate.mRate); // round up
}
uint32_t clockPrescaler(uint8_t ratio, uint32_t apbFreq)
{
    return clockRatioToCode(ratio);
}

enum: uint32_t
{
    kDisableOutput = 1,
    kDisableInput = 2,
    kHardwareNSS = 4,
    kSoftwareNSS = 0,
    kIdleClockIsLow = 8,
    kIdleClockIsHigh = 0,
    k16BitFrame = 16,
    k8BitFrame = 0,
    kFirstClockTransition = 32,
    kSecondClockTransition = 0,
    kLsbFirst = 64,
    kMsbFirst = 0
};

template <uint32_t SPI, bool Remap=false>
class SpiMaster: public PeriphInfo<SPI, Remap>
{
public:
    template <class S>
    void init(S speed, uint32_t config)
    {
        rcc_periph_clock_enable(this->kClockId);
        rcc_periph_clock_enable(PeriphInfo<this->kPortId>::kClockId);

        uint32_t outputPins = this->kPinSck;
        if ((config & kDisableOutput) == 0) {
            outputPins |= this->kPinMosi;
        }
        gpio_set_mode(this->kPortId, GPIO_MODE_OUTPUT_50_MHZ,
            GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, outputPins);

        if (config & kHardwareNSS) {
            gpio_set_mode(this->kPortId, GPIO_MODE_OUTPUT_50_MHZ,
                GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, this->kPinNss);
            spi_enable_ss_output(SPI);
        }

        if ((config & kDisableInput) == 0) {
            gpio_set_mode(this->kPortId, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT,
                this->kPinMiso);
        }
        /* Reset SPI, SPI_CR1 register cleared, SPI is disabled */
        spi_reset(SPI);

        spi_init_master(SPI, clockPrescaler(speed, this->apbFreq()),
            (config & kIdleClockIsLow) ? SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE : SPI_CR1_CPOL_CLK_TO_1_WHEN_IDLE,
            (config & kFirstClockTransition) ? SPI_CR1_CPHA_CLK_TRANSITION_1 : SPI_CR1_CPHA_CLK_TRANSITION_2,
            (config & k16BitFrame) ? SPI_CR1_DFF_16BIT : SPI_CR1_DFF_8BIT,
            (config & kLsbFirst) ? SPI_CR1_LSBFIRST : SPI_CR1_MSBFIRST);

        spi_enable(SPI);
    }
    void send(uint16_t data)
    {
        spi_send(SPI, data);
    }
    uint16_t recv()
    {
        return spi_read(SPI);
    }
    bool isBusy() const { return (SPI_SR(SPI) & SPI_SR_BSY) != 0; }
    void waitComplete() const { while (SPI_SR(SPI) & SPI_SR_BSY); }
    uint8_t dmaWordSize() const
    {
        return ((SPI_CR1(SPI) & SPI_CR1_DFF) == SPI_CR1_DFF_16BIT) ? 2 : 1;
    }
    void dmaStartPeripheralTx() { spi_enable_tx_dma(SPI); }
    void dmaStartPeripheralRx() { spi_enable_rx_dma(SPI); }
    //WARNING: The dmaStopPerpheralXX can be called from an ISR
    void dmaStopPeripheralTx()
    {
        waitComplete();
        spi_disable_tx_dma(SPI);
        this->stop();
    }
    void dmaStopPeripheralRx()
    {
        waitComplete();
        spi_disable_rx_dma(SPI);
    }
};

uint8_t clockRatioToCode(uint8_t ratio)
{
    if (ratio <= 2) {
        return SPI_CR1_BAUDRATE_FPCLK_DIV_2;
    } else if (ratio <= 4) {
        return SPI_CR1_BAUDRATE_FPCLK_DIV_4;
    } else if (ratio <= 8) {
        return SPI_CR1_BAUDRATE_FPCLK_DIV_8;
    } else if (ratio <= 16) {
        return SPI_CR1_BAUDRATE_FPCLK_DIV_16;
    } else if (ratio <= 32) {
        return SPI_CR1_BAUDRATE_FPCLK_DIV_32;
    } else if (ratio <= 64) {
        return SPI_CR1_BAUDRATE_FPCLK_DIV_64;
    } else if (ratio <= 128) {
        return SPI_CR1_BAUDRATE_FPCLK_DIV_128;
    } else {
        return SPI_CR1_BAUDRATE_FPCLK_DIV_256;
    }
}

uint16_t codeToClockRatio(uint8_t code)
{
    switch (code)
    {
        case SPI_CR1_BAUDRATE_FPCLK_DIV_2: return 2;
        case SPI_CR1_BAUDRATE_FPCLK_DIV_4: return 4;
        case SPI_CR1_BAUDRATE_FPCLK_DIV_8: return 8;
        case SPI_CR1_BAUDRATE_FPCLK_DIV_16: return 16;
        case SPI_CR1_BAUDRATE_FPCLK_DIV_32: return 32;
        case SPI_CR1_BAUDRATE_FPCLK_DIV_64: return 64;
        case SPI_CR1_BAUDRATE_FPCLK_DIV_128: return 128;
        case SPI_CR1_BAUDRATE_FPCLK_DIV_256: return 256;
        default: __builtin_trap();
    }
}
}

STM32PP_PERIPH_INFO(SPI1)
    enum: uint32_t { kPortId = GPIOA };
    enum: uint16_t { kPinSck = GPIO_SPI1_SCK, kPinNss = GPIO_SPI1_NSS,
                     kPinMosi = GPIO_SPI1_MOSI, kPinMiso = GPIO_SPI1_MISO };
    static constexpr rcc_periph_clken kClockId = RCC_SPI1;
    static uint32_t apbFreq() { return rcc_apb2_frequency; }
// DMA info
    enum: uint32_t { kDmaTxId = DMA1, kDmaRxId = DMA1 };
    enum: uint8_t {
        kDmaTxChannel = DMA_CHANNEL3,
        kDmaRxChannel = DMA_CHANNEL2
    };
    static const uint32_t dmaRxDataRegister() { return (uint32_t)(&SPI1_DR); }
    static const uint32_t dmaTxDataRegister() { return (uint32_t)(&SPI1_DR); }
};

STM32PP_PERIPH_INFO(SPI2)
    enum: uint32_t { kPortId = GPIOB };
    enum: uint16_t { kPinSck = GPIO_SPI2_SCK, kPinNss = GPIO_SPI2_NSS,
                     kPinMosi = GPIO_SPI2_MOSI, kPinMiso = GPIO_SPI2_MISO };
    static constexpr rcc_periph_clken kClockId = RCC_SPI2;
    static uint32_t apbFreq() { return rcc_apb1_frequency; }

    enum: uint32_t { kDmaTxId = DMA1, kDmaRxId = DMA1 };
    enum: uint8_t {
        kDmaTxChannel = DMA_CHANNEL5,
        kDmaRxChannel = DMA_CHANNEL4
    };
    static const uint32_t dmaRxDataRegister() { return (uint32_t)(&SPI2_DR); }
    static const uint32_t dmaTxDataRegister() { return (uint32_t)(&SPI2_DR); }
};

#endif
