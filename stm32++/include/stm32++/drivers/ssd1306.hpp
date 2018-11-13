/**
 * based on work from:  Tilen Majerle<tilen@majerle.eu>
 * author: Alexander Vassilev

   ----------------------------------------------------------------------
    Copyright (C) Alexander Lutsai, 2016
    Copyright (C) Alexander Vassilev, 2017

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
   ----------------------------------------------------------------------
 */
#ifndef SSD1306_HPP_INCLUDED
#define SSD1306_HPP_INCLUDED

#include <libopencm3/stm32/i2c.h>
#include <stm32++/timeutl.hpp>
#include <stm32++/font.hpp>
#include <string.h>

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

/* Absolute value */
#define ABS(x)   ((x) > 0 ? (x) : -(x))

enum Color: bool
{
    kColorBlack = false,
    kColorWhite = true
};
enum { kOptExternVcc = 1 };
template <class IO, uint16_t W, uint16_t H, uint8_t Opts=0>
class SSD1306
{
protected:
    enum: uint8_t {
        kFontHspaceMask = 0x3,
        kFontHspaceOff = 0,
        kFontHspace1 = 1,
        kFontHspace2 = 2,
        kFontHspace3 = 3,
        kStateInitialized = 4,
        kStateInverted = 8
    };
    /* SSD1306 data buffer */
    enum: uint16_t { kBufSize = W * H / 8 };
    uint8_t mBuf[kBufSize];
    IO& mIo;
    uint16_t mCurrentX;
    uint16_t mCurrentY;
    uint8_t mState = kFontHspace2;
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
    bool isInverted() const { return mState & kStateInverted; }
    SSD1306(IO& intf, uint8_t addr=0x3C): mIo(intf), mAddr(addr) {}
    bool init()
    {
        /* Check if LCD connected to I2C */
        if (!mIo.isDeviceConnected(mAddr))
            return false;
        /* A little delay */
        usDelay(400);

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

        /* Clear screen */
        fill(kColorBlack);

        /* Update screen */
        updateScreen();

        /* Set default values */
        mCurrentX = 0;
        mCurrentY = 0;

        /* Initialized OK */
        mState |= kStateInitialized;
        return true;
    }
    void setContrast(uint8_t val)
    {
        cmd(SSD1306_SETCONTRAST, val);    // 0x81
    }
    void updateScreen()
    {
        cmd(SSD1306_COLUMNADDR, 0, W-1);
        cmd(SSD1306_PAGEADDR, 0, H/8-1);

        mIo.startSend(mAddr, false);
        mIo.sendByte(0x40);
        if (mIo.hasTxDma)
        {
            mIo.dmaSend(mBuf, W*H/8, nullptr);
        }
        else
        {
            mIo.blockingSend(mBuf, W*H/8);
            mIo.stop();
        }
    }
    template <class... Args>
    void cmd(Args... args)
    {
        mIo.startSend(mAddr);
        mIo.sendByte(0);
        mIo.sendByte(args...);
        mIo.stop();
    }

void toggleInvert(void)
{
    if (mState & kStateInverted)
        mState &= ~kStateInverted;
    else
        mState |= kStateInverted;

    /* Do memory toggle */
    for (int i = 0; i < sizeof(mBuf); i++)
    {
        auto& byte = mBuf[i];
        byte = ~byte;
    }
}

void fill(Color color)
{
    /* Set memory */
    memset(mBuf, (color == kColorBlack) ? 0x00 : 0xFF, sizeof(mBuf));
}

void setPixel(uint16_t x, uint16_t y, uint8_t color)
{
    if (x >= W || y >= H) {
        /* Error */
        tprintf("out of range\n");
        return;
    }

    /* Check if pixels are inverted */
    if (isInverted()) {
        color = !color;
    }

    /* Set color */
    if (color == kColorWhite) {
        mBuf[x + (y / 8) * W] |= 1 << (y % 8);
    } else {
        mBuf[x + (y / 8) * W] &= ~(1 << (y % 8));
    }
}

void gotoXY(uint16_t x, uint16_t y)
{
    /* Set write pointers */
    mCurrentX = x;
    mCurrentY = y;
}

uint8_t putc(char ch, const Font& font, Color color = kColorWhite)
{
    if (isInverted())
    {
        color = (Color)!color;
    }
    uint8_t fontW = font.width;
    uint8_t symPages = (font.height+7)/8;
    uint8_t symLastPage = symPages - 1;
    const uint8_t* sym = font.data + (ch - 32) * fontW * symPages;
    uint8_t* bufDest = mBuf + (mCurrentY >> 3) * W + mCurrentX; //(ypos div 8) * width + xpos

    uint8_t writeWidth;
    if (mCurrentX + fontW < W)
    {
        writeWidth = fontW;
    }
    else
    {
        if (mCurrentX >= W)
        {
            return 0;
        }
        writeWidth = W - mCurrentX;
    }
    uint8_t vOfs = (mCurrentY % 8); //offset within the vertical byte
    if (vOfs == 0)
    {
        for (int page = 0; page <= symLastPage; page++)
        {
            uint8_t* dest = bufDest+page*W;
            const uint8_t* src = sym+page*fontW;
            const uint8_t* srcEnd = src+writeWidth;
            uint8_t wmask = (page < symLastPage)
                ? 0xff
                : 0xff >> (8 - font.height);

            for (; src < srcEnd; src++, dest++)
            {
                *dest = (*dest & ~wmask) | ((color ? (*src) : (~*src)) & wmask);
            }
        }
    }
    else
    {
        uint8_t displayFirstPageHeight = 8 - vOfs; // write height of first display page
        uint8_t firstLineFullMask = 0xff << vOfs; //masks the bits above the draw line
        bool onlyFirstLine = font.height <= displayFirstPageHeight;
        uint8_t wmask = onlyFirstLine //write mask
             ? ((1 << font.height)-1) << vOfs //the font doesn't span to line bottom, if font height is small
             : firstLineFullMask; //font spans to bottom of line
        uint8_t* wptr = bufDest; //write pointer
        uint8_t* wend = wptr+writeWidth;
        const uint8_t* rptr = sym; //read pointer
        if (color)
        {
            while(wptr < wend)
            {
                uint8_t b = *wptr;
                b = (b &~ wmask) | ((*(rptr++) << vOfs) & wmask);
                *(wptr++) = b;
            }
        }
        else
        {
            while(wptr < wend)
            {
                uint8_t b = *wptr;
                b = (b &~ wmask) | (((~*(rptr++)) << vOfs) & wmask);
                *(wptr++) = b;
            }
        }
        if (onlyFirstLine)
            return writeWidth; //we don't span on the next byte (page)

        uint8_t wpage = 1;
        uint8_t secondPageMask = 0xff << displayFirstPageHeight; // low vOfs bits set
        uint8_t symLastPageHeight = font.height % 8;
        if (symLastPageHeight == 0)
        {
            symLastPageHeight = font.height;
        }
        uint8_t symLastPageToFirstDisplayPageMask;
        uint8_t symLastPageToSecondDisplayPageMask;
        if (symLastPageHeight >= displayFirstPageHeight)
        {
            symLastPageToFirstDisplayPageMask = firstLineFullMask;
            symLastPageToSecondDisplayPageMask = 0xff >> (8-(symLastPageHeight-displayFirstPageHeight));
        }
        else
        {
            symLastPageToFirstDisplayPageMask = firstLineFullMask & (0xff >> (8-symLastPageHeight));
            symLastPageToSecondDisplayPageMask = 0x00;
        }
        wptr = bufDest+wpage*W;
        wend = wptr+writeWidth;
        assert(wend <= mBuf+kBufSize);

        for (int8_t rpage = 0; rpage <= symLastPage; rpage++)
        {
            rptr = sym + rpage * fontW;
            while(wptr < wend)
            {
                uint8_t b = *wptr;
                if (wpage > rpage)
                {
                    // we are drawing the bottom part of the symbol page,
                    // to the top part of the next display page.
                    uint8_t srcByte = color
                        ? (*(rptr++) >> displayFirstPageHeight)
                        : ((~*(rptr++)) >> displayFirstPageHeight);
                    wmask = (rpage < symLastPage)
                       ? secondPageMask // not the last symbol page, so copy all 8 bits
                       : symLastPageToSecondDisplayPageMask; //the last symbol page

                    b = (b & ~wmask) | (srcByte & wmask);
                }
                else
                {
                    // we are drawing the top part of the symbol page,
                    // to the bottom part of the same display page
                    wmask = (rpage < symLastPage)
                       ? firstLineFullMask // not the last symbol page, so copy all 8 bits
                       : symLastPageToFirstDisplayPageMask; //the last symbol page
                    uint8_t srcByte = color
                        ? ((*(rptr++) << vOfs))
                        : ((~*(rptr++) << vOfs));
                    b = (b & ~wmask) | srcByte;
                    wpage++;
                    wptr = bufDest +wpage * W;
                    wend = wptr + writeWidth;
                    assert(wend <= mBuf+kBufSize);
                }
                *(wptr++) = b;
            }
        }
    }
    return writeWidth;
}

template <class F>
bool puts(const char* str, const F& font, Color color=kColorWhite)
{
    while (*str)
    {
        /* Write character by character */
        auto writeWidth = putc(*str, font, color);
        if (!writeWidth) {
            return false;
        }
        mCurrentX += writeWidth + (mState & kFontHspaceMask);
        str++;
    }
    return true;
}

void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t c)
{
    int16_t dx, dy, sx, sy, err, e2, i, tmp;

    /* Check for overflow */
    if (x0 >= W) {
        x0 = W - 1;
    }
    if (x1 >= W) {
        x1 = W - 1;
    }
    if (y0 >= H) {
        y0 = H - 1;
    }
    if (y1 >= H) {
        y1 = H - 1;
    }

    dx = (x0 < x1) ? (x1 - x0) : (x0 - x1);
    dy = (y0 < y1) ? (y1 - y0) : (y0 - y1);
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = ((dx > dy) ? dx : -dy) / 2;

    if (dx == 0) {
        if (y1 < y0) {
            tmp = y1;
            y1 = y0;
            y0 = tmp;
        }

        if (x1 < x0) {
            tmp = x1;
            x1 = x0;
            x0 = tmp;
        }

        /* Vertical line */
        for (i = y0; i <= y1; i++) {
            setPixel(x0, i, c);
        }

        /* Return from function */
        return;
    }

    if (dy == 0) {
        if (y1 < y0) {
            tmp = y1;
            y1 = y0;
            y0 = tmp;
        }

        if (x1 < x0) {
            tmp = x1;
            x1 = x0;
            x0 = tmp;
        }

        /* Horizontal line */
        for (i = x0; i <= x1; i++) {
            setPixel(i, y0, c);
        }

        /* Return from function */
        return;
    }

    while (1) {
        setPixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = err;
        if (e2 > -dx) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy) {
            err += dx;
            y0 += sy;
        }
    }
}

void drawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t c)
{
    /* Check input parameters */
    if (
        x >= W ||
        y >= H
    ) {
        /* Return error */
        return;
    }

    /* Check width and height */
    if ((x + w) >= W) {
        w = W - x;
    }
    if ((y + h) >= H) {
        h = H - y;
    }

    /* Draw 4 lines */
    drawLine(x, y, x + w, y, c);         /* Top line */
    drawLine(x, y + h, x + w, y + h, c); /* Bottom line */
    drawLine(x, y, x, y + h, c);         /* Left line */
    drawLine(x + w, y, x + w, y + h, c); /* Right line */
}

void drawFilledRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t c)
{
    uint8_t i;

    /* Check input parameters */
    if (
        x >= W ||
        y >= H
    ) {
        /* Return error */
        return;
    }

    /* Check width and height */
    if ((x + w) >= W) {
        w = W - x;
    }
    if ((y + h) >= H) {
        h = H - y;
    }

    /* Draw lines */
    for (i = 0; i <= h; i++) {
        /* Draw lines */
        drawLine(x, y + i, x + w, y + i, c);
    }
}

void drawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint8_t color)
{
    /* Draw lines */
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x3, y3, color);
    drawLine(x3, y3, x1, y1, color);
}


void drawFilledTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint8_t color)
{
    int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0,
    yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0,
    curpixel = 0;

    deltax = ABS(x2 - x1);
    deltay = ABS(y2 - y1);
    x = x1;
    y = y1;

    if (x2 >= x1) {
        xinc1 = 1;
        xinc2 = 1;
    } else {
        xinc1 = -1;
        xinc2 = -1;
    }

    if (y2 >= y1) {
        yinc1 = 1;
        yinc2 = 1;
    } else {
        yinc1 = -1;
        yinc2 = -1;
    }

    if (deltax >= deltay){
        xinc1 = 0;
        yinc2 = 0;
        den = deltax;
        num = deltax / 2;
        numadd = deltay;
        numpixels = deltax;
    } else {
        xinc2 = 0;
        yinc1 = 0;
        den = deltay;
        num = deltay / 2;
        numadd = deltax;
        numpixels = deltay;
    }

    for (curpixel = 0; curpixel <= numpixels; curpixel++) {
        drawLine(x, y, x3, y3, color);

        num += numadd;
        if (num >= den) {
            num -= den;
            x += xinc1;
            y += yinc1;
        }
        x += xinc2;
        y += yinc2;
    }
}

