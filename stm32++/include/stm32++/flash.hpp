#ifndef FLASH_HPP_INCLUDED
#define FLASH_HPP_INCLUDED

#include <stddef.h>
#include <stdint.h>
#ifdef __arm__
    #include <libopencm3/stm32/flash.h>
#else
    #include <assert.h>
    #include <memory.h>
    #include <stdio.h>
#endif
namespace flash
{
template <typename T> bool addressIsEven(T* addr)
{ return ((((size_t)addr) & 0x1) == 0); }

template <typename T>
static inline uint16_t roundToNextEven(T x) { return ((uint16_t)x + 1) & (~0x1); }

#ifdef __arm__
#define STM32PP_FLASH_LOG(fmtString,...) tprintf("FLASH: " fmtString "\n", ##__VA_ARGS__)

struct DefaultFlashDriver
{
    class WriteUnlocker
    {
    protected:
        bool isUpperBank;
        bool wasLocked;
        bool isLocked() const { return (FLASH_CR & FLASH_CR_LOCK) != 0; }
        bool isUpperLocked() const { return (FLASH_CR2 & FLASH_CR_LOCK) != 0; }
    public:
        WriteUnlocker(void* page)
        : isUpperBank((DESIG_FLASH_SIZE > 512) && ((size_t)page >= FLASH_BASE+0x00080000))
        {
            if (isUpperBank)
            {
                wasLocked = isUpperLocked();
                if (wasLocked) flash_unlock_upper();
            }
            else
            {
                wasLocked = isLocked();
                if (wasLocked) flash_unlock();
            }
            clearStatusFlags();
        }
        ~WriteUnlocker()
        {
            if (isUpperBank)
            {
                if (wasLocked) flash_lock_upper();
            }
            else
            {
                if (wasLocked) flash_lock();
            }
        }
    };
    enum: uint32_t { kFlashWriteErrorFlags = FLASH_SR_WRPRTERR
#ifdef FLASH_SR_PGERR
    | FLASH_SR_PGERR
#endif
#ifdef FLASH_SR_PGAERR
    | FLASH_SR_PGAERR
#endif
#ifdef FLASH_SR_PGPERR
    | FLASH_SR_PGPERR
#endif
#ifdef FLASH_SR_ERSERR
    | FLASH_SR_ERSERR
#endif
    };
    uint16_t pageSize() const
    {
        static const uint16_t flashPageSize = DESIG_FLASH_SIZE << 10;
        return flashPageSize;
    }
    bool write16(uint8_t* addr, uint16_t data)
    {
        assert(addressIsEven(addr));
        flash_program_half_word(addr, data);
        return (*(uint16_t*)(addr) == data);
    }
    bool write16Block(uint8_t* dest, const uint8_t* src, uint16_t wordCnt)
    {
        assert((dest & 0x1) == 0);
        assert((src & 0x1) == 0);
        uint16_t* wptr = (uint16_t*)dest;
        uint16_t* rptr = (uint16_t*)src;
        uint16_t* rend = rptr + wordCnt;
        for (; rptr < rend; rptr++, wptr++)
        {
            flash_program_half_word(wptr, *data);
            if (*wptr != *data)
            {
                return false;
            }
        }
        return true;
    }
    void clearStatusFlags()
    {
        flash_clear_status_flags();
    }
    uint32_t errorFlags()
    {
        uint32_t flags = FLASH_SR;
        if (DESIG_FLASH_SIZE > 512)
        {
            flags |= FLASH_SR2;
        }
        return flags & kFlashWriteErrorFlags;
    }
    static bool erasePage(uint8_t* page)
    {
        assert(((size_t)page) % 4 == 0);
        flash_erase_page(page);
        auto err = errorFlags();
        if (err)
        {
            STM32PP_FLASH_LOG_ERROR("erasePage: Error flag(s) set after erase: %", fmtBin(err));
            return false;
        }
        uint32_t* pageEnd = (uint32_t*)(page + pageSize());
        for (uint32_t* ptr = (uint32_t*)page; ptr < pageEnd; ptr++)
        {
            if (*ptr != 0xffffffff)
            {
                STM32PP_FLASH_LOG_ERROR("erasePage: Page contains a byte that is not 0xff after erase");
                return false;
        }
        return true;
    }
};
#else
// x86, test mode
#define STM32PP_FLASH_LOG(fmtString,...) printf("FLASH: " fmtString "\n", ##__VA_ARGS__)

// This is a simulator used for testing the library, on a pc
struct DefaultFlashDriver
{
    class WriteUnlocker
    { public: WriteUnlocker(uint8_t* addr){} };
    enum: uint32_t { kFlashWriteErrorFlags = 0x1 };
#ifdef STM32PP_FLASH_SIMULATE_POWER_LOSS
    static int32_t failAtWriteNum;
#endif
    static bool write16(uint8_t* addr, uint16_t data)
    {
#ifdef STM32PP_FLASH_SIMULATE_POWER_LOSS
        if (--failAtWriteNum <= 0)
        {
            throw 0;
        }
#endif
        *((uint16_t*)addr) = data;
        return true;
    }
    static uint16_t pageSize() { return 1024; }
    static bool write16Block(uint8_t* dest, const uint8_t* src, uint16_t wordCnt)
    {
        assert(addressIsEven(dest));
//      assert(addressIsEven(src));
        for (auto srcEnd = src + wordCnt * 2; src < srcEnd; src+=2, dest+=2)
        {
            if (!write16(dest, *((uint16_t*)src)))
            {
                return false;
            }
        }
        return true;
    }
    static uint32_t errorFlags() { return 0; }
    static void clearStatusFlags() {}
    static bool erasePage(uint8_t* page)
    {
        memset(page, 0xff, pageSize());
        return true;
    }
};
#endif

#define STM32PP_FLASH_LOG_ERROR(fmtString,...) STM32PP_FLASH_LOG("ERROR: " fmtString, ##__VA_ARGS__)
#define STM32PP_FLASH_LOG_WARNING(fmtString,...) STM32PP_FLASH_LOG("WARN: " fmtString, ##__VA_ARGS__)
#define STM32PP_FLASH_LOG_DEBUG(fmtString,...) STM32PP_FLASH_LOG("DEBUG: " fmtString, ##__VA_ARGS__)

template <class Driver>
struct FlashPageInfo
{
    enum { kMagicLen = 6 };
    enum ValidateError: uint8_t {
           kErrNone = 0, kErrMagic = 1,
           kErrCounter = 2, kErrDataEndAlign = 3,
           kErrData = 4
    };
    uint8_t* page;
    uint8_t* dataEnd;
    uint16_t pageCtr;
    ValidateError validateError;

