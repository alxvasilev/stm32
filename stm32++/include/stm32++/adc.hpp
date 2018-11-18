/** @author Alexander Vassilev
 * @copyright BSD License
 */

/*TODO:
 - Implement adc resulution and word width selection
 - Interleaved mode
*/

#ifndef STM32PP_ADC_HPP
#define STM32PP_ADC_HPP

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/flash.h> //needed for the custom system clocks setup that allows max sample rate
#include <libopencm3/stm32/i2c.h>
#include <stm32++/dma.hpp>
#include <stm32++/timeutl.hpp>
#include <stm32++/common.hpp>
#include <stm32++/xassert.hpp>

//#define ADC_ENABLE_DEBUG
#ifdef ADC_ENABLE_DEBUG
#define ADC_LOG_DEBUG(fmt,...) tprintf("adc: " fmt "\n", ##__VA_ARGS__)
#else
#define ADC_LOG_DEBUG(fmt,...)
#endif

namespace nsadc
{
enum: uint16_t {
    kOptScanMode = 1,
    kOptContConv = 2,
    kOptNoVref = 4,
    kOptNoCalibrate = 8
};

// To differentiate the type of value when passing to setChannels()
struct ClockCnt
{
public:
    uint8_t value;
    ClockCnt(uint8_t aVal): value(aVal){}
};
struct NanoTime
{
public:
    uint8_t value;
    NanoTime(uint8_t aVal): value(aVal){}
};


template<uint32_t ADC>
class AdcNoDma: public PeriphInfo<ADC>
{
protected:
    typedef AdcNoDma<ADC> Self;
    enum { kOptNotInitialized = 0x8000 };
    uint16_t mInitOpts = kOptNotInitialized;
    uint32_t mClockFreq = 0;
    uint32_t currentClockFreq() const
    {
        uint32_t code = (RCC_CFGR & RCC_CFGR_ADCPRE) >> RCC_CFGR_ADCPRE_SHIFT;
        return rcc_apb2_frequency / codeToClockRatio(code);
    }
    uint8_t sampleNanosecToCode(uint32_t nanosec)
    {
        return sampleCyclesToCode(nanosec/(1000000000/mClockFreq));
    }
    uint8_t sampleFreqToCode(uint32_t freq)
    {
        return sampleCyclesToCode(mClockFreq / freq); //must multiply cycles x10
    }
    uint8_t sampleTimeFreqToCode(uint32_t freq) { return sampleFreqToCode(freq); }
    uint8_t sampleTimeFreqToCode(ClockCnt clocks) { return sampleCyclesToCode(clocks.value()); } //Clock is x10, because it contains .5 units
    uint8_t sampleTimeFreqToCode(NanoTime nano) { return sampleNanosecToCode(nano.value()); }

