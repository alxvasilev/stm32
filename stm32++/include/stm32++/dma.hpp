/** @author Alexander Vassilev
 *  @copyright BSD Licence
 *
 */

#ifndef DMA_HPP
#define DMA_HPP
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/dma.h>
#include "xassert.hpp"
#include "snprint.hpp"
#include "common.hpp"

//#define DMA_ENABLE_DEBUG

#ifdef DMA_ENABLE_DEBUG
    #define DMA_LOG_DEBUG(fmt,...) tprintf("%(%): " fmt "\n", Base::periphName(), DmaInfo::periphName(), ##__VA_ARGS__)
#else
   #define DMA_LOG_DEBUG(fmt,...)
#endif
TYPE_SUPPORTS(HasTxDma, &std::remove_reference<T>::type::dmaTxStop);
TYPE_SUPPORTS(HasRxDma, &std::remove_reference<T>::type::dmaRxStop);

namespace dma
{
constexpr uint32_t periphSizeCode(uint8_t size)
{
    switch(size)
    {
        case 1: return DMA_CCR_PSIZE_8BIT;
        case 2: return DMA_CCR_PSIZE_16BIT;
        case 4: return DMA_CCR_PSIZE_32BIT;
        default: __builtin_trap();
    }
}
constexpr uint32_t memSizeCode(uint8_t size)
{
    switch(size)
    {
        case 1: return DMA_CCR_MSIZE_8BIT;
        case 2: return DMA_CCR_MSIZE_16BIT;
        case 4: return DMA_CCR_MSIZE_32BIT;
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
    typedef PeriphInfo<Base::kDmaTxId> DmaInfo;
    typedef void(*FreeFunc)(void*);
    volatile bool mTxBusy = false;
    volatile const void* mTxBuf = nullptr;
    volatile FreeFunc mFreeFunc = nullptr;
protected:
    enum: uint8_t { kDmaTxIrq = Self::dmaIrqForChannel(Base::kDmaTxChannel) };
public:
    template <typename... Args>
    void init(Args... args)
    {
        enum: uint8_t { chan = Self::kDmaTxChannel };
        enum: uint32_t { dma = Self::kDmaTxId };

        Base::init(args...);
        DMA_LOG_DEBUG("Tx: Initializing channel %, irq %, opts: %",
            (int)Base::kDmaTxChannel, (int)kDmaTxIrq, fmtHex(Opts));

        if (!HasRxDma<Base>::value)
        {
            rcc_periph_clock_enable(Self::kDmaClockId);
            DMA_LOG_DEBUG("Tx: Enabled clock");
        }

        dma_channel_reset(dma, chan);
        dma_set_peripheral_address(dma, chan, Base::kDmaTxDataRegister);
        dma_set_peripheral_size(dma, chan, periphSizeCode(Self::kDmaWordSize));
        dma_disable_peripheral_increment_mode(dma, chan);

        dma_set_read_from_memory(dma, chan);
        dma_enable_memory_increment_mode(dma, chan);
        dma_set_priority(dma, chan, ((Opts & kPrioMask) >> kPrioShift) << DMA_CCR_PL_SHIFT);
        if ((Opts & kDmaNoDoneIntr) == 0)
        {
            nvic_set_priority(kDmaTxIrq, (Opts & kIrqPrioMask) >> kIrqPrioShift);
            DMA_LOG_DEBUG("Tx: Enabled transfer complete interrupt");
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
        enum: uint32_t { dma = Self::kDmaTxId };
        xassert(size % Base::kDmaWordSize == 0);

        while(mTxBusy);
        mTxBusy = true;
        mTxBuf = data;
        mFreeFunc = freeFunc;

        dma_set_memory_address(dma, chan, (uint32_t)data);
        dma_set_number_of_data(dma, chan, size / Base::kDmaWordSize);
        dma_set_memory_size(dma, chan, memSizeCode(Self::kDmaWordSize));
        if ((Opts & kDmaNoDoneIntr) == 0)
        {
            dma_enable_transfer_complete_interrupt(dma, chan);
            nvic_enable_irq(kDmaTxIrq);
        }
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
        xassert(mTxBuf);
        //FIXME: mFreeFunc may not be reentrant
        if (mFreeFunc)
        {
            mFreeFunc((void*)mTxBuf);
        }
        mTxBuf = nullptr;
        mTxBusy = false;
    }
    static void dmaPrintSink(const char* str, size_t len, int fd, void* userp)
    {
        auto& self = *static_cast<Self*>(userp);
        self.dmaTxStart((const void*)str, len, tprintf_free);
    }
    void setDmaPrintSink()
    {
        ::setPrintSink(dmaPrintSink, this, kPrintSinkLeaveBuffer);
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
    typedef PeriphInfo<Base::kDmaRxId> DmaInfo;
public:
    enum: uint8_t { kDmaRxIrq = Self::dmaIrqForChannel(Self::kDmaRxChannel) };
    volatile bool dmaRxBusy() const { return mRxBusy; }
    template<typename... Args>
    void init(Args... args)
    {
        Base::init(args...);
        DMA_LOG_DEBUG("Rx: Initializing channel %, irq %, opts: %",
            (int)Base::kDmaRxChannel, (int)kDmaRxIrq, fmtHex(Opts));

        enum: uint8_t { chan = Base::kDmaRxChannel };
        enum: uint32_t { dma = Base::kDmaRxId };

        if (!HasTxDma<Base>::value)
        {
            rcc_periph_clock_enable(Self::kDmaClockId);
            DMA_LOG_DEBUG("Rx: Enabled clock");
        }

        dma_disable_channel(dma, chan);
        dma_channel_reset(dma, chan);
        dma_set_peripheral_address(dma, chan, (uint32_t)Base::kDmaRxDataRegister);
        dma_set_peripheral_size(dma, chan, periphSizeCode(Base::kDmaWordSize));
        dma_disable_peripheral_increment_mode(dma, chan);

        dma_enable_memory_increment_mode(dma, chan);
        dma_set_read_from_peripheral(dma, chan);
        dma_set_priority(dma, chan, ((Opts & kPrioMask) >> kPrioShift) << DMA_CCR_PL_SHIFT);
        if (Opts & kDmaCircularMode)
        {
            dma_enable_circular_mode(dma, chan);
        }
        else if ((Opts & kDmaNoDoneIntr) == 0) // Interrupt when transfer complete
        {
            nvic_set_priority(kDmaRxIrq, (Opts & kIrqPrioMask) >> kIrqPrioShift);
            DMA_LOG_DEBUG("Rx: Enabled transfer complete interrupt");
        }
    }
    template <typename... Args>
    void dmaRxStart(const void* data, uint16_t size, Args... args)
    {
        xassert(size % Base::kDmaWordSize == 0);
        enum: uint32_t { dma = Base::kDmaRxId };
        enum: uint8_t { chan = Base::kDmaRxChannel };
        while(mRxBusy);
        mRxBusy = true;

        dma_set_memory_address(dma, chan, (uint32_t)data);
        dma_set_memory_size(dma, chan, memSizeCode(Base::kDmaWordSize));
        dma_set_number_of_data(dma, chan, size / Base::kDmaWordSize);
        dma_enable_channel(dma, chan);
        if ((Opts & kDmaNoDoneIntr) == 0)
        {
            dma_enable_transfer_complete_interrupt(dma, chan);
            nvic_enable_irq(kDmaRxIrq);
        }
        Base::dmaStartPeripheralRx(args...);
    }
    void dmaRxIsr()
    {
        if ((DMA_ISR(Base::kDmaRxId) & DMA_ISR_TCIF(Self::kDmaRxChannel)) == 0)
        {
            return;
        }
        DMA_IFCR(Base::kDmaRxId) |= DMA_IFCR_CTCIF(Self::kDmaRxChannel);
        dmaRxStop();
    }
    void dmaRxStop()
    {
        nvic_disable_irq(kDmaRxIrq);
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
#ifndef NDEBUG
    static constexpr const char* periphName() { return "DMA1"; }
#endif
};

template<>
struct PeriphInfo<DMA2>
{
    enum: uint32_t { kDmaId = DMA2 };
    static constexpr rcc_periph_clken kDmaClockId = RCC_DMA2;
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
#ifndef NDEBUG
    static constexpr const char* periphName() { return "DMA2"; }
#endif
};

#endif // DMA_HPP
