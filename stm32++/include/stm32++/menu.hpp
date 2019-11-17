#ifndef STM32PP_MENU_HPP
#define STM32PP_MENU_HPP

namespace menu
{
enum: uint8_t Event {
    kEventLeave=1,
    kEventBtnUp=2, kEventBtnDown=3, kEventBtnOk=4, kEventBtnBack=5
};

struct Item
{
    enum: uint8_t Type { kTypeMenu=1, kTypeInt, kTypeFloat, kTypeBool, kTypeEnum };
    Type type;
    const char* text;
    Item(Type aType, const char* aText): type(aType), text(aText){}
    virtual const char* strValue() { return nullptr; }
    virtual ~Item() {}
};

template <typename T, uint8_t Id, void(*ChangeHandler)(T newVal)=nullptr>
struct Value: public Item
{
    enum: uint8_t { kValueId = Id };
    T value;
    Value(Type aType, const char* aText, T aValue)
        : Item(aType, aText), value(aValue) {}
    virtual void onEvent(Event evt) {}
};

template <typename T, uint8_t Id, void(*ChangeHandler)(T newVal)=nullptr>
struct NumValue: public Value<T, Id, ChangeHandler>
{
    enum: uint8_t { kEditBufSize = 18 };
    T min = std::numeric_limits<T>::min;
    T max = std::numeric_limits<T>::max;
    char* mEditBuf = nullptr;
    NumValue(Type aType, const char* aText, uint8_t aId, T aValue, T aMin, T aMax)
        : Value(aType, aText, aValue), min(aMin), max(aMax){}
    virtual const char* strValue()
    {
        if (!mEditBuf)
        {
            mEditBuf = new char[kEditBufSize];
            toString(mEditBuf, kEditBufSize, value);
        }
        return mEditBuf;
    }
    virtual const char* onEvent(Event evt, uint8_t cursorX)
    {
        xassert(mEditBuf);
        switch(evt)
        {
        case kEventLeave:
            delete[] mEditBuf;
            mEditBuf = nullptr;
            return nullptr;
        case kEventBtnUp:
            return onButtonUp(cursorX);
        case kEventBtnDown:
            return onButtonDown(cursorX);
        default:
            __builtin_trap();
            return;
        }
    }
    std::enable_if<std::is_floating_point<T>::value, bool> isAtMin()
    {
        return fabs(value-min) <= 0;
    }
    std::enable_if<std::is_floating_point<T>::value, bool> isAtMax()
    {
        return fabs(value-max) >= 0;
    }

    std::enable_if<!std::is_integral<T>::value, bool> isAtMin()
    {
        return value <= min;
    }
    std::enable_if<!std::is_integral<T>::value, bool> isAtMax()
    {
        return value >= max;
    }
    const char* onButtonUp(uint8_t cursorX)
    {
        if (isAtMax())
        {
            return nullptr;
        }
        value += 1;
        if (ChangeHandler)
        {
            ChangeHandler(value);
        }
        return toString(mEditBuf, kEditBufSize, value);
    }
    const char* onButtonDown(uint8_t cursorX)
    {
        if (isAtMin())
        {
            return nullptr;
        }
        value -= 1;
        if (ChangeHandler)
        {
            ChangeHandler(value);
        }
        return toString(mEditBuf, kEditBufSize, value);
    }
};

template <uint8_t ValueId, void(*ChangeHandler)(T newVal)=nullptr>
struct EnumValue: public Value<uint8_t, ValueId, ChangeHandler>
{
    const char* enames[];
    uint8_t max;
    EnumValue(const char* aText, uint8_t aValue, const char* names[])
        : Value(kTypeEnum, aText, aValue), enames(names)
    {
        uint8_t cnt = 0;
        while(*(names++)) cnt++;
        numVals = cnt;
        xassert(aValue <= max);
    }
    virtual const char* strValue()
    {
        return enames[value];
    }
    virtual const char* onEvent(Event evt, uint8_t cursorX)
    {
        switch(evt)
        {
        case kEventLeave:
            return nullptr;
        case kEventBtnUp:
            return onButtonUp(cursorX);
        case kEventBtnDown:
            return onButtonDown(cursorX);
        default:
            __builtin_trap();
            return;
        }
    }
    const char* onButtonUp(uint8_t cursorX)
    {
        if (value == max)
        {
            value = 0;
        }
        else
        {
            value++;
        }
        if (ChangeHandler)
        {
            ChangeHandler(value);
        }
        return enames[value];
    }
    const char* onButtonDown(uint8_t cursorX)
    {
        if (value == 0)
        {
            value = max;
        }
        else
        {
            value--;
        }
        if (ChangeHandler)
        {
            ChangeHandler(value);
        }
        return enames[value];
    }
};

template <uint8_t ValueId, void(*ChangeHandler)(T newVal)=nullptr>
struct BoolValue: public EnumValue<uint8_t, ValueId, ChangeHandler>
{
    BoolValue(const char* aText, uint8_t aValue, const char* names[]=nullptr["yes", "no"])
    : EnumValue(aText, aValue, names){}
};

struct Menu: public MenuItem
{
    Menu* parentMenu;
    std::vector<MenuItem*> items;
    Menu(Menu* aParent, const char* aText)
        : Item(kTypeMenu, aText), parentMenu(aParent)
    {}
    ~Menu()
    {
        for (auto item: items)
        {
            delete item;
        }
        items.clear();
    }
};

enum: uint8_t { kMenuNoBackButton = 1 };
template <class LCD>
struct MenuSystem
{
    LCD& lcd;
    Menu menu;
    Menu* mCurrent = &menu;
    uint8_t mScrollOffset = 0;
    uint8_t mMaxItems;
    uint8_t mConfig;
    uint8_t textWidth = 80;
    MenuSystem(LCD& aLcd, const char* title, uint8_t aConfig=kMenuNoBackButton)
        : lcd(aLcd), menu(nullptr, title), mConfig(aConfig)
    {
        mMaxItems = ((lcd.height() - 2) / lcd.font.height() - 1);
        if (config & kMenuNoBackButton)
        {
            menu.items.push_back(nullptr);
        }
    }
    void render()
    {
        lcd.clear();
        lcd.putsCentered(y, title);
        lcd.hLine(0, lcd.width()-1, lcd.font().height());
        int16_t y = lcd.font.height() + 2;
        auto end = mScrollOffset + mMaxItems;
        for (uint8_t i = mScrollOffset; i < end; i++)
        {
            lcd.gotoXY(0, y);
            auto item = items[i];
            if (!item)
            {
                lcd.puts("< Back", textWidth);
            }
            else
            {
                lcd.puts(item->text, textWidth);
                if (item->type != Item::kTypeMenu)
                {
                    lcd.gotoXY(y, textWidth);
                    lcd.puts(item->strValue());
                }
            }
        }
    }
};
}

#endif