    void enableVrefAsync()
    {
        static_assert(ADC == ADC1, "ADC device is not ADC1");
        /* We want to read the temperature sensor, so we have to enable it. */
        adc_enable_temperature_sensor();
        //17100 nanoseconds sample time required for temperature sensor
        setChanSampleTime(ADC_CHANNEL_TEMP, 18000);
        setChanSampleTime(ADC_CHANNEL_VREF, 18000);
        ADC_LOG_DEBUG("Enabled reference and temperature channels");
    }
public:
    uint32_t clockFreq() const { return mClockFreq; }
    bool isInitialized() const { return (mInitOpts & kOptNotInitialized) == 0; }
    void init(uint8_t opts, uint32_t adcClockFreq=12000000)
    {
        ADC_LOG_DEBUG("Initializing with options %", opts);
        xassert(adcClockFreq > 0 && adcClockFreq <= 14000000);
        // calculate rounded ratio - adc is clocked by dividing apb2 clock
        uint32_t ratio = ((rcc_apb2_frequency << 1) + adcClockFreq) / (adcClockFreq << 1);
        if (ratio & 1) //must be an even number
        {
            ratio++;
        }
        int8_t divCode = clockRatioToCode(ratio);
        if (divCode < 0) //could not find exact match
        {
            divCode = -divCode;
            ratio = codeToClockRatio(divCode);
        }
        rcc_periph_clock_enable(Self::kClockId);
        ADC_LOG_DEBUG("Enabled clock");

        /* Make sure the ADC doesn't run during config. */
        adc_power_off(ADC);
        rcc_periph_reset_pulse(Self::kResetBit);
        rcc_set_adcpre(divCode);
        mClockFreq = currentClockFreq();

        adc_set_right_aligned(ADC);
        adc_set_dual_mode(ADC_CR1_DUALMOD_IND);

        if (opts & kOptContConv)
        {
            adc_set_continuous_conversion_mode(ADC);
            ADC_LOG_DEBUG("Set continuous conversion mode");
        }
        else
        {
            adc_set_single_conversion_mode(ADC);
            ADC_LOG_DEBUG("Set single conversion mode");
        }
        if (opts & kOptScanMode)
        {
            adc_enable_scan_mode(ADC);
            ADC_LOG_DEBUG("Set scan mode");

        }
        else
        {
            adc_disable_scan_mode(ADC);
            ADC_LOG_DEBUG("Disabled scan mode");
        }

        mInitOpts = opts & ~kOptNotInitialized;
        if ((opts & kOptNoVref) == 0)
        {
            enableVrefAsync();
        }
        usDelay((opts & kOptNoVref) ? 3 : 10);
        ADC_LOG_DEBUG("Init complete: requested clock: %Hz, actual clock: = %Hz", adcClockFreq, mClockFreq);
    }
    void enableExtTrigRegular(uint32_t trig)
    {
        adc_enable_external_trigger_regular(ADC, trig);
    }
    template <class T>
    void setChannels(uint8_t* chans, uint8_t count, T timeFreq)
    {
        adc_set_regular_sequence(ADC, count, chans);
        uint8_t code = sampleTimeFreqToCode(timeFreq);
        for (uint8_t i = 0; i < count; i++)
        {
            adc_set_sample_time(ADC, chans[i], code);
        }
    }
    template <typename T>
    void setChannels(uint8_t* chans, uint8_t count, T* timeFreqs)
    {
        adc_set_regular_sequence(ADC, count, chans);
        for (uint8_t i = 0; i < count; i++)
        {
            adc_set_sample_time(ADC, chans[i], sampleTimeFreqToCode(timeFreqs[i]));
        }
    }
    /**
     * Sets the sampling time for a channel. If the time is specified as an
     * integer, it is treated as nanoseconds. If it is a Clocks value, then
     * the sample time is set to this number of ADC clocks. The same is valid
     * for all versions of the setChannel() method
     */
    template <typename T>
    uint8_t setChanSampleTime(uint8_t chan, T timeFreq)
    {
        uint8_t code = sampleTimeFreqToCode(timeFreq);
        adc_set_sample_time(ADC, chan, code);
        return code;
    }
    uint32_t sampleTimeCodeToFreq(uint8_t code)
    {
        auto cycles = codeToSampleCycles(code);
        return mClockFreq / cycles;
    }
    uint32_t sampleTimeCodeToNs(uint8_t code)
    {
        return (1000000000 / mClockFreq) * codeToSampleCycles(code);
    }
    bool isRunning() const { return (ADC_CR2(ADC) & ADC_CR2_ADON) != 0; }
    void powerOn(uint32_t trig=ADC_CR2_EXTSEL_SWSTART)
    {
        xassert(!isRunning());
        adc_enable_external_trigger_regular(ADC, trig);
        ADC_LOG_DEBUG("Enabled external trigger %", fmtHex(trig));
        adc_power_on(ADC);
        ADC_LOG_DEBUG("Powered on");
        //at least 2 clock cycles after power on, before calibration
        uint32_t dly = 4000000000 / mClockFreq;
        usDelay(dly);
        adc_reset_calibration(ADC);
        adc_calibrate(ADC);
        usDelay(dly);
        ADC_LOG_DEBUG("Calibrated");
    }
    void start(uint32_t trig=ADC_CR2_EXTSEL_SWSTART)
    {
        if (!isRunning())
        {
            powerOn();
        }
        if (trig == ADC_CR2_EXTSEL_SWSTART)
        {
            adc_start_conversion_regular(ADC); //sets ADC_CR2_SWSTART
            ADC_LOG_DEBUG("Started conversion by software");
        }
    }
    void powerOff()
    {
        adc_power_off(ADC);
    }
    void enableVref()
    {
        enableVrefAsync();
        usDelay(10);
    }
    void disableVref()
    {
        adc_disable_temperature_sensor();
        ADC_LOG_DEBUG("Disabled reference and temperature channels");
    }
protected:
    void dmaStartPeripheralRx(uint32_t trig=ADC_CR2_EXTSEL_SWSTART)
    {
        adc_enable_dma(ADC);
        ADC_LOG_DEBUG("Enabled DMA");
        start(trig);
    }
    void dmaStopPeripheralRx()
    {
        powerOff();
        adc_disable_dma(ADC);
    }
public:
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
        default: xassert(false); return 0; //silence no return warning
        }
    }
    static uint8_t sampleCyclesToCode(int16_t cycles)
    {
        // Total conversion time = sample_time + 12.5 cycles
        // sample_time is what we set in the register, and cycles is the
        // total conversion time
        cycles = (cycles * 10) - 125;
        if (cycles <= 15)
            return ADC_SMPR_SMP_1DOT5CYC;
        else if (cycles <= 75)
            return ADC_SMPR_SMP_7DOT5CYC;
        else if (cycles <= 135)
            return ADC_SMPR_SMP_13DOT5CYC;
        else if (cycles <= 285)
            return ADC_SMPR_SMP_28DOT5CYC;
        else if (cycles <= 415)
            return ADC_SMPR_SMP_41DOT5CYC;
        else if (cycles <= 555)
            return ADC_SMPR_SMP_55DOT5CYC;
        else if (cycles <= 715)
            return ADC_SMPR_SMP_71DOT5CYC;
        else
            return ADC_SMPR_SMP_239DOT5CYC;
    }
    static uint16_t codeToSampleCycles(uint8_t code)
    {
        // total sample cycles are sample_time + 12.5
        switch (code)
        {
            case ADC_SMPR_SMP_1DOT5CYC: return 14;
            case ADC_SMPR_SMP_7DOT5CYC: return 20;
            case ADC_SMPR_SMP_13DOT5CYC: return 26;
            case ADC_SMPR_SMP_28DOT5CYC: return 41;
            case ADC_SMPR_SMP_41DOT5CYC: return 54;
            case ADC_SMPR_SMP_55DOT5CYC: return 68;
            case ADC_SMPR_SMP_71DOT5CYC: return 84;
            case ADC_SMPR_SMP_239DOT5CYC: return 252;
            default: __builtin_trap();
        }
    }
    template<class T>
    uint8_t useSingleChannel(uint8_t chan, T timeFreq)
    {
        xassert(chan < 18);
//        assert(isInitialized() &&
//            ((mInitOpts & (kOptContConv|kOptScanMode)) == 0));
        uint8_t code = sampleTimeFreqToCode(timeFreq);
        adc_set_regular_sequence(ADC, 1, &chan);
        adc_set_sample_time(ADC, chan, code);
        ADC_LOG_DEBUG("useSingleChannel: chan %, sample freq: %Hz (%ns, code: %)",
            chan, sampleTimeCodeToFreq(code), sampleTimeCodeToNs(code), code);
        return code;
    }
    uint16_t convertSingle()
    {
        adc_start_conversion_direct(ADC);
        /* Wait for end of conversion. */
        while (!(adc_eoc(ADC)));
        return adc_read_regular(ADC);
    }
};



