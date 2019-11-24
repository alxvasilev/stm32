#ifndef LCDEMU_HPP
#define LCDEMU_HPP
/**
  Emulation of an LCD display and a button panel, using wxWidgets
  @author: Alexander Vassilev
  @copyright BSD License
*/

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/tglbtn.h>
#include <stm32++/button.hpp>
#include <stm32++/gfx.hpp>
#include <wx/time.h>

class BtnDriver;
struct IButtonHandler
{
protected:
    uint16_t mPort = 0;
public:
    uint16_t port() const { return mPort; }
    virtual bool onKeyEvent(bool down, int code) = 0;
};
struct ButtonAppBase: wxApp
{
    IButtonHandler* keyHandler = nullptr;
};

template <uint16_t Pins, uint16_t RptPins, uint8_t Flags=0, uint8_t DebncDelay=10>
class ButtonPanel: public wxPanel, public IButtonHandler
{
public:
    typedef ButtonPanel<Pins, RptPins, Flags, DebncDelay> Self;
    btn::Buttons<0, Pins, RptPins, Flags, 127, DebncDelay, BtnDriver> buttons;
    ButtonPanel(wxWindow* parent, btn::EventCb handler, void* userp)
    : wxPanel(parent)
    {
        auto sizer = new wxBoxSizer(wxHORIZONTAL);
        for (uint8_t pos = 0; pos < 16; pos++) {
            uint16_t mask = 1 << pos;
            if ((Pins & mask) == 0) {
                continue;
            }
            auto btn = new wxToggleButton(this, wxID_HIGHEST + 100 + pos, wxString::Format("%d", pos));
            btn->SetMinSize(wxSize(1, 30));
            sizer->Add(btn, 1, wxEXPAND);
            Bind(wxEVT_TOGGLEBUTTON, &Self::onButton, this, btn->GetId());
        }
        SetSizer(sizer);
        static_cast<ButtonAppBase*>(wxTheApp)->keyHandler = this;
        buttons.init(handler, userp);
    }
    virtual void onButton(wxCommandEvent& evt)
    {
        auto n = evt.GetId() - wxID_HIGHEST-100;
        uint16_t mask = (1 << n);
        assert(Pins & mask);
        if (evt.IsChecked()) {
            mPort |= mask;
        } else {
            mPort &= ~mask;
        }
    }
    virtual bool onKeyEvent(bool down, int code)
    {
        if (code < '0' || code > '9') {
            return false;
        }
        int n = code - '0';
        uint16_t mask = (1 << n);
        if ((Pins & mask) == 0) {
            return false;
        }
        auto btn = (wxToggleButton*)wxWindow::FindWindowById(wxID_HIGHEST + 100 + n);
        if (!btn) {
            return false;
        }
        auto val = (mPort & mask) != 0;
        if (down == val) {
            return true;
        }
        if (down) {
            mPort |= mask;
        } else {
            mPort &= ~mask;
        }
        btn->SetValue(down);
        return true;
    }
};
extern uint16_t pinStates;

struct BtnDriver
{
    static uint32_t now()
    {
        return wxGetUTCTimeMillis().GetValue() & 0xffffffff;
    }
    static uint32_t ms10ElapsedSince(uint32_t sinceTicks)
    {
        auto now = BtnDriver::now();
        if (now >= sinceTicks)
        {
            return (now - sinceTicks) / 10;
        }
        else // DwtCounter wrap
        {
            return (((uint64_t)now + 0xffffffff) - sinceTicks) / 10;
        }
    }
    static uint32_t ticksToMs(uint32_t ticks) { return ticks; }
    static bool isIrqEnabled(uint8_t irqn) { return false; }
    static void enableIrq(uint8_t irqn) { }
    static void disableIrq(uint8_t irqn) { }
    static void gpioSetPuPdInput(uint32_t port, uint16_t pins, int pullUp) {}
    static void gpioSetFloatInput(uint32_t port, uint16_t pins) {}
    static uint16_t gpioRead(uint32_t port)
    {
        return static_cast<ButtonAppBase*>(wxTheApp)->keyHandler->port();
    }
};

