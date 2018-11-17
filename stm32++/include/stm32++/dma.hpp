/** @author Alexander Vassilev
 *  @copyright BSD Licence
 *
 */

#ifndef DMA_HPP
#define DMA_HPP
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/dma.h>
#include <assert.h>
#include "snprint.hpp"
#include "common.hpp"

#define DMA_LOG_DEBUG(fmt,...) tprintf("dma: " fmt "\n", ##__VA_ARGS__)

namespace dma
{
constexpr uint32_t periphSizeCode(uint8_t size)
{
    switch(size)
    {
        case 8: return DMA_CCR_PSIZE_8BIT;
        case 16: return DMA_CCR_PSIZE_16BIT;
        case 32: return DMA_CCR_PSIZE_32BIT;
        default: __builtin_trap();
    }
}
constexpr uint32_t memSizeCode(uint8_t size)
{
    switch(size)
    {
        case 8: return DMA_CCR_MSIZE_8BIT;
        case 16: return DMA_CCR_MSIZE_16BIT;
        case 32: return DMA_CCR_MSIZE_32BIT;
        default: __builtin_trap();
    }
}
enum: uint8_t {
    // DMA channel priority
    kPrioMask           = 0x3, //bits 0 and 1 denote DMA interrupt priority
    kPrioShift          = 0,
    kPrioVeryHigh       = 3,
    kPrioHigh           = 2,
    kPrioMedium         = 1,
    kPrioLow            = 0,
    // DMA IRQ priority
    kIrqPrioMask        = 0x3 << 2,
    kIrqPrioShift       = 2,
    kIrqPrioVeryHigh    = 0 << 2,
    kIrqPrioHigh        = 1 << 2,
    kIrqPrioMedium      = 2 << 2,
    kIrqPrioLow         = 3 << 2,
    kDmaDontEnableClock = 0x10, // In case the DMA controller clock is already enabled
    kDmaNoDoneIntr      = 0x20, // Disable dma complete interrupt
    kDmaCircularMode    = 0x40,
    kDefaultOpts = kIrqPrioMedium | kPrioMedium,
    kAllMaxPrio = kPrioVeryHigh | kIrqPrioVeryHigh
};

bool dmaChannelIsBusy(uint32_t dma, uint8_t chan)
{
    return (DMA_CCR(dma, chan) & DMA_CCR_EN);
}

/** Mixin to support Tx DMA. Base is a peripheral, which is derived from
 *  PeriphInfo<Periph>, where Periph is the actual peripheral id (such as ADC1)
 *  for which DMA is to be supported. No method should conflict with one in dma::Rx
 */
template <class Base, uint8_t Opts=kDefaultOpts>
class Tx: public Base, private PeriphInfo<Base::kDmaTxId>
{
private:
    typedef Tx<Base, Opts> Self;
    typedef void(*FreeFunc)(void*);
    volatile bool mTxBusy = false;
    volatile const void* mTxBuf = nullptr;
    volatile FreeFunc mFreeFunc = nullptr;
protected:
    enum: uint8_t { kDmaTxIrq = Self::dmaIrqForChannel(Base::kDmaTxChannel) };
public:
    template <typename... Args>
    void dmaTxInit(Args... args)
    {
        Base::init(args...);
        DMA_LOG_DEBUG("Initializing %, channel %, irq % for Tx with opts: %",
            Self::deviceName(), Base::kDmaTxChannel, kDmaTxIrq, fmtHex(Opts));
        nvic_set_priority(kDmaTxIrq, (Opts & kIrqPrioMask) >> kIrqPrioShift);
        nvic_enable_irq(kDmaTxIrq);
        if ((Opts & kDmaDontEnableClock) == 0)
        {
            rcc_periph_clock_enable(Self::kDmaClockId);
            DMA_LOG_DEBUG("Enabled clock for %", Self::deviceName());
        }
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
    template <typename... Args>
    void dmaTxStart(const void* data, uint16_t size, FreeFunc freeFunc, Args... args)
    {
        enum: uint8_t { chan = Self::kDmaTxChannel };
        enum: uint32_t { dma = Self::dmaId };
        while(mTxBusy);
        mTxBusy = true;
        mTxBuf = data;
        mFreeFunc = freeFunc;

        dma_channel_reset(dma, chan);
        dma_set_peripheral_address(dma, chan, Self::dataRegister());
        dma_set_memory_address(dma, chan, (uint32_t)data);
        dma_set_number_of_data(dma, chan, size);
        dma_set_read_from_memory(dma, chan);
        dma_enable_memory_increment_mode(dma, chan);
        dma_disable_peripheral_increment_mode(dma, chan);
        dma_set_peripheral_size(dma, chan, periphSizeCode(Self::kDmaWordSize));
        dma_set_memory_size(dma, chan, memSizeCode(Self::kDmaWordSize));
        dma_set_priority(dma, chan, ((Opts & kPrioMask) >> kPrioShift) << DMA_CCR_PL_SHIFT);
        dma_enable_transfer_complete_interrupt(dma, chan);
        dma_enable_channel(dma, chan);
        //have to enable DMA for peripheral at the upper level and the transfer should start
        Base::dmaStartPeripheralTx(args...);
    }
    volatile bool txBusy() const { return mTxBusy; }
    void dmaTxIsr()
    {
        // check if transfer complete flag is set
        // if it is not set, assume something is wrong and bail out
        if ((DMA_ISR(Base::kDmaTxId) & DMA_ISR_TCIF(Base::kDmaTxChannel)) == 0)
            return;

        // clear flag
        DMA_IFCR(Base::kDmaTxId) |= DMA_IFCR_CTCIF(Base::kDmaTxChannel);
        dmaTxStop();
    }
    void dmaTxStop() // this is called from an ISR
    {
        dma_disable_transfer_complete_interrupt(Base::kDmaTxId, Base::kDmaTxChannel);
        Base::dmaStopPeripheralTx();
        dma_disable_channel(Base::kDmaTxId, Base::kDmaTxChannel);
        assert(mTxBuf);
        //FIXME: mFreeFunc may not be reentrant
        if (mFreeFunc)
        {
            mFreeFunc((void*)mTxBuf);
        }
        mTxBuf = nullptr;
        mTxBusy = false;
    }
};
/** Mixin to support Rx DMA. Base is derived from DmaInfo<Periph>,
 * where Periph is the actual peripheral for which DMA is to be supported
 */
template <class Base, uint8_t Opts=kDefaultOpts>
class Rx: public Base, private PeriphInfo<Base::kDmaRxId>
{
private:
    volatile bool mRxBusy = false;
    typedef Rx<Base, Opts> Self;
public:
    enum: uint8_t { kDmaRxIrq = Self::dmaIrqForChannel(Self::kDmaRxChannel) };
    volatile bool dmaRxBusy() const { return mRxBusy; }
    template<typename... Args>
    void init(Args... args)
    {
        Base::init(args...);
        DMA_LOG_DEBUG("Initializing %, channel %, irq % for Rx with opts: %",
            Self::deviceName(), (int)Base::kDmaRxChannel, (int)kDmaRxIrq, fmtHex(Opts));

        enum: uint8_t { chan = Base::kDmaRxChannel };
        enum: uint32_t { dma = Base::kDmaRxId };

        nvic_set_priority(Self::kDmaRxIrq, (Opts & kIrqPrioMask) >> kIrqPrioShift);
        if ((Opts & kDmaDontEnableClock) == 0)
        {
            rcc_periph_clock_enable(Self::kDmaClockId);
            DMA_LOG_DEBUG("Enabled clock for %", Self::deviceName());
        }
    }
    template <typename... Args>
    void dmaRxStart(const void* data, uint16_t size, Args... args)
    {
        enum: uint32_t { dma = Base::kDmaRxId };
        enum: uint8_t { chan = Base::kDmaRxChannel };
        while(mRxBusy);
        mRxBusy = true;
        dma_channel_reset(dma, chan);
        dma_set_peripheral_address(dma, chan, (uint32_t)Base::kDmaRxDataRegister);
        dma_set_memory_address(dma, chan, (uint32_t)data);
        dma_set_number_of_data(dma, chan, size);
        dma_set_read_from_peripheral(dma, chan);
        dma_enable_memory_increment_mode(dma, chan);
        dma_disable_peripheral_increment_mode(dma, chan);
        dma_set_peripheral_size(dma, chan, periphSizeCode(Base::kDmaWordSize));
        dma_set_memory_size(dma, chan, memSizeCode(Base::kDmaWordSize));
        dma_set_priority(dma, chan, ((Opts & kPrioMask) << kPrioShift) << DMA_CCR_PL_SHIFT);
        if (Opts & kDmaCircularMode)
        {
            dma_enable_circular_mode(dma, chan);
        }
        else if ((Opts & kDmaNoDoneIntr) == 0) // Interrupt when transfer complete
        {
            dma_enable_transfer_complete_interrupt(dma, chan);
        }

        dma_enable_transfer_complete_interrupt(dma, chan);
        nvic_enable_irq(Self::kDmaRxIrq);
        dma_enable_channel(dma, chan);
        Base::dmaStartPeripheralRx(args...);
    }
    void dmaRxIsr()
    {
        if ((DMA_ISR(Base::kDmaRxId) & DMA_ISR_TCIF(Self::kDmaRxChannel)) == 0)
            return;

        DMA_IFCR(Base::kDmaRxId) |= DMA_IFCR_CTCIF(Self::kDmaRxChannel);
        dmaRxStop();
    }
    void dmaRxStop()
    {
        dma_disable_transfer_complete_interrupt(Base::kDmaRxId, Base::kDmaRxChannel);
        Base::dmaStopPeripheralRx();
        dma_disable_channel(Base::kDmaRxId, Base::kDmaRxChannel);
        mRxBusy = false;
    }
};
}

/** Peripheral definitions */
template<>
struct PeriphInfo<DMA1>
{
    enum: uint32_t { kDmaId = DMA1 };
    static constexpr const char* deviceName() { return "DMA1"; }
    static constexpr rcc_periph_clken kDmaClockId = RCC_DMA1;
    static constexpr uint8_t dmaIrqForChannel(const uint8_t chan)
    {
        switch (chan)
        {
            case 1: return NVIC_DMA1_CHANNEL1_IRQ;
            case 2: return NVIC_DMA1_CHANNEL2_IRQ;
            case 3: return NVIC_DMA1_CHANNEL3_IRQ;
            case 4: return NVIC_DMA1_CHANNEL4_IRQ;
            case 5: return NVIC_DMA1_CHANNEL5_IRQ;
            case 6: return NVIC_DMA1_CHANNEL6_IRQ;
            case 7: return NVIC_DMA1_CHANNEL7_IRQ;
            default: __builtin_trap();
        }
    }
};

template<>
struct PeriphInfo<DMA2>
{
    enum: uint32_t { kDmaId = DMA2 };
    static constexpr rcc_periph_clken kDmaClockId = RCC_DMA2;
    static constexpr const char* deviceName() { return "DMA2"; }
    static constexpr uint8_t dmaIrqForChannel(const uint8_t chan)
    {
        switch (chan)
        {
            case 1: return NVIC_DMA2_CHANNEL1_IRQ;
            case 2: return NVIC_DMA2_CHANNEL2_IRQ;
            case 3: return NVIC_DMA2_CHANNEL3_IRQ;
            case 4: return NVIC_DMA2_CHANNEL4_5_IRQ;
            case 5: return NVIC_DMA2_CHANNEL5_IRQ;
            default: __builtin_trap();
        }
    }
};
TYPE_SUPPORTS(HasTxDma, T::dmaTxStop);
TYPE_SUPPORTS(HasRxDma, T::dmaRxStop);

#endif // DMA_HPP
