#include <stm32++/flash.hpp>
#include <string>

uint16_t page1[512];
uint16_t page2[512];

FlashValueStore<> store;
std::string values[] = {
    "this is a test message",
    "new value",
    "I am having a huge list of items (15000) to be populated on the items drop down in the front end.",
    "Hence I have made an AJAX call (triggered upon a selection of a Company) and this AJAX call to made",
    "to an action method in the Controller and this action method populates the list of service items and",
    "returns it back to the AJAX call via response. This is where my AJAX call is failing.",
    "If i have about 100 - 500 items, the ajax call works. How do I fix this issue?"
};
bool setString(uint8_t key, const std::string& val)
{
    return store.setValue(key, val.c_str(), val.size(), false);
}
int main()
{
    store.init((size_t)page1, (size_t)page2);
    setString(0xab, values[0]);
    setString(0xab, values[1]);
    setString(0xab, values[2]);
    setString(0xab, values[3]);

    return 0;
}