template <int16_t Width=128, int16_t Height=64>
class LcdDriver : public wxPanel
{
public:
    enum: uint8_t { kNumPages = Height / 8 };
    enum { kBufSize = kNumPages * Width }; // LCD Driver API
    enum: uint8_t { kFrameWidth = 4, kLcdBorderWidth = 8, kBorderWidth = kFrameWidth+kLcdBorderWidth };
    uint8_t mBuf[kBufSize];
    wxColor mPixelColor = wxColor(0x50, 0x50, 0x50);
    bool mIsARLocked = true;
    // LCD driver API
    int16_t width() { return Width; }
    int16_t height() { return Height; }
    uint8_t* rawBuf() { return mBuf; }
    void updateDisplay()
    {
        Refresh();
        Update();
        //wxClientDC dc(this);
        //render(dc);
    }
    void setContrast(uint8_t val)
    {
        val = 255 - val;
        mPixelColor.Set(0, 0, 0, val);
    }
    bool init()
    {
        memset(mBuf, 0, sizeof(mBuf));
        return true;
    }
    //====
    static constexpr wxSize minSize()
    {
        return wxSize(Width + 6*kBorderWidth, Height + 6*kBorderWidth);
    }
    static constexpr wxSize sizeInc() { return wxSize(Width, Height); }

    LcdDriver(wxWindow* parent): wxPanel(parent)
    {
        static_assert(Height % 8 == 0);
        // connect event handlers
        Connect(wxEVT_PAINT, wxPaintEventHandler(LcdDriver::paintEvent));
    }
    void paintEvent(wxPaintEvent & evt)
    {
        wxPaintDC dc(this);
        render(dc);
    }
    void render(wxDC& dc)
    {
        auto canvSize = dc.GetSize();
        auto pixelWidth = (canvSize.GetWidth()  - kBorderWidth * 2) / Width;
        if (pixelWidth < 1) {
            return;
        }
        auto pixelHeight = (canvSize.GetHeight() - kBorderWidth * 2) / Height;
        if (pixelHeight < 1) {
            return;
        }
        if (mIsARLocked) {
            pixelHeight = pixelWidth = std::min(pixelWidth, pixelHeight);
        }
        auto imageWidth = Width * pixelWidth;
        auto imageHeight = Height * pixelHeight;

        auto hPad = (canvSize.GetWidth() - imageWidth) / 2;
        auto vPad = (canvSize.GetHeight() - imageHeight) / 2;
        dc.SetPen(wxPen(wxColor(0x00, 0x20, 0x20), kFrameWidth));
        dc.DrawRectangle(hPad-kLcdBorderWidth - kFrameWidth/2, vPad-kLcdBorderWidth-kFrameWidth/2,
            imageWidth+kBorderWidth*2-kFrameWidth, imageHeight+kBorderWidth*2-kFrameWidth);
        dc.GradientFillLinear(
            wxRect(hPad-kLcdBorderWidth, vPad-kLcdBorderWidth, imageWidth+kLcdBorderWidth*2, imageHeight+kLcdBorderWidth*2),
            wxColor(0xf1, 0xf7, 0xde), wxColor(0xcf, 0xd9, 0xb0), wxBOTTOM
        );

        dc.SetBrush(wxBrush(mPixelColor));
        dc.SetPen(wxPen(mPixelColor, 1));

        auto pixPtr = mBuf;
        for (uint8_t page = 0; page < kNumPages; page++) {
            for (int16_t x = 0; x < Width; x++) {
                auto pixels = *pixPtr++;
                uint8_t mask = 0x01;
                for (uint8_t bit = 0; bit < 8; bit++) {
                    if (pixels & mask) {
                        dc.DrawRectangle(
                            hPad + x * pixelWidth, vPad + (page * 8 + bit) * pixelHeight,
                            pixelWidth, pixelHeight);
                    }
                    mask <<= 1;
                }
            }
        }
    }
};

template<uint16_t Width, uint16_t Height, uint16_t Pins, uint16_t RptPins,
         uint8_t Flags=0>
