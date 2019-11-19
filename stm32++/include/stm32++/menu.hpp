#ifndef STM32PP_MENU_HPP
#define STM32PP_MENU_HPP
#include <vector>

namespace nsmenu
{
enum Event: uint8_t {
    kEventLeave=1,
    kEventBtnUp=2, kEventBtnDown=3, kEventBtnOk=4, kEventBtnBack=5
};
struct Item
{
    enum Flags: uint8_t { kIsMenu = 1 };
    const char* text;
    uint8_t flags;
    Item(const char* aText, uint8_t aFlags=0): text(aText), flags(aFlags){}
    virtual ~Item() {}
};

struct IValue: public Item
{
    using Item::Item;
    virtual const char* strValue() = 0;
    virtual void* binValue(uint8_t& size) const = 0;
    virtual const char* onEvent(Event evt) = 0;
};

template <typename T, uint8_t Id, bool(*ChangeHandler)(T newVal)=nullptr>
struct Value: public IValue
{
    enum: uint8_t { kValueId = Id };
    T value;
    Value(const char* aText, T aValue): IValue(aText), value(aValue) {}
    virtual void* binValue(uint8_t& size) const
    {
        size = sizeof(T);
        return (void*)&value;
    }
};

template <typename T>
struct DefaultStep
{
    template<typename X=T>
    static constexpr typename std::enable_if<std::is_integral<X>::value, X>::type value()
    {
        return 1;
    }
    template<typename X=T>
    static constexpr typename std::enable_if<!std::is_integral<X>::value, X>::type value()
    {
        return 0.1;
    }
};

template <typename T, uint8_t Id,
    bool(*ChangeHandler)(T newVal)=nullptr,
    T Step=DefaultStep<T>::value(),
    T Min=std::numeric_limits<T>::min(), T Max=std::numeric_limits<T>::max()>
struct NumValue: public Value<T, Id, ChangeHandler>
{
    enum: uint8_t { kEditBufSize = 18 };
    char* mEditBuf = nullptr;
    NumValue(const char* aText, T defValue)
        : Value<T, Id, ChangeHandler>(aText, defValue){}
    virtual const char* strValue()
    {
        if (!mEditBuf) {
            mEditBuf = new char[kEditBufSize];
            toString(mEditBuf, kEditBufSize, this->value);
        }
        return mEditBuf;
    }
    virtual const char* onEvent(Event evt)
    {
        xassert(mEditBuf);
        switch(evt)
        {
        case kEventLeave:
            delete[] mEditBuf;
            mEditBuf = nullptr;
            return nullptr;
        case kEventBtnUp:
            return onButtonUp();
        case kEventBtnDown:
            return onButtonDown();
        default:
            __builtin_trap();
            return nullptr;
        }
    }
    const char* onButtonUp()
    {
        if (this->value >= Max) {
            return nullptr;
        }
        if (!(void*)ChangeHandler) {
            this->value += Step;
        } else {
            auto newVal = this->value + Step;
            if (ChangeHandler(newVal)) {
                this->value = newVal;
            } else {
                return nullptr;
            }
        }
        return toString(mEditBuf, kEditBufSize, this->value);
    }
    const char* onButtonDown()
    {
        if (this->value <= Min) {
            return nullptr;
        }
        if (!(void*)ChangeHandler) {
            this->value -= Step;
        } else {
            auto newVal = this->value - Step;
            if (!ChangeHandler(newVal)) {
                return nullptr;
            } else {
                this->value = newVal;
            }
        }
        return toString(mEditBuf, kEditBufSize, this->value);
    }
};

template <uint8_t ValueId, bool(*ChangeHandler)(uint8_t newVal)=nullptr>
struct EnumValue: public Value<uint8_t, ValueId, ChangeHandler>
{
    const char* enames[];
    uint8_t max;
    EnumValue(const char* aText, uint8_t aValue, const char* names[])
        : Value<uint8_t, ValueId, ChangeHandler>(aText, aValue), enames(names)
    {
        uint8_t cnt = 0;
        while(*(names++)) cnt++;
        max = cnt;
        xassert(aValue <= max);
    }
    virtual const char* strValue()
    {
        return enames[this->value];
    }
    virtual const char* onEvent(Event evt)
    {
        switch(evt)
        {
        case kEventLeave:
            return nullptr;
        case kEventBtnUp:
            return onButtonUp();
        case kEventBtnDown:
            return onButtonDown();
        default:
            __builtin_trap();
            return nullptr;
        }
    }
    const char* onButtonUp()
    {
        auto newVal = (this->value == max) ? 0 : this->value+1;
        if (ChangeHandler) {
            if (!ChangeHandler(newVal)) {
                return nullptr;
            }
        }
        this->value = newVal;
        return enames[this->value];
    }
    const char* onButtonDown()
    {
        auto newVal = (this->value == 0) ? max : this->value-1;
        if (ChangeHandler) {
            if (!ChangeHandler(newVal)) {
                return nullptr;
            }
        }
        this->value = newVal;
        return enames[this->value];
    }
};

template <uint8_t ValueId, bool(*ChangeHandler)(uint8_t newVal)=nullptr>
struct BoolValue: public EnumValue<ValueId, ChangeHandler>
{
    BoolValue(const char* aText, uint8_t aValue, const char* names[]={"yes", "no"})
    : EnumValue<ValueId, ChangeHandler>(aText, aValue, names){}
};

struct Menu: public Item
{
    Menu* parentMenu;
    std::vector<Item*> items;
    Menu(Menu* aParent, const char* aText)
    : Item(aText, kIsMenu), parentMenu(aParent)
    {}
    ~Menu()
    {
        for (auto item: items) {
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
        mMaxItems = ((lcd.height() - 2) / lcd.font().height - 1);
        if (aConfig & kMenuNoBackButton) {
            menu.items.push_back(nullptr);
        }
    }
    void render()
    {
        lcd.clear();
        lcd.putsCentered(0, menu.text);
        lcd.hLine(0, lcd.width()-1, lcd.font().height);
        int16_t y = lcd.font().height + 2;
        uint8_t end = mScrollOffset + mMaxItems;
        if (menu.items.size() < end) {
            end = menu.items.size();
        }
        for (uint8_t i = mScrollOffset; i < end; i++) {
            lcd.gotoXY(0, y);
            auto item = menu.items[i];
            if (!item) {
                lcd.puts("< Back", textWidth);
            } else {
                lcd.puts(item->text, textWidth);
                if (!(item->flags & Item::kIsMenu)) {
                    lcd.gotoXY(y, textWidth + 2);
                    lcd.puts(static_cast<IValue*>(item)->strValue());
                }
            }
        }
    }
};
}

#endif
