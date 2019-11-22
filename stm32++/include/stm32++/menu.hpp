#ifndef STM32PP_MENU_HPP
#define STM32PP_MENU_HPP
#include <stdint.h>
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
    uint8_t max;
    const char** enames;
    EnumValue(const char* aText, uint8_t aValue, const char** names)
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
        if ((void*)ChangeHandler) {
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
        if ((void*)ChangeHandler) {
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
    template<class T, typename... Args>
    void addValue(Args... args)
    {
        items.push_back(new T(args...));
    }
    Menu* submenu(const char* name)
    {
        auto item = new Menu(this, name);
        items.push_back(item);
        return item;
    }
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
struct MenuSystem: public Menu
{
    LCD& lcd;
    int16_t mTop;
    int16_t mHeight;
    Menu* mCurrentMenu = this;
    int8_t mCurrentLine = 0;
    int8_t mScrollOffset = 0;
    int8_t mMaxItems;
    uint8_t mConfig;
    MenuSystem(LCD& aLcd, const char* title, int16_t y, int16_t height=-1,
        uint8_t aConfig=kMenuNoBackButton)
    : Menu(nullptr, title), lcd(aLcd), mTop(y), mConfig(aConfig)
    {
        if (height < 0) {
            mHeight = LCD::height() - mTop;
        } else {
            xassert(mTop + mHeight <= LCD::height());
            mHeight = height;
        }
        if (aConfig & kMenuNoBackButton) {
            items.push_back(nullptr);
        }
    }
    void goToLine(uint8_t line)
    {
        if (mCurrentLine > -1) {
            //
        }
    }
    void render()
    {
        mMaxItems = (mHeight - 5) / (lcd.font().height + 1) - 1;
        lcd.clear();
        auto y = mTop;
        lcd.putsCentered(y, text);
        y += lcd.font().height + 2;
        lcd.hLine(0, lcd.width()-1, y);
        y += 3;
        uint8_t endItem = mScrollOffset + mMaxItems;
        if (items.size() < endItem) {
            endItem = items.size();
        }
        for (uint8_t i = mScrollOffset; i < endItem; i++) {
            lcd.gotoXY(0, y);
            auto item = items[i];
            if (!item) {
                lcd.puts("< Back");
            } else {
                lcd.puts(item->text);
                if (item->flags & Item::kIsMenu) {
                    lcd.puts(" -->");
                } else {
                    lcd.putsRAligned(y, static_cast<IValue*>(item)->strValue());
                }
            }
            y += lcd.font().height + 1;
        }
    }
};
}

#endif
