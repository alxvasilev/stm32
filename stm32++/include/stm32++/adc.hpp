/** @author Alexander Vassilev
 * @copyright BSD License
 */

#ifndef STM32PP_ADC_HPP
#define STM32PP_ADC_HPP

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/i2c.h>
#include <stm32++/usart.hpp>
#include <stm32++/timeutl.h>
using namespace usart;

static void gpio_setup(void)
{
	/* Enable GPIO clocks. */
	rcc_periph_clock_enable(RCC_GPIOC);

	/* Setup the LEDs. */
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ,
              GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
}

Usart<UsartTxDma<Usart1, DMA1>> gUsart;

void dma1_channel4_isr()
{
    gUsart.dmaTxIsr();
}

enum: uint16_t {
    kOptScanMode = 1,
    kOptContConv = 2,
    kOptUseDma = 4, kOptDmaDontEnableClock = 8,
    kOptDmaCircular = 16, kOptDmaNoDoneIntr = 32,
    kOptDmaDontEnableIrq = 64,
    kOptNoVref = 128,
    kOptNoCalibrate = 256
};

enum: uint8_t { kStateRunning = 1, kStateDataReady = 2 };
struct Clocks
{
protected:
    uint8_t mVal;
public:
    Clocks(uint8_t aVal): mVal(aVal){}
    uint8_t value() const { return mVal; }
};

template <uint32_t ADC>
constexpr rcc_periph_clken adcClock();

template <uint32_t ADC>
struct DmaInfoForAdc;