/** @brief Clock setup for maximum ADC sample rate of 1 MHz.
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

template <uint32_t ADC>
class Adc: public dma::Rx<AdcNoDma<ADC>, dma::kAllMaxPrio>
{
};
}

template<>
struct PeriphInfo<ADC1>
{
    static constexpr rcc_periph_clken kClockId = RCC_ADC1;
    enum: uint32_t { kDmaRxId = DMA1, kDmaRxDataRegister = (uint32_t)(&ADC1_DR) };
    enum: uint8_t { kDmaRxChannel = DMA_CHANNEL1, kDmaWordSize = 2 };
    static constexpr rcc_periph_rst kResetBit = RST_ADC1;
};
template<>
struct PeriphInfo<ADC2>
{
    static constexpr rcc_periph_clken kClockId = RCC_ADC2;
    static constexpr rcc_periph_rst kResetBit = RST_ADC2;
    // ADC2 has no own DMA support.
};

template<>
struct PeriphInfo<ADC3>
{
    static constexpr rcc_periph_clken kClockId = RCC_ADC3;
    enum: uint32_t { kDmaRxId = DMA2, kDmaRxDataRegister = (uint32_t)(&ADC3_DR) };
    enum: uint8_t { kDmaRxChannel = DMA_CHANNEL5, kDmaWordSize = 2 };
    static constexpr rcc_periph_rst kResetBit = RST_ADC3;
};

#endif