    bool isPageValid() const { return validateError == kErrNone; }
    static const uint8_t* magic()
    {
        alignas(2) static const char magic[] = "nvstor";
        static_assert(sizeof(magic) == kMagicLen + 1, "Mismatch of magic string and the enum denoting its length");
        assert(addressIsEven(magic));
        return (const uint8_t*)magic;
    }
    static uint16_t getPageCounter(uint8_t* page)
    {
        return *((uint16_t*)(page + Driver::pageSize() - kMagicLen - sizeof(uint16_t)));
    }
    /**
     * @brief findDataEnd Finds where the data in a page ends, by scanning the page backwards
     * and finding the last byte with value 0xff
     * @param page The start of the page which to scan
     * @return A pointer to the first byte after the last data entry (first byte with 0xff value)
     * If the pointer is not an even address, then the page content is not valid, and nullptr
     * is returned
     */
    static uint8_t* findDataEnd(uint8_t* page)
    {
        assert(addressIsEven(page));
        for (uint8_t* ptr = page + Driver::pageSize() - 9; ptr >= page; ptr--)
        {
            if (*ptr != 0xff)
            {
                ptr++;
                return addressIsEven(ptr) ? ptr : nullptr;
            }
        }
        // page is completely empty
        return page;
    }

    FlashPageInfo(uint8_t* aPage);
};

template<class Driver=DefaultFlashDriver>
class FlashValueStore
{
protected:
    using PageInfo = FlashPageInfo<Driver>;
    uint8_t* mPage1 = nullptr;
    uint8_t* mPage2 = nullptr;

