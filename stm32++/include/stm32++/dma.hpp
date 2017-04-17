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
#include "snprint.h"
namespace dma
{
template <uint32_t DMA>
struct DmaInfo;

template<>
struct DmaInfo<DMA1>
{
    enum: uint32_t { dmaId = DMA1 };
    static constexpr rcc_periph_clken dmaClockId = RCC_DMA1;
};
template <uint32_t DMA, uint8_t chan>
static inline constexpr uint8_t chanIrq();


template<>
struct DmaInfo<DMA2>
{
    enum: uint32_t { dmaId = DMA2 };
    static constexpr rcc_periph_clken dmaClockId = RCC_DMA2;
};

template <uint8_t S>
static inline constexpr uint32_t periphSizeCode();
template <uint8_t S>
static inline constexpr uint32_t memSizeCode();

template<>
constexpr uint32_t periphSizeCode<8>() { return DMA_CCR_PSIZE_8BIT; }
template<>
constexpr uint32_t periphSizeCode<16>() { return DMA_CCR_PSIZE_16BIT; }
template<>
constexpr uint32_t periphSizeCode<32>() { return DMA_CCR_PSIZE_32BIT; }
template<>
constexpr uint32_t memSizeCode<8>() { return DMA_CCR_MSIZE_8BIT; }
template<>
constexpr uint32_t memSizeCode<16>() { return DMA_CCR_MSIZE_16BIT; }
template<>
constexpr uint32_t memSizeCode<32>() { return DMA_CCR_MSIZE_32BIT; }

enum: uint8_t {
    kPrioMask =      0x3, //bits 0 and 1 denote DMA interrupt priority
    kPrioShift =       0,
    kPrioVeryHigh =    3,
    kPrioHigh =        2,
    kPrioMedium =      1,
    kPrioLow =         0,
    kIrqPrioMask =   0x3 << 2,
    kIrqPrioShift =    2,
    kIrqPrioVeryHigh = 0 << 2,
    kIrqPrioHigh =     1 << 2,
    kIrqPrioMedium =   2 << 2,
    kIrqPrioLow =      3 << 2,
    kDontEnableClock = 0x10,
    kDefaultOpts = kIrqPrioMedium|kPrioMedium,
    kAllMaxPrio = kPrioVeryHigh | kIrqPrioVeryHigh
};

/** Mixin to support Tx DMA. Base is derived from DmaInfo<Periph>,
 * where Periph is the actual peripheral for which DMA is to be supported
 */
template <class Base, class Derived, uint8_t Opts=kDefaultOpts>
class Tx: public Base, public DmaInfo<Base::periphId>
{
protected:
    typedef void(*FreeFunc)(void*);
    volatile bool mTxBusy = false;
    volatile const void* mTxBuf = nullptr;
    volatile FreeFunc mFreeFunc = nullptr;
    typedef Tx<Base, Derived, Opts> Self;
    enum: uint8_t { kDmaTxIrq = chanIrq<Self::dmaId, Self::kDmaTxChannel>() };
    Derived& derived() { return *static_cast<Derived*>(this); }
    void dmaTxInit()
    {
        nvic_set_priority(kDmaTxIrq, (Opts & kIrqPrioMask) >> kIrqPrioShift);
        nvic_enable_irq(kDmaTxIrq);
        if ((Opts & kDontEnableClock) == 0)
            rcc_periph_clock_enable(Self::dmaClockId);
    }
public:
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
    void dmaSend(const void* data, uint16_t size, FreeFunc freeFunc)
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
        dma_set_peripheral_size(dma, chan, periphSizeCode<Self::kDmaWordSize>());
        dma_set_memory_size(dma, chan, memSizeCode<Self::kDmaWordSize>());
        dma_set_priority(dma, chan, ((Opts & kPrioMask) >> kPrioShift) << DMA_CCR_PL_SHIFT);
        dma_enable_transfer_complete_interrupt(dma, chan);
        dma_enable_channel(dma, chan);
        //have to enable DMA for peripheral at the upper level and the transfer should start
        derived().dmaStartPeripheralTx();
    }
    volatile bool txBusy() const { return mTxBusy; }
    void dmaTxIsr()
    {
        if ((DMA_ISR(Self::dmaId) & DMA_ISR_TCIF(Self::kDmaTxChannel)) == 0)
            return;

        DMA_IFCR(Self::dmaId) |= DMA_IFCR_CTCIF(Self::kDmaTxChannel);
        dmaTxStop();
    }
    void dmaTxStop()
    {
        //good to disable dma in peripheral before calling this
        dma_disable_transfer_complete_interrupt(Self::dmaId, Self::kDmaTxChannel);
        derived().dmaStopPeripheralTx();
        dma_disable_channel(Self::dmaId, Self::kDmaTxChannel);
        assert(mTxBuf);
        //FIXME: mFreeFunc may not be reentrant
        if (mFreeFunc)
            mFreeFunc((void*)mTxBuf);
        mTxBuf = nullptr;
        mTxBusy = false;
    }
};
/** Mixin to support Rx DMA. Base is derived from DmaInfo<Periph>,
 * where Periph is the actual peripheral for which DMA is to be supported
 */