template<uint32_t ADC>
class Adc
{
public:
    typedef DmaInfoForAdc<ADC> DmaInfo;
protected:
    enum { kOptNotInitialized = 0x8000 };
    uint16_t mInitOpts = kOptNotInitialized;
    uint32_t mClockFreq = 0;
    volatile uint8_t mState = 0;
    uint32_t obtainClockFreq() const
    {
        uint32_t code = (RCC_CFGR & RCC_CFGR_ADCPRE) >> RCC_CFGR_ADCPRE_SHIFT;
        return rcc_apb2_frequency / codeToClockRatio(code);
    }
    enum { dma = DmaInfo::kDmaId };
    enum { chan = DmaInfo::kDmaChannel };
    template <typename T>
    uint8_t sampleNsTimeToCode(T nanosec)
    {
        static_assert(std::is_integral<T>::value);
        return sampleCyclesToCode(nanosec/(100000000/mClockFreq));
    }
    template <typename T>
    uint8_t sampleTimeToCode(T val) { return sampleNsTimeToCode(val); }
    uint8_t sampleTimeToCode(Clocks clocks) { return clocks.value(); }
    void enableVrefAsync()
    {
        static_assert(ADC == ADC1);
        /* We want to read the temperature sensor, so we have to enable it. */
        adc_enable_temperature_sensor();
        //17100 nanoseconds sample time required for temperature sensor
        setChanSampleTime(ADC_CHANNEL_TEMP, 18000);
        setChanSampleTime(ADC_CHANNEL_VREF, 18000);
    }
public:
    volatile uint8_t state() const { return mState; }
    template <typename T>
    void setChanSampleTime(uint8_t chan, T time)
    {
        adc_set_sample_time(ADC, chan, sampleTimeToCode(time));
    }
    uint32_t clockFreq() const { return mClockFreq; }
    bool isInitialized() const { return (mInitOpts & kOptNotInitialized) == 0; }
    void init(uint8_t opts, uint32_t adcFreq=12000000)
    {
        if (adcFreq)
        {
            assert(adcFreq <= 14000000);
            uint32_t ratio = ((rcc_apb2_frequency << 1) + adcFreq) / (adcFreq << 1);
            if (ratio & 1) //must be an even number
                ratio++;
            int8_t divCode = clockRatioToCode(ratio);
            if (divCode < 0) //could not find exact match
            {
                divCode = -divCode;
                ratio = codeToClockRatio(divCode);
            }
            mClockFreq = rcc_apb2_frequency / ratio;
            assert(mClockFreq <= adcFreq);
            rcc_set_adcpre(divCode);
        }
        else
        {
            mClockFreq = obtainClockFreq();
        }

        rcc_periph_clock_enable(adcClock<ADC>());
        /* Make sure the ADC doesn't run during config. */
        adc_power_off(ADC);
        adc_set_right_aligned(ADC);

        if (opts & kOptContConv)
        {
            adc_set_continuous_conversion_mode(ADC);
        }
        else
        {
            adc_set_single_conversion_mode(ADC);
        }
        if (opts & kOptScanMode)
        {
            adc_enable_scan_mode(ADC);
            assert(opts & kOptUseDma);
        }
        else
        {
            adc_disable_scan_mode(ADC);
        }

        mInitOpts = opts;
        if ((opts & kOptNoVref) == 0)
            enableVrefAsync();

        adc_power_on(ADC);
        usDelay((opts & kOptNoVref) ? 3 : 10);
        if ((opts & kOptNoCalibrate) == 0)
        {
            adc_reset_calibration(ADC);
            adc_calibrate(ADC);
        }
        if (opts & kOptUseDma)
             dmaInit();
    }
    void enableExtTrigRegular(uint32_t trig)
    {
        adc_enable_external_trigger_regular(ADC, trig);
    }
    template <class T>
    void setChannels(uint8_t* chans, uint8_t count, T smpTime)
    {
        adc_set_regular_sequence(ADC, count, chans);
        if (smpTime)
        {
            uint8_t timeCode = sampleTimeToCode(smpTime);
            uint32_t reg32 = timeCode;
            for (uint8_t i = 0; i < 7; i++)
            {
                smpTime <<= 3;
                reg32 |= smpTime;
            }
            ADC_SMPR1(ADC) = (ADC_SMPR1(ADC) & 0xf0000000) | reg32;
            smpTime <<= 3;
            reg32 |= smpTime;
            smpTime <<= 3;
            reg32 |= smpTime;
            ADC_SMPR2(ADC) = (ADC_SMPR2(ADC) & 0xc0000000) | reg32;
        }
    }
    template <typename T>
    void setChannels(uint8_t* chans, uint8_t count, T* smpTimes)
    {
        adc_set_regular_sequence(ADC, count, chans);
        for (uint8_t i = 0; i < count; i++)
            adc_set_sample_time(ADC, chans[i], sampleTimeToCode(smpTimes[i]));
    }
    template <typename T>
    void setChannels(uint8_t chan, T smpTime)
    {
        adc_set_sample_time(ADC, chan, sampleTimeToCode(smpTime));
        setSingleChannel(chan);
    }
    void start(uint32_t trig=ADC_CR2_EXTSEL_SWSTART)
    {
        mState = kStateRunning;
        if ((ADC_CR2(ADC) & ADC_CR2_ADON) == 0)
        {
            adc_power_on(ADC);
            usDelay(3);
        }
        adc_enable_external_trigger_regular(ADC, trig);
        if (trig == ADC_CR2_EXTSEL_SWSTART)
            adc_start_conversion_regular(ADC);
    }
    void start(void* buf, size_t bufsize, uint32_t trig=ADC_CR2_EXTSEL_SWSTART)
    {
        assert(mInitOpts & kOptUseDma);
        dmaStart(buf, bufsize);
        start(trig);
    }
    void stop()
    {
        adc_power_off(ADC);
        mState &= ~kStateRunning;
    }
    void dmaCompleteIsr()
    {
        DMA_IFCR(dma) |= DMA_IFCR_CTCIF(chan);
        stop();
        dmaStop();
        mState |= kStateDataReady;
    }
    void enableVref()
    {
        enableVrefAsync();
        usDelay(10);
    }
    void disableVref() { adc_disable_temperature_sensor(); }
protected:
    void dmaInit()
    {
        if ((mInitOpts & kOptDmaDontEnableClock) == 0)
        {
            rcc_periph_clock_enable(DmaInfo::clock());
        }

        dma_channel_reset(dma, chan);
        // Set mode: High priority, read from peripheral
        if (mInitOpts & kOptDmaCircular)
        {
            dma_enable_circular_mode(dma, chan);
        }
        else
        {
            if ((mInitOpts & kOptDmaNoDoneIntr) == 0) // Interrupt when transfer complete
            {
                dma_enable_transfer_complete_interrupt(dma, chan);
            }
        }
        dma_set_priority(dma, chan, DMA_CCR_PL_VERY_HIGH);
        dma_set_read_from_peripheral(dma, chan);
        dma_set_peripheral_address(dma, chan, (uint32_t)(&ADC_DR(ADC)));
        dma_set_peripheral_size(dma, chan, DMA_CCR_PSIZE_16BIT);
        dma_set_memory_size(dma, chan, DMA_CCR_MSIZE_16BIT);

        // Set increment mode
        dma_disable_peripheral_increment_mode(dma, chan);
        dma_enable_memory_increment_mode(dma, chan);
        if ((mInitOpts & kOptDmaDontEnableIrq) == 0)
        {
            nvic_set_priority(DmaInfo::kIrqNo, 0);
            nvic_enable_irq(DmaInfo::kIrqNo);
        }
    }
    void dmaStart(void* buf, size_t bufsize)
    {
        assert((bufsize & 1) == 0);
        assert((DMA_CCR(dma, chan) & DMA_CCR_EN) == 0);
        dma_set_number_of_data(dma, chan, bufsize/2);
        dma_set_memory_address(dma, chan, (uint32_t)buf);
        // Enable DMA channel
        dma_enable_transfer_complete_interrupt(dma, chan);
        dma_enable_channel(dma, chan);
        adc_enable_dma(ADC);
    }
    void dmaStop()
    {
        dma_disable_transfer_complete_interrupt(dma, chan);
        adc_disable_dma(ADC);
        dma_disable_channel(dma, chan);
    }
public:
    bool dmaBusy()
    {
        return (DMA_CCR(dma, chan) & DMA_CCR_EN);
    }
    int8_t clockRatioToCode(uint32_t ratio)
    {
        switch (ratio)
        {
        case 2: return 0;
        case 4: return 1;
        case 6: return 2;
        case 8: return 3;
        default: return -3;
        }
    }
    static uint8_t codeToClockRatio(uint8_t code)
    {
        switch (code)
        {
        case 0: return 2;
        case 1: return 4;
        case 2: return 6;
        case 3: return 8;
        default: assert(false); return 0; //silence no return warning
        }
    }
    static uint8_t sampleCyclesToCode(uint16_t cyclesx10)
    {
        if (cyclesx10 <= 15)
            return ADC_SMPR_SMP_1DOT5CYC;
        else if (cyclesx10 <= 75)
            return ADC_SMPR_SMP_7DOT5CYC;
        else if (cyclesx10 <= 135)
            return ADC_SMPR_SMP_13DOT5CYC;
        else if (cyclesx10 <= 285)
            return ADC_SMPR_SMP_28DOT5CYC;
        else if (cyclesx10 <= 415)
            return ADC_SMPR_SMP_41DOT5CYC;
        else if (cyclesx10 <= 555)
            return ADC_SMPR_SMP_55DOT5CYC;
        else if (cyclesx10 <= 715)
            return ADC_SMPR_SMP_71DOT5CYC;
        else
            return ADC_SMPR_SMP_239DOT5CYC;
    }
    void setSingleChannel(uint8_t chan)
    {
        assert(chan < 18);
        ADC_SQR1(ADC) = (ADC_SQR1(ADC) & ~ADC_SQR1_L_MSK) | (1 << ADC_SQR1_L_LSB);
        ADC_SQR3(ADC) = chan;
    }
    uint16_t convertSingle(uint8_t channel)
    {
        assert(isInitialized() &&
               ((mInitOpts & (kOptContConv|kOptScanMode)) == 0));
        setSingleChannel(channel);
        adc_start_conversion_direct(ADC);
        /* Wait for end of conversion. */
        while (!(adc_eoc(ADC)));
        return adc_read_regular(ADC);
    }
};