    bool mIsShuttingDown = false;
    uint8_t* mActivePage = nullptr;
    uint8_t* mDataEnd;
    uint16_t mReserveBytes;
    friend PageInfo;
public:
    uint8_t* activePage() const { return mActivePage; }
    bool init(size_t page1Addr, size_t page2Addr, uint16_t reserveBytes=0)
    {
        assert(page1Addr % 4 == 0);
        assert(page2Addr % 4 == 0);
        mPage1 = (uint8_t*)page1Addr;
        mPage2 = (uint8_t*)page2Addr;
        mReserveBytes = reserveBytes;

        PageInfo info1(mPage1);
        PageInfo info2(mPage2);
        if (info1.isPageValid())
        {
            if (info2.isPageValid())
            {
                // both contain data, the one with bigger counter wins
                if (info1.pageCtr >= info2.pageCtr)
                {
                    if (info1.pageCtr == info2.pageCtr)
                    {
                        STM32PP_FLASH_LOG_WARNING("init: Both pages are valid, and have the same counter, using page1");
                    }
                    else
                    {
                        STM32PP_FLASH_LOG_DEBUG("init: Both pages are valid, but page1 has bigger counter, using it");
                    }
                    mActivePage = info1.page;
                    mDataEnd = info1.dataEnd;
                }
                else if (info2.pageCtr > info1.pageCtr)
                {
                    mActivePage = info2.page;
                    mDataEnd = info2.dataEnd;
                    STM32PP_FLASH_LOG_DEBUG("init: Both pages are valid, but page2 has bigger counter, using it");
                }
            }
            else // page1 valid, page2 invalid
            {
                mActivePage = info1.page;
                mDataEnd = info1.dataEnd;
                STM32PP_FLASH_LOG_DEBUG("init: Page1 is valid, page2 is invalid, using page1");
            }
        }
        else // page1 invalid
        {
            if (info2.isPageValid())
            {
                mActivePage = info2.page;
                mDataEnd = info2.dataEnd;
                STM32PP_FLASH_LOG_DEBUG("init: Page2 is valid, page1 is invalid, using page2");
            }
            else // both pages invalid
            {
                STM32PP_FLASH_LOG_WARNING("No page is initialized, initializing and using page1");
                Driver::erasePage(info1.page);
                writePageCtrAndMagic(info1.page, 1);
                mActivePage = mDataEnd = info1.page;
            }
        }
        return true;
    }
    /**
     * @brief getRawValue
     * @param key - The id of the value
     * @param size - Outputs the size of the returned data.
     * @return
     * - If a value with the specified key was found, returns a pointer to the data and
     * \c size is set to the size of the data
     * - If the value was not found, \c nullptr is returned and \c size is set to zero
     * - If an error occurred during the search, \c nullptr is returned and \c size is set
     * to a nonzero error code
     */
    uint8_t* getRawValue(uint8_t key, uint8_t& size)
    {
        if (mDataEnd == mActivePage)
        {
            size = 0;
            return nullptr;
        }

        for(uint8_t* ptr = mDataEnd;;)
        {
            auto entryKey = *(ptr - 1);
            if (key == entryKey)
            {
                size = *(ptr-2);
                if (!size)
                {
                    return nullptr;
                }
                if (size & 1) // odd size, we have a padding byte, verify it
                {
                    if (*(ptr - 3) == 0) // data is incomplete (due to power loss?), skip this entry
                    {
                        return ptr - 3 - size;
                    }
                }
                else // no way to verify data completeness if data size is even
                {
                    return (ptr - 2 - size);
                }
            }
            ptr = getPrevEntryEnd(ptr, mActivePage);
            if (!ptr)
            {
                size = 0;
                return nullptr;
            }
            if (ptr == (uint8_t*)-1)
            {
                size = 1;
                return nullptr;
            }
        }
    }
    template <typename T>
    bool getValue(uint8_t key, T& val)
    {
        uint8_t size;
        auto ptr = getRawValue(key, size);
        if (!ptr || size != sizeof(T))
        {
            return false;
        }
        if (sizeof(T) <= 2)
        {
            val = *(T*)(ptr);
        }
        else
        {
            memcpy(&val, ptr, size);
        }
        return true;
    }
    template <typename T>
    T getValue(uint8_t key, T defaultVal)
    {
        uint8_t size;
        auto ptr = getRawValue(key, size);
        if (!ptr || size != sizeof(T))
        {
            return defaultVal;
        }
        if (sizeof(T) <= 2)
        {
            return *(T*)(ptr);
        }
        else
        {
            T val;
            memcpy(&val, ptr, size);
            return val;
        }
    }
    uint16_t pageBytesFree() const
    {
        return Driver::pageSize() - (mDataEnd - mActivePage) - PageInfo::kMagicLen - 2;
    }
    uint8_t activePageId() const { return (mActivePage == mPage1) ? 1 : 2; }
    bool setValue(uint8_t key, const void* data, uint8_t len, bool isEmergency=false)
    {
        if (key == 0xff)
        {
            STM32PP_FLASH_LOG_ERROR("setValue: Invalid key 0xff provided");
            return false;
        }
        auto bytesFree = pageBytesFree();
        if (!isEmergency)
        {
            if (mIsShuttingDown)
            {
                STM32PP_FLASH_LOG_ERROR("setValue: Refusing to write, system is shutting down");
                return false;
            }
            bytesFree -= mReserveBytes;
        }
        uint16_t bytesNeeded = 2 + roundToNextEven(len);
        if (bytesNeeded > bytesFree)
        {
            STM32PP_FLASH_LOG_DEBUG("Not enough space in page%d to write value, switching to other page and compacting", activePageId());
            if (!compact()) // should log error message
            {
                return false;
            }
            STM32PP_FLASH_LOG_DEBUG("Compacted to %d bytes\n", Driver::pageSize()-pageBytesFree());
            bytesFree = pageBytesFree();
            if (bytesNeeded > bytesFree)
            {
                STM32PP_FLASH_LOG_ERROR("Not enough space to write value even after compacting: available: %d, required %d bytes", bytesFree, bytesNeeded);
                return false;
            }
        }
        typename Driver::WriteUnlocker unlocker(mActivePage);
        // data.len [pad.1] len.1 key.1
        bool ok = true;
        // First write the trailer, so that if we are are interrupted while writing
        // the actual length, we can still have the record boundary
        ok &= Driver::write16(mDataEnd + roundToNextEven(len), (key << 8) | len);

        if ((len & 1) == 0) // even number of bytes
        {
            if (len)
            {
                ok &= Driver::write16Block(mDataEnd, (uint8_t*)data, len / 2);
            }
            mDataEnd += (len + 2);
        }
        else
        { // len is odd
            uint8_t even = len - 1;
            if (even)
            {
                Driver::write16Block(mDataEnd, (uint8_t*)data, even / 2);
                mDataEnd += even;
            }
            // write last (odd) data byte and a zero padding byte
            ok &= Driver::write16(mDataEnd, ((uint8_t*)data)[even]);
            mDataEnd += 4; // 1 word with last odd and padding byte, and length+key
        }
        auto err = Driver::errorFlags();
        if (err)
        {
            STM32PP_FLASH_LOG_ERROR("Error writing value: %x", err);
            return false;
        }
        return true;
    }
    template <typename T>
    bool setValue(uint8_t key, T val, bool isEmergency=false)
    {
        return setValue(key, &val, sizeof(T), isEmergency);
    }
protected:
    /**
     * @brief verifyAllEntries Parses all entries
     * @return false if an error was detected, true otherwise
     */
    static bool verifyAllEntries(uint8_t* dataEnd, uint8_t* page)
    {
        while(dataEnd > page)
        {
            if (*(dataEnd - 1) == 0xff) // key can't be 0xff
            {
                STM32PP_FLASH_LOG_ERROR("verifyAllEntries: Found a key with value 0xff");
                return false;
            }
            dataEnd = getPrevEntryEnd(dataEnd, page);
            if (!dataEnd)
            {
                return true;
            }
            if (dataEnd == (uint8_t*)-1)
            {
                return false;
            }
        }
        // getPrevEntryEnd guarantees it won't return a pointer < page
        assert(dataEnd == page);
        return true;
    }

