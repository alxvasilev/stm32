#ifndef FLASH_HPP_INCLUDED
#define FLASH_HPP_INCLUDED

#ifdef __arm__
#include <libopencm3/stm32/flash.h>

typedef uint32_t Addr;
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
        WriteUnlocker(Addr page)
        : isUpperBank((DESIG_FLASH_SIZE > 512) && (page >= FLASH_BASE+0x00080000))
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
    bool write16(Addr addr, uint16_t data)
    {
        assert((addr & 0x1) == 0);
        flash_program_half_word(addr, data);
        return (*(uint16_t*)(addr) == data);
    }
    bool writeBuf(Addr addr, uint16_t* data, uint16_t wordCnt)
    {
        assert((addr & 0x1) == 0);
        uint16_t* wptr = (uint16_t*)addr;
        uint16_t* end = data + wordCnt;
        for (; data < end; data++, wptr++)
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
        return flags & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR);
    }
    bool erasePage(Addr pageAddr, size_t pageSize)
    {
        assert((pageSize % 4) == 0);
        flash_erase_page(pageAddr);
        uint32_t* pageEnd = pageAddr + (pageSize / sizeof(uint32_t));
        for (uint32_t* ptr = pageAddr; ptr < pageEnd; ptr++)
        {
            if (*ptr != 0xffffffff)
                return false;
        }
        return true;
    }
};
#else
typedef size_t Addr;
struct DefaultFlashDriver
{
    class WriteUnlocker
    { WriteUnlocker(Addr addr){} };
    void write16(Addr addr, uint16_t data)
    {
        *((uint16_t*)addr) = data;
    }
    uint32_t errorFlags() { return 0; }
    void clearStatusFlags() {}
    void erasePage(Addr addr) {}
};
#endif

template<int PageSize, Addr Page1Addr, Addr Page2Addr, Driver=DefaultFlashDriver>
class FlashValueStore
{
protected:
    const uint8_t* Page1 = (uint8_t*)Page1Addr;
    const uint8_t* Page2 = (uint8_t*)Page2Addr;