template <>
constexpr rcc_periph_clken adcClock<ADC1>() { return RCC_ADC1; }
template <>
constexpr rcc_periph_clken adcClock<ADC2>() { return RCC_ADC2; }

template<>
struct DmaInfoForAdc<ADC1>
{
    enum: uint32_t { kDmaId = DMA1 };
    enum: uint8_t { kDmaChannel = DMA_CHANNEL1 };
    enum: uint8_t { kIrqNo = NVIC_DMA1_CHANNEL1_IRQ };
    static constexpr rcc_periph_clken clock() { return RCC_DMA1; }
};
template<>
struct DmaInfoForAdc<ADC3>
{
    enum: uint32_t { kDmaId = DMA2 };
    enum: uint8_t { kDmaChannel = DMA_CHANNEL5 };
    enum: uint8_t { kIrqNo = NVIC_DMA2_CHANNEL5_IRQ };
    static constexpr rcc_periph_clken clock() { return RCC_DMA2; }
};

/** @brief CLock setup for maximum ADC sample rate of 1 MHz.
 *  Based on libopencm3 clock setup function for CPU clock at 72 MHz
 */
void rcc_clock_setup_in_hse_8mhz_out_56mhz()
{
    /* Enable internal high-speed oscillator. */
    rcc_osc_on(RCC_HSI);
    rcc_wait_for_osc_ready(RCC_HSI);

    /* Select HSI as SYSCLK source. */
    rcc_set_sysclk_source(RCC_CFGR_SW_SYSCLKSEL_HSICLK);

    /* Enable external high-speed oscillator 8MHz. */
    rcc_osc_on(RCC_HSE);
    rcc_wait_for_osc_ready(RCC_HSE);
    rcc_set_sysclk_source(RCC_CFGR_SW_SYSCLKSEL_HSECLK);

    /*
     * Set prescalers for AHB, ADC, ABP1, ABP2.
     * Do this before touching the PLL (TODO: why?).
     */
    rcc_set_hpre(RCC_CFGR_HPRE_SYSCLK_NODIV);    /* Set. 56MHz Max. 72MHz */
    rcc_set_adcpre(RCC_CFGR_ADCPRE_PCLK2_DIV4);  /* Set. 14MHz Max. 14MHz */
    rcc_set_ppre1(RCC_CFGR_PPRE1_HCLK_DIV2);     /* Set. 28MHz Max. 36MHz */
    rcc_set_ppre2(RCC_CFGR_PPRE2_HCLK_NODIV);    /* Set. 56MHz Max. 72MHz */

    /*
     * Sysclk runs with 72MHz -> 2 waitstates.
     * 0WS from 0-24MHz
     * 1WS from 24-48MHz
     * 2WS from 48-72MHz
     */

    flash_set_ws(FLASH_ACR_LATENCY_2WS);

    /*
     * Set the PLL multiplication factor to 9.
     * 8MHz (external) * 9 (multiplier) = 72MHz
     */
    rcc_set_pll_multiplication_factor(RCC_CFGR_PLLMUL_PLL_CLK_MUL7);

    /* Select HSE as PLL source. */
    rcc_set_pll_source(RCC_CFGR_PLLSRC_HSE_CLK);

    /*
     * External frequency undivided before entering PLL
     * (only valid/needed for HSE).
     */
    rcc_set_pllxtpre(RCC_CFGR_PLLXTPRE_HSE_CLK);

    /* Enable PLL oscillator and wait for it to stabilize. */
    rcc_osc_on(RCC_PLL);
    rcc_wait_for_osc_ready(RCC_PLL);

    /* Select PLL as SYSCLK source. */
    rcc_set_sysclk_source(RCC_CFGR_SW_SYSCLKSEL_PLLCLK);

    /* Set the peripheral clock frequencies used */
    rcc_ahb_frequency = 56000000;
    rcc_apb1_frequency = 28000000;
    rcc_apb2_frequency = 56000000;
}
#endif