template <class Base, class Derived, uint8_t Opts=kDefaultOpts>
class Rx: public Base, public DmaInfo<Base::periphId>
{
protected:
    volatile bool mRxBusy = false;
    typedef Rx<Base, Derived, Opts> Self;
    Derived& derived() { return *static_cast<Derived*>(this); }
public:
    volatile bool rxBusy() const { return mRxBusy; }
    void dmaRxInit()
    {
        nvic_set_priority(Self::kDmaRxIrq, (Opts & kIrqPrioMask) >> kIrqPrioShift);
        nvic_enable_irq(Self::kDmaRxIrq);
        if ((Opts & kDontEnableClock) == 0)
            rcc_periph_clock_enable(Self::dmaClock);
    }
    void dmaRecv(const char *data, uint16_t size)
    {
        enum: uint8_t { chan = Self::kDmaChannelRx };
        enum: uint32_t { dma = Self::dmaId };
        while(mRxBusy);
        mRxBusy = true;
        dma_channel_reset(dma, chan);
        dma_set_peripheral_address(dma, chan, (uint32_t)Self::dataRegister);
        dma_set_memory_address(dma, chan, (uint32_t)data);
        dma_set_number_of_data(dma, chan, size);
        dma_set_read_from_peripheral(dma, chan);
        dma_enable_memory_increment_mode(dma, chan);
        dma_disable_peripheral_increment_mode(dma, chan);
        dma_set_peripheral_size(dma, chan, periphSizeCode(Base::kDmaWordSize));
        dma_set_memory_size(dma, chan, memSizeCode(Base::kDmaWordSize));
        dma_set_priority(dma, chan, ((Opts & kPrioMask) << kPrioShift) << DMA_CCR_PL_SHIFT);
        dma_enable_transfer_complete_interrupt(dma, chan);
        dma_enable_channel(dma, chan);
        derived().enablePeripheralRx();
    }
    void dmaRxIsr()
    {
        if ((DMA_ISR(Self::dmaId) & DMA_ISR_TCIF(Self::kDmaRxChan)) == 0)
            return;

        DMA_IFCR(Self::dmaId) |= DMA_IFCR_CTCIF(Self::kDmaRcChan);
        dmaRxStop();
    }
    void dmaRxStop()
    {
        dma_disable_transfer_complete_interrupt(Self::dmaId, Self::kDmaRxChan);
        derived().dmaStopPeripheralRx();
        dma_disable_channel(Self::dmaId, Self::kDmaRxChan);
        mRxBusy = false;
    }
};

/** Peripheral definitions */


/** Common base for all DmaInfo classes that are about a peripheral
 * (i.e. not about the DMAx controllers themself)
 */
template <uint32_t Periph, uint32_t DMA, uint8_t txChan, uint8_t rxChan,
          uint8_t wordSize>
struct PeriphDmaInfo: public DmaInfo<DMA>
{
    enum: uint8_t
    {
        kDmaWordSize = wordSize,
        kDmaTxChannel = txChan,
        kDmaRxChannel = rxChan
    };
};
template<> constexpr uint8_t chanIrq<DMA1, 1>()
{ return NVIC_DMA1_CHANNEL1_IRQ; }
template<> constexpr uint8_t chanIrq<DMA1, 2>()
{ return NVIC_DMA1_CHANNEL2_IRQ; }
template<> constexpr uint8_t chanIrq<DMA1, 3>()
{ return NVIC_DMA1_CHANNEL3_IRQ; }
template<> constexpr uint8_t chanIrq<DMA1, 4>()
{ return NVIC_DMA1_CHANNEL4_IRQ; }
template<> constexpr uint8_t chanIrq<DMA1, 5>()
{ return NVIC_DMA1_CHANNEL5_IRQ; }
template<> constexpr uint8_t chanIrq<DMA1, 6>()
{ return NVIC_DMA1_CHANNEL6_IRQ; }
template<> constexpr uint8_t chanIrq<DMA1, 7>()
{ return NVIC_DMA1_CHANNEL7_IRQ; }
template<> constexpr uint8_t chanIrq<DMA2, 1>()
{ return NVIC_DMA2_CHANNEL1_IRQ; }
template<> constexpr uint8_t chanIrq<DMA2, 2>()
{ return NVIC_DMA2_CHANNEL2_IRQ; }
template<> constexpr uint8_t chanIrq<DMA2, 3>()
{ return NVIC_DMA2_CHANNEL3_IRQ; }
template<> constexpr uint8_t chanIrq<DMA2, 4>()
{ return NVIC_DMA2_CHANNEL4_5_IRQ; }
template<> constexpr uint8_t chanIrq<DMA2, 5>()
{ return NVIC_DMA2_CHANNEL5_IRQ; }
}

#endif // DMA_HPP
