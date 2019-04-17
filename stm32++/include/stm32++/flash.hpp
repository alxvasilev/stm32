#ifndef FLASH_HPP_INCLUDED
#define FLASH_HPP_INCLUDED

#include <libopencm3/stm32/flash.h>

template<int PageSize, uint32_t Page1Addr, uint32_t Page2Addr>
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
    static constexpr char kPageMagicSig[] = "nvstor";
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
        bool page1Valid = verifyPageChecksum(Page1);
        bool page2Valid = verifyPageChecksum(Page2);
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
            if (!initPage(Page1))
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
        if (!verifyAllEntries(end, page))
        {
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
    void* getRawValue(uint8_t key, uint16_t& size)
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
    bool setValue(uint8_t key, void* data, uint16_t size, bool isEmergency=false)
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
        uint16_t bytesNeeded = size + ((size & 1) ? 3 : 4);
        if (bytesNeeded > bytesFree)
        {
            if (!compactData()) // should log error message
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
        return true;
    }

    /**
     * @brief findDataEnd Finds where the data in a page ends, by scanning the page backwards
     * and finding the last byte with value 0xff
     * @param page The start of the page which to scan
     * @return A pointer to the first byte after the last data entry (first byte with 0xff value)
     */
    void findDataEnd(uint8_t* page)
    {
        for (uint8_t* ptr = page + PageSize - 1; ptr >= page; ptr--)
        {
            if (*ptr != 0xff)
            {
                return ptr + 1;
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
};

#endif
