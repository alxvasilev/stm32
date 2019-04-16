/**
 * @author: Alexander Vassilev
 * @copyright BSD License
 */
#ifndef SSD1306_HPP_INCLUDED
#define SSD1306_HPP_INCLUDED

//#include <libopencm3/stm32/i2c.h>
#include <stm32++/timeutl.hpp>
#include <stm32++/common.hpp>
#include <string.h>
#include <stm32++/gfx.hpp>

#define SSD1306_SETCONTRAST 0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF

#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA

#define SSD1306_SETVCOMDETECT 0xDB

#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9

#define SSD1306_SETMULTIPLEX 0xA8

#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10

#define SSD1306_SETSTARTLINE 0x40

#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR   0x22

#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8

#define SSD1306_SEGREMAP 0xA0

#define SSD1306_CHARGEPUMP 0x8D

#define SSD1306_EXTERNALVCC 0x1
#define SSD1306_SWITCHCAPVCC 0x2

// Scrolling #defines
#define SSD1306_ACTIVATE_SCROLL 0x2F
#define SSD1306_DEACTIVATE_SCROLL 0x2E
#define SSD1306_SET_VERTICAL_SCROLL_AREA 0xA3
#define SSD1306_RIGHT_HORIZONTAL_SCROLL 0x26
#define SSD1306_LEFT_HORIZONTAL_SCROLL 0x27
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A

enum { kOptExternVcc = 1 };
template <class IO, uint16_t W, uint16_t H, uint8_t Opts=0>
class SSD1306_Driver
{
protected:
    /* SSD1306 data buffer */
    enum: uint16_t { kBufSize = W * H / 8 };
    uint8_t mBuf[kBufSize];
    IO& mIo;
    uint8_t mAddr;
    constexpr static uint16_t mkType(uint8_t w, uint8_t h) { return (w << 8) | h; }
public:
    enum: uint16_t {
        kType = mkType(W, H),
        SSD1306_128_32 = mkType(128, 32),
        SSD1306_128_64 = mkType(128, 64),
        SSD1306_96_16 = mkType(96,16)
    };
    uint8_t* rawBuf() { return mBuf; }
    static uint16_t width() { return W; }
    static uint16_t height() { return H; }
    SSD1306_Driver(IO& intf, uint8_t addr=0x3C): mIo(intf), mAddr(addr) {}
    bool init()
    {
        /* Check if LCD connected to I2C */
        if (!mIo.isDeviceConnected(mAddr))
            return false;
        /* LCD needs some time after initial power up */

        // Init sequence
        cmd(SSD1306_DISPLAYOFF);               // cmd 0xAE
        cmd(SSD1306_SETDISPLAYCLOCKDIV, 0xf0); // cmd 0xD5, the suggested ratio 0x80

        cmd(SSD1306_SETMULTIPLEX, H - 1);      // cmd 0xA8
        cmd(SSD1306_SETDISPLAYOFFSET, 0x0);    // cmd 0xD3, no offset
        cmd(SSD1306_SETSTARTLINE | 0x0);       // line #0
        cmd(SSD1306_MEMORYMODE, 0x00);         // cmd 0x20, 0x0 horizontal then vertical increment, act like ks0108
        cmd(SSD1306_SEGREMAP | 0x1);
        cmd(SSD1306_COMSCANDEC);
        if (kType == SSD1306_128_32)
        {
            cmd(SSD1306_SETCOMPINS, 0x02);     // 0xDA
        }
        else if (kType == SSD1306_128_64)
        {
            cmd(SSD1306_SETCOMPINS, 0x12);
        }
        else if (kType == SSD1306_96_16)
        {
            cmd(SSD1306_SETCOMPINS, 0x02);
        }

        cmd(SSD1306_SETPRECHARGE, Opts&kOptExternVcc ? 0x22 : 0xF1);  // 0xd9
        cmd(SSD1306_SETVCOMDETECT, 0x40);                 // 0xDB
        cmd(SSD1306_DISPLAYALLON_RESUME);           // 0xA4
        cmd(SSD1306_NORMALDISPLAY);                 // 0xA6

        cmd(SSD1306_DEACTIVATE_SCROLL);
        setContrast(0x8F);
        cmd(SSD1306_CHARGEPUMP, Opts&kOptExternVcc ? 0x10 : 0x14); // 0x8D
        cmd(SSD1306_DISPLAYON);                     //--turn on oled panel
        return true;
    }
    void setContrast(uint8_t val)
    {
        cmd(SSD1306_SETCONTRAST, val);    // 0x81
    }
    template <bool D=HasTxDma<IO>::value>
    typename std::enable_if<D, void>::type sendBuffer()
    {
        mIo.dmaTxStart(mBuf, sizeof(mBuf), nullptr);
    }
    template <bool D=HasTxDma<IO>::value>
    typename std::enable_if<!D, void>::type sendBuffer()
    {
        mIo.blockingSend(mBuf, W*H/8);
        mIo.stop();
    }
    void updateScreen()
    {
        cmd(SSD1306_COLUMNADDR, 0, W-1);
        cmd(SSD1306_PAGEADDR, 0, H/8-1);

        mIo.startSend(mAddr, false);
        mIo.sendByte(0x40);
        sendBuffer();
    }
    template <bool D=HasTxDma<IO>::value>
    typename std::enable_if<D, void>::type waitTxComplete()
    {
        while (mIo.txBusy());
    }
    template <bool D=HasTxDma<IO>::value>
    typename std::enable_if<!D, void>::type waitTxComplete()
    {}
    template <class... Args>
    void cmd(Args... args)
    {
        waitTxComplete();
        mIo.startSend(mAddr);
        mIo.sendByte(0);
        mIo.sendByte(args...);
        mIo.stop();
    }
    void powerOn()
    {
        cmd(SSD1306_CHARGEPUMP, 0x14);
        cmd(0xAF);
    }
    void powerOff()
    {
        cmd(0xAE);
        cmd(SSD1306_CHARGEPUMP, 0x10);
    }
};

template <class IO, uint16_t W, uint16_t H, uint8_t Opts=0>
using SSD1306 = DisplayGfx<SSD1306_Driver<IO, W, H, Opts>>;
#endif
