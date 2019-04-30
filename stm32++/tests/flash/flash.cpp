#define STM32PP_FLASH_SIMULATE_POWER_LOSS
#include <stm32++/flash.hpp>
#include <string>
using namespace flash;

uint16_t page1[512];
uint16_t page2[512];
std::string dumpPage(uint8_t* page)
{
    std::string ret;
    auto start = page;
    auto end = page + 1024;
    for (int col = 0; page < end; page++, col++)
    {
        if (col >= 20)
        {
            ret.append("  ").append(std::to_string(page - start)).append("\r\n");
            col = 0;
        }
        auto ch = *page;
        if (ch >= 32 && ch < 129)
        {
            ret.append("  ") += ch;
        }
        else
        {
            char buf[4];
            snprintf(buf, 4, "%02x", ch);
            ret.append(" ").append(buf);
        }
    }
    return ret;
}
std::string dumpPage(uint16_t* page)
{
    return dumpPage((uint8_t*) page);
}

FlashValueStore<> store;
std::string values[] = {
    "this is a test message",
    "new value",
    "I am having a huge list of items (15000) to be populated on the items drop down in the front end.",
    "Hence I have made an AJAX call (triggered upon a selection of a Company) and this AJAX call to made",
    "to an action method in the Controller and this action method populates the list of service items and",
    "returns it back to the AJAX call via response. This is where my AJAX call is failing.",
    "If i have about 100 - 500 items, the ajax call works. How do I fix this issue?",
    "Here, you specify the length as an int argument to printf(), which treats the '*' in the format as a request to get the length from an argument.",
    "shouldn't be necessary unless the compiler is far more broken than not implicitly converting char arguments to int.",
    "suggests that it is in fact not doing the conversion, and picking up the other 8 bits from trash on the stack or left over in a register",
    "Incorrect. Since printf is a variadic function, arguments of type char or unsigned char are promoted to int",
    "unsigned char gets promoted to int because printf() is a variadic function (assuming <stdio.h> is included). If the header isn't included, then (a) it should be and (b) you don't have a prototype in scope so the unsigned char will still be promoted to int",
    "Here, you specify the length as an int argument to printf(), which treats the '*' in the format as a request to get the length from an argument.",
    "shouldn't be necessary unless the compiler is far more broken than not implicitly converting char arguments to int.",

    "to an action method in the Controller and this action method populates the list of service items and",
    "returns it back to the AJAX call via response. This is where my AJAX call is failing.",
    "If i have about 100 - 500 items, the ajax call works. How do I fix this issue?",
    "Here, you specify the length as an int argument to printf(), which treats the '*' in the format as a request to get the length from an argument.",
    "shouldn't be necessary unless the compiler is far more broken than not implicitly converting char arguments to int.",
    "suggests that it is in fact not doing the conversion, and picking up the other 8 bits from trash on the stack or left over in a register",

};
bool setString(uint8_t key, const std::string& val)
{
    if (val.size() > 255)
    {
        printf("WARN: setString: string length is more than 255 bytes");
    }
    return store.setValue(key, val.c_str(), val.size(), false);
}
std::string getString(uint8_t key)
{
    uint8_t len;
    uint8_t* data = store.getRawValue(key, len);
    if (!data) {
        assert(len == 0); // len is a fatal error
        return std::string();
    } else {
        return std::string((const char*)data, len);
    }
}
int32_t DefaultFlashDriver::failAtWriteNum = 0x7fffffff;

int main()
{
    for (int32_t failWord = 0; failWord < 1400; failWord++)
    {
        printf("==== %d press any key ====", failWord);
        getchar();
        std::string lastWrittenOk;
        try
        {
            DefaultFlashDriver::failAtWriteNum = failWord;
            store.init((size_t)page1, (size_t)page2);
            for (auto& str: values) {
                setString(0xab, str);
                lastWrittenOk = str;
            }
        }
        catch(int)
        {
            auto pageToDump = store.activePage();
            if (!pageToDump) {
                pageToDump = (uint8_t*)page1;
            }
            if (pageToDump == (uint8_t*)page1) {
                printf("exception %d caught, page1 is active:\n%s\n", failWord, dumpPage(pageToDump).c_str());
            } else {
                printf("exception %d caught, page2 is active:\n%s\n", failWord, dumpPage(pageToDump).c_str());
                //printf("page1:\n%s\n", dumpPage(page1).c_str());
            }
            DefaultFlashDriver::failAtWriteNum = 0x7fffffff;
            store.init((size_t)page1, (size_t)page2);
            auto val = getString(0xab);
            if (val == lastWrittenOk)
            {
                printf("val = last written ok(%s)\n", val.c_str());
            } else
            {
                printf("val = %s\n", val.c_str());
            }
            DefaultFlashDriver::erasePage((uint8_t*)page1);
            DefaultFlashDriver::erasePage((uint8_t*)page2);
        }
    }
    return 0;
}