    bool mIsShuttingDown = false;
    uint8_t* mActivePage = nullptr;
    uint8_t* mDataEnd = nullptr;
    uint16_t mReserveBytes;
public:
    static constexpr alignas(2) char kPageMagicSig[] = "nvstor";
    enum { kPageMagicSigLen = sizeof(kPageMagicSig) - 1 };
    FlashValueStore(uint16_t reserveBytes=0): mReserveBytes(reserveBytes)
    {
        static_assert(PageSize <= 65535, "Page size is larger than 64K");
        static_assert(PageSize % 4 == 0, "Page size is not a multiple of 4 bytes");
        static_assert(Page1Addr % 4 == 0, "Page1 is not on a 32 bit word boundary");
        static_assert(Page2Addr % 4 == 0, "Page2 is not on a 32 bit word boundary");
    }
    bool init()
    {
        bool page1Valid = verifyPage(Page1);
        bool page2Valid = verifyPage(Page2);
        if (page1Valid && page2Valid)
        {
            auto ctr1 = getPageEraseCounter(Page1);
            auto ctr2 = getPageEraseCounter(Page2);
            if (ctr1 > ctr2)
            {
                mActivePage = Page1;
            }
            else if (ctr2 > ctr1)
            {
                mActivePage = Page2;
            }
            else // counters equal, print a warning and assume page1
            {
                FLASH_LOG_WARNING("FlashValueStore: Both pages have the same erase counter (%), using page1", ctr1);
                mActivePage = Page1;
            }
        }
        else if (page1Valid)
        {
            mActivePage = Page1;
        }
        else if (page2Valid)
        {
            mActivePage = Page2;
        }
        else
        {
            FLASH_LOG_WARNING("No page is initialized, initializing and using page1");
            if (!initPage(Page1, 0))
            {
                return false;
            }
            mActivePage = Page1;
        }
        return true;
    }
    uint16_t getPageEraseCounter(uint8_t* page)
    {
        return *((uint16_t*)(page + PageSize - sizeof(uint16_t)));
    }
    bool setPage(uint8_t* page)
    {
        auto end = findDataEnd(page);
        if (!end)
        {
            FLASH_LOG_WARN("Refusing to set page: content ends on an even address");
            return false;
        }
        if (!verifyAllEntries(end, page))
        {
            FLASH_LOG_WARN("Refusing to set page: error parsing page content");
            return false;
        }
        mActivePage = page;
        mDataEnd = end;
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
    uint8_t* getRawValue(uint8_t key, uint16_t& size)
    {
        uint8_t* ptr = mDataEnd; // equal to mActivePage if page is empty
        while (ptr > page)
        {
            auto entryKey = *(ptr - 3);
            if (key == entryKey)
            {
                size = *((uint16_t*)(ptr-2));
                return size ? (ptr - 4 + (size & 1) - size) : nullptr;
            }
            ptr = getPrevEntryEnd(ptr, mActivePage);
            if (ptr == (uint8_t*)-1)
            {
                size = 1;
                return nullptr;
            }
        }
        size = 0;
        return nullptr;
    }
    uint16_t bytesFree() const
    {
        return PageSize - (mDataEnd - mActivePage) - kPageMagicSigLen - 2;
    }
    bool setValue(uint8_t key, uint8_t* data, uint16_t len, bool isEmergency=false)
    {
        auto bytesFree = PageSize - (mDataEnd - mActivePage) - kPageMagicSigLen - 2;
        if (!isEmergency)
        {
            if (mIsShuttingDown)
            {
                return false;
            }
            bytesFree -= mReserveBytes;
        }
        uint16_t bytesNeeded = len + ((len & 1) ? 3 : 4);
        if (bytesNeeded > bytesFree)
        {
            if (!compactPage()) // should log error message
            {
                return false;
            }
            bytesFree = bytesFree();
            if (bytesNeeded > bytesFree)
            {
                FLASH_LOG_ERROR("Not enough space to write value even after compacting: available: %, required % bytes", bytesFree, bytesNeeded);
                return false;
            }
        }
        Driver::WriteUnlocker unlocker(mActivePage);
        if ((len & 1) == 0) // even number of bytes
        {
            if (len)
            {
                uint8_t* end = mDataEnd + len;
                for (; mDataEnd < end; data+=2, mDataEnd+=2)
                {
                    Driver::write16(mDataEnd, *((uint16_t*)data));
                }
            }
            Driver::write16(mDataEnd, key);
            Driver::write16(mDataEnd + 2, len);
            mDataEnd += 4;
        }
        else
        { // len is odd
            uint16_t even = len - 1;
            if (even)
            {
                uint8_t* end = mDataEnd + even;
                for (; mDataEnd < end; data+=2, mDataEnd+=2)
                {
                    Driver::write16(mDataEnd, *((uint16_t*)data));
                }
            }
            // write last (odd) data byte and key
            Driver::write16(mDataEnd, (key << 8) | data[even]);
            Driver::write16(mDataEnd + 2, len);
            mDataEnd += 4;
        }
        auto err = Driver::errorFlags();
        if (err)
        {
            FLASH_LOG_ERROR("Error writing value: %", fmtHex(err));
        }
        return err == 0;
    }
protected:
    /**
     * @brief verifyAllEntries Parses all entries
     * @return false if an error was detected, true otherwise
     */
    bool verifyAllEntries(uint8_t* dataEnd, uint8_t* page)
    {
        uint8_t* ptr = dataEnd; // equal to mActivePage if page is empty
        while (ptr > page)
        {
            ptr = getPrevEntryEnd(ptr, page);
            if (ptr == (uint8_t*)-1)
            {
                return false;
            }
        }
        if (ptr != page)
        {
            FLASH_LOG_ERROR("verifyAllEntries: backward scan did not end at page start");
            return false;
        }
        return true;
    }

    /**
     * @brief findDataEnd Finds where the data in a page ends, by scanning the page backwards
     * and finding the last byte with value 0xff
     * @param page The start of the page which to scan
     * @return A pointer to the first byte after the last data entry (first byte with 0xff value)
     * If the pointer is not an even address, then the page content is not valid, and nullptr
     * is returned
     */
    uint8_t* findDataEnd(uint8_t* page)
    {
        assert((page & 0x1) == 0);
        for (uint8_t* ptr = page + PageSize - 9; ptr >= page; ptr--)
        {
            if (*ptr != 0xff)
            {
                ptr++;
                return (ptr & 0x1) ? nullptr : ptr;
            }
        }
        // page is completely empty
        return page;
    }
    /**
     * @brief getPrevEntryEnd - returns a pointer past the end of the entry preceding the specified one
     * @param entryEnd - pointer past the last byte of an entry
     * @return Pointer to the first byte of this entry, i.e. points past the end of the previous entry
     */
    uint8_t* getPrevEntryEnd(uint8_t* entryEnd, uint8_t* page)
    {
         // data.len [align.1] key.1 len.2
        if (entryEnd - page < (1 + sizeof(uint16_t)))
        {
            if (entryEnd != page)
            {
                FLASH_LOG_ERROR("getPrevEntryEnd: provided entryEnd is less than 3 bytes past the start of the page");
                return (uint8_t*)-1;
            }
            return nullptr;
        }
        auto len = *((uint16_t*)(entryEnd - sizeof(uint16_t)));
        if (len & 0x8000)
        {
            // This is an error: most significant bit of len must always be zero,
            // to detect presence of data (after flash all bits are 1s)
            return (uint8_t*)-1;
        }
        // if length is odd, we don't need a padding byte -> 3 bytes of metadata
        // if length is even, metadata must also be even -> 4 bytes = 3 bytes + 1 pad byte
        // skip metadata + len + 1 to reach previous entry
        auto ret = entryEnd - 4 + (len & 1) - len;
        if (ret < page)
        {
            FLASH_LOG_ERROR("getPrevEntryEnd: provided entry spans before page start");
            return (uint8_t*)-1;
        }
        return ret;
    }
    bool compactPage()
    {
        if (mIsShuttingDown)
        {
            FLASH_LOG_ERROR("compactPage: System is shutting down");
            return false;
        }
        if (mDataEnd == mActivePage)
        {
            FLASH_LOG_DEBUG("compactPage: Page is empty, nothing to compact");
            return true;
        }
        auto otherPage = (mActivePage == Page1) ? Page2 : Page1;
        if (!otherPage)
        {
            return compactPageInplace();
        }
        auto srcPage = mActivePage;
        uint8_t* srcEnd = mDataEnd; // equal to mActivePage if page is empty

        mActivePage = mDataEnd = otherPage;
        Driver::writeUnlocker unlocker(mActivePage);
        Driver::erasePage(mActivePage, PageSize);
        uint32_t hadKey[8] = { 0 };
        while (srcEnd > srcPage)
        {
            uint8_t key = *(srcEnd - 3);
            uint8_t idx = key >> 5;
            uint32_t mask = 1 << (key & 0b00011111);
            auto& flags = hadKey[idx];
            if (flags & mask)
            {
                continue;
            }
            flags |= mask;
            uint16_t len = *((uint16_t*)(srcEnd - 2));
            uint8_t* data = srcEnd - 4 + (len & 0x1) - len;
            setValue(key, data, len, true);

            srcEnd = getPrevEntryEnd(srcEnd, srcPage);
            assert(srcEnd != (uint8_t*)-1);
        }
        assert(srcEnd == srcPage);
        Driver::write16(mActivePage-6,

        auto end = mDataEnd;
        while (end > mActivePage)
        {

        auto

        isPage

        struct EntryInfo
        {
            uint16_t dataOfs;
            uint16_t num;
        };
        EntryInfo entries[256];


};

#endif