void drawCircle(int16_t x0, int16_t y0, int16_t r, uint8_t c)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    setPixel(x0, y0 + r, c);
    setPixel(x0, y0 - r, c);
    setPixel(x0 + r, y0, c);
    setPixel(x0 - r, y0, c);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        setPixel(x0 + x, y0 + y, c);
        setPixel(x0 - x, y0 + y, c);
        setPixel(x0 + x, y0 - y, c);
        setPixel(x0 - x, y0 - y, c);

        setPixel(x0 + y, y0 + x, c);
        setPixel(x0 - y, y0 + x, c);
        setPixel(x0 + y, y0 - x, c);
        setPixel(x0 - y, y0 - x, c);
    }
}

void drawFilledCircle(int16_t x0, int16_t y0, int16_t r, uint8_t c)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    setPixel(x0, y0 + r, c);
    setPixel(x0, y0 - r, c);
    setPixel(x0 + r, y0, c);
    setPixel(x0 - r, y0, c);
    drawLine(x0 - r, y0, x0 + r, y0, c);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        drawLine(x0 - x, y0 + y, x0 + x, y0 + y, c);
        drawLine(x0 + x, y0 - y, x0 - x, y0 - y, c);

        drawLine(x0 + y, y0 + x, x0 - y, y0 + x, c);
        drawLine(x0 + y, y0 - x, x0 - y, y0 - x, c);
    }
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
#endif