class LcdPanel: public wxPanel
{
public:
    typedef DisplayGfx<LcdDriver<Width, Height>> LcdDisplay;
    typedef ButtonPanel<Pins, RptPins, Flags> Buttons;
    LcdDisplay* lcd;
    Buttons* buttons;
    typedef LcdPanel<Width, Height, Pins, RptPins, Flags> Self;
    LcdPanel(wxWindow* parent, btn::EventCb handler)
    : wxPanel(parent)
    {
        lcd = new LcdDisplay(this);
        auto sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(lcd, 1, wxEXPAND|wxALL);
        buttons = new Buttons(this, handler, parent);
        sizer->Add(buttons, 0, wxEXPAND|wxALL);
        SetSizer(sizer);
    }
    wxSize minSize() const
    {
        auto minSize = lcd->minSize();
        return wxSize(minSize.GetWidth(), minSize.GetHeight() + buttons->GetSize().GetHeight());
    }
};

template <class App, uint16_t Width, uint16_t Height, uint16_t Pins, uint16_t RptPins,
          uint8_t Flags=0>
class ButtonFrame: public wxFrame
{
public:
    typedef ButtonFrame<App, Width, Height, Pins, RptPins, Flags> Self;
    typedef LcdPanel<Width, Height, Pins, RptPins, Flags> Panel;
    App& mApp;
    Panel* mPanel;
    decltype(mPanel->lcd) lcd;
    decltype(mPanel->buttons->buttons) buttons;
    ButtonFrame(App& app, const wxString& title, const wxPoint& pos, const wxSize& size)
        : wxFrame(NULL, wxID_ANY, title, pos, size), mApp(app)
    {
        mPanel = new Panel(this, App::buttonHandler);
        lcd = mPanel->lcd;
        buttons = mPanel->buttons->buttons;
        SetSizeHints(mPanel->minSize(), wxDefaultSize, lcd->sizeInc());
        Connect(wxEVT_SHOW, wxShowEventHandler(Self::onShow));
    }
    void onShow(wxShowEvent& evt)
    {
        Connect(wxEVT_COMMAND_ENTER, wxCommandEventHandler(Self::onStart));
        AddPendingEvent(wxCommandEvent(wxEVT_COMMAND_ENTER));
    }
    void onStart(wxCommandEvent& evt)
    {
        mApp.onStart();
    }
};
template <class App, uint16_t Width, uint16_t Height, uint16_t Pins, uint16_t RptPins,
          uint8_t Flags=0>
class ButtonApp: public ButtonAppBase
{
public:
    typedef ButtonApp<App, Width, Height, Pins, RptPins, Flags> Self;
    typedef ButtonFrame<App, Width, Height, Pins, RptPins, Flags> Frame;
    enum {kTimerId = wxID_HIGHEST + 1};
    Frame* mFrame;
    wxTimer* mTimer = nullptr;
    int FilterEvent(wxEvent& event)
    {
        auto type = event.GetEventType();
        if (type == wxEVT_KEY_DOWN)
        {
            auto code = ((wxKeyEvent&)event).GetKeyCode();
            return keyHandler->onKeyEvent(true, code) ? Event_Processed : Event_Skip;
        }
        else if (type == wxEVT_KEY_UP)
        {
            auto code = ((wxKeyEvent&)event).GetKeyCode();
            return keyHandler->onKeyEvent(false, code) ? Event_Processed : Event_Skip;
        }
        else
        {
            return Event_Skip;
        }
    }
    virtual bool OnInit()
    {
        mFrame = new Frame((App&)*this, "STM32 LCD Emulator", wxDefaultPosition, wxSize(500, 300));
        mFrame->CenterOnScreen();
        mFrame->Show(true);
        return true;
    }
    void startTimer(int ms)
    {
        if (mTimer) {
            printf("Timer already started\n");
            return;
        }
        mTimer = new wxTimer(this, kTimerId);
        mTimer->Start(ms);

        //Connect(wxID_ANY, wxEVT_IDLE, wxIdleEventHandler(Self::onIdle));
        Connect(kTimerId, wxEVT_TIMER, wxTimerEventHandler(Self::onTimer));
    }
    void stopTimer()
    {
        if (!mTimer) {
            printf("Timer has not been started\n");
            return;
        }
        mTimer->Stop();
        Disconnect(kTimerId, wxEVT_TIMER, wxTimerEventHandler(Self::onTimer));
        delete mTimer;
        mTimer = nullptr;
    }
    virtual void onTimer(wxTimerEvent& evt)
    {
        mFrame->buttons.poll();
        mFrame->buttons.process();
    }
};

#endif // LCDEMU_HPP