    /**
     * @brief getPrevEntryEnd - returns a pointer past the end of the entry preceding the specified one
     * @param entryEnd - pointer past the last byte of an entry
     * @return Pointer to the first byte of this entry, i.e. points past the end of the previous entry
     */
    static uint8_t* getPrevEntryEnd(uint8_t* entryEnd, uint8_t* page)
    {
         // data.len [align.1] len.1 key.1
        if (entryEnd < page + 2)
        {
            if (entryEnd == page)
            {
                return nullptr; // we have reached the start of the page
            }
            else
            {
                STM32PP_FLASH_LOG_ERROR("getPrevEntryEnd: provided entryEnd is less than 2 bytes past the start of the page");
                return (uint8_t*)-1; // 0xffffffff pointer means error
            }
        }
        uint8_t len = *(entryEnd - 2);
        // if len is odd, round it to the next even number, i.e. we have an extra
        // padding byte after the data, if len is odd
        auto ret = entryEnd - 2 - roundToNextEven(len);
        if (ret < page)
        {
            STM32PP_FLASH_LOG_ERROR("getPrevEntryEnd: provided entry spans before page start");
            return (uint8_t*)-1;
        }
        return ret;
    }
    static bool writePageCtrAndMagic(uint8_t* page, uint16_t ctr)
    {
        bool ok = true;
        auto end = page + Driver::pageSize();
        ok &= Driver::write16Block(end - PageInfo::kMagicLen, PageInfo::magic(), PageInfo::kMagicLen / 2);
        ok &= Driver::write16(end - PageInfo::kMagicLen - sizeof(uint16_t), ctr);
        return ok;
    }
    bool compact()
    {
        if (mIsShuttingDown)
        {
            STM32PP_FLASH_LOG_ERROR("compactPage: System is shutting down");
            return false;
        }
        if (mDataEnd == mActivePage)
        {
            STM32PP_FLASH_LOG_DEBUG("compactPage: Page is empty, nothing to compact");
            return true;
        }
        auto otherPage = (mActivePage == mPage1) ? mPage2 : mPage1;
        auto srcPage = mActivePage;
        uint8_t* srcEnd = mDataEnd; // equal to mActivePage if page is empty

        mActivePage = mDataEnd = otherPage;
        typename Driver::WriteUnlocker unlocker(mActivePage);
        Driver::erasePage(mActivePage);
        uint32_t hadKey[8] = { 0 };
        while (srcEnd > srcPage)
        {
            uint8_t len = *(srcEnd - 2);
            uint8_t* data = srcEnd - 2 - roundToNextEven(len);

            uint8_t key = *(srcEnd - 1);
            assert(key != 0xff);
            uint8_t idx = key >> 5;
            uint32_t mask = 1 << (key & 0b00011111);
            auto& flags = hadKey[idx];
            if ((flags & mask) == 0)
            {
                flags |= mask;
                setValue(key, data, len, true);
            }
            srcEnd = data;
        }
        if (srcEnd != srcPage)
        {
            STM32PP_FLASH_LOG_ERROR("compactPage: backward scan did not end at page start, still continuing");
        }
        writePageCtrAndMagic(mActivePage, PageInfo::getPageCounter(srcPage) + 1);
        // Done writing compacted page to the new page
        return true;
    }
};
template <class Driver>
FlashPageInfo<Driver>::FlashPageInfo(uint8_t* aPage)
    : page(aPage), pageCtr(getPageCounter(page))
{
    if (memcmp(page + Driver::pageSize() -kMagicLen, magic(), kMagicLen) != 0)
    {
        validateError = kErrMagic;
        return;
    }
    //magic is ok
    dataEnd = findDataEnd(page);
    if (!dataEnd)
    {
        validateError = kErrDataEndAlign;
        return;
    }
    if (pageCtr == 0xffff)
    {
        validateError = kErrCounter;
        return;
    }
    if (!FlashValueStore<Driver>::verifyAllEntries(dataEnd, page))
    {
        validateError = kErrData;
        return;
    }
    validateError = kErrNone;
}

}
#endif
