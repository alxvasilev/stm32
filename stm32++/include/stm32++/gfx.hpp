/**
  LCD Graphics and text layer
  @author: Alexander Vassilev
  @copyright BSD License
 */
#ifndef GFX_HPP_INCLUDED
#define GFX_HPP_INCLUDED

#include <stm32++/xassert.hpp>
#include <string.h>
#include <stm32++/font.hpp>
#include <algorithm> //for std::swap

/* Absolute value */
#define ABS(x)   ((x) > 0 ? (x) : -(x))

#define gfx_checkbounds_x(x) if (x > Driver::width()) return;
#define gfx_checkbounds_y(y) if (y > Driver::height()) return;

enum Color: bool
{
    kColorBlack = false,
    kColorWhite = true
};
template <class Driver>
class DisplayGfx: public Driver
{
protected:
    enum: uint8_t {
        kFontHspaceMask = 0x3,
        kFontHspaceOff = 0,
        kFontHspace1 = 1,
        kFontHspace2 = 2,
        kFontHspace3 = 3,
        kStateInverted = 4
    };
    uint16_t mCurrentX = 0;
    uint16_t mCurrentY = 0;
    uint8_t mState = kFontHspace2;
    Color mColor = kColorWhite;
    const Font* mFont = nullptr;
public:
    void setDrawColor(Color aColor) { mColor = aColor; }
    Color drawColor() const { return mColor; }
    void setFont(Font& font) { mFont = &font; }
    const Font& font() const { return *mFont; }
    bool hasFont() const { return mFont != nullptr; }
    uint8_t charSpacing() const { return mState & kFontHspaceMask; }
    uint8_t charWidthWithSpacing() const { return mFont->width + charSpacing(); }
    bool isInverted() const { return mState & kStateInverted; }
    using Driver::Driver;
bool init()
{
    if (!Driver::init())
    {
        return false;
    }
    /* Clear screen */
    fill(isInverted() ? kColorWhite : kColorBlack);
    /* Update screen */
    Driver::updateScreen();
    /* Initialized OK */
    return true;
}
void toggleInvert(void)
{
    if (mColor)
        mColor = kColorBlack;
    else
        mColor = kColorWhite;

    /* Do memory toggle */
    size_t* bufEnd = static_cast<size_t*>(Driver::mBuf + Driver::kBufSize);
    for (size_t* ptr = Driver::mBuf; ptr < bufEnd; ptr++)
    {
        *ptr = ~*ptr;
    }
}
void fill(Color color)
{
    /* Set memory */
    memset(Driver::rawBuf(), (color == kColorBlack) ? 0x00 : 0xFF, Driver::kBufSize);
}
void clear()
{
    fill(kColorBlack);
}
template <bool Check = true>
void setPixel(uint16_t x, uint16_t y)
{
    if (Check)
    {
        if (x >= Driver::width() || y >= Driver::height())
        {
            /* Error */
            //tprintf("out of range\n");
            return;
        }
    }

    /* Set color */
    if (mColor) {
        Driver::mBuf[x + (y >> 3) * Driver::width()] |= 1 << (y % 8);
    } else {
        Driver::mBuf[x + (y >> 3) * Driver::width()] &= ~(1 << (y % 8));
    }
}

void gotoXY(uint16_t x, uint16_t y)
{
    /* Set write pointers */
    mCurrentX = x;
    mCurrentY = y;
}

void gotoX(uint16_t x)
{
    mCurrentX = x;
}

uint8_t putc(char ch, int16_t xLim=10000)
{
    xassert(mFont);
    uint8_t fontW = mFont->width;
    uint8_t symPages = (mFont->height + 7) / 8;
    uint8_t symLastPage = symPages - 1;
    const uint8_t* sym = mFont->data + (ch - 32) * fontW * symPages;
    uint8_t* bufDest = Driver::mBuf + (mCurrentY >> 3) * Driver::width() + mCurrentX;

    if (mCurrentY >= Driver::height())
    {
        return 0;
    }
    xLim = std::min(Driver::width(), xLim);
    uint8_t writeWidth;
    if (mCurrentX + fontW <= xLim)
    {
        writeWidth = fontW;
    }
    else
    {
        if (mCurrentX >= xLim)
        {
            return 0;
        }
        writeWidth = xLim - mCurrentX;
    }
    uint8_t vOfs = (mCurrentY % 8); //offset within the vertical byte
    if (vOfs == 0)
    {
        for (int page = 0; page <= symLastPage; page++)
        {
            uint8_t* dest = bufDest+page*Driver::width();
            const uint8_t* src = sym+page*fontW;
            const uint8_t* srcEnd = src+writeWidth;
            uint8_t wmask = (page < symLastPage)
                ? 0xff
                : 0xff >> (8 - mFont->height);

            for (; src < srcEnd; src++, dest++)
            {
                *dest = (*dest & ~wmask) | ((mColor ? (*src) : (~*src)) & wmask);
            }
        }
    }
    else
    {
        uint8_t displayFirstPageHeight = 8 - vOfs; // write height of first display page
        uint8_t firstLineFullMask = 0xff << vOfs; //masks the bits above the draw line
        bool onlyFirstLine = mFont->height <= displayFirstPageHeight;
        uint8_t wmask = onlyFirstLine //write mask
             ? ((1 << mFont->height) - 1) << vOfs //the font doesn't span to line bottom, if font height is small
             : firstLineFullMask; //font spans to bottom of line
        uint8_t* wptr = bufDest; //write pointer
        uint8_t* wend = wptr+writeWidth;
        const uint8_t* rptr = sym; //read pointer
        if (mColor)
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
        {
            return writeWidth; //we don't span on the next byte (page)
        }
        auto bufEnd = Driver::mBuf + Driver::kBufSize;
        uint8_t wpage = 1;
        wptr = bufDest + wpage*Driver::width();
        wend = wptr + writeWidth;
        if (wend > bufEnd)
        {
            return writeWidth;
        }

        uint8_t secondPageMask = 0xff << displayFirstPageHeight; // low vOfs bits set
        uint8_t symLastPageHeight = mFont->height % 8;
        if (symLastPageHeight == 0)
        {
            symLastPageHeight = mFont->height;
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
                    uint8_t srcByte = mColor
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
                    uint8_t srcByte = mColor
                        ? ((*(rptr++) << vOfs))
                        : ((~*(rptr++) << vOfs));
                    b = (b & ~wmask) | srcByte;
                    wpage++;
                    wptr = bufDest +wpage * Driver::width();
                    wend = wptr + writeWidth;
                    if (wend > bufEnd)
                    {
                        return writeWidth;
                    }
                }
                *(wptr++) = b;
            }
        }
    }
    return writeWidth;
}

bool puts(const char* str, int16_t xLim=10000)
{
    while(*str)
    {
        /* Write character by character */
        auto writeWidth = putc(*str, xLim);
        if (!writeWidth)
        {
            return false;
        }
        mCurrentX += writeWidth + (mState & kFontHspaceMask);

        str++;
    }
    return true;
}
int16_t textWidth(const char* str)
{
    xassert(mFont);
    int strWidth;
    if (mFont->isMono())
    {
        return (mFont->width + (mState & kFontHspaceMask)) * strlen(str) - (mState & kFontHspaceMask);
    }

    strWidth = 0;
    while(*str)
    {
        strWidth += mFont->widths[*str - 32];
    }
    return strWidth;
}

bool putsCentered(int16_t y, const char* str)
{
    auto strWidth = textWidth(str);
    if (strWidth > Driver::width())
    {
        return false;
    }
    gotoXY((Driver::width() - strWidth) / 2, y);
    puts(str);
    return true;
}

bool putsRAligned(int16_t y, const char* str, int16_t right=Driver::width()-1)
{
    auto left = right - textWidth(str) + 1;
    if (left < 0)
    {
        return false;
    }
    gotoXY(left, y);
    puts(str);
    return true;
}

void hLine(uint16_t x1, uint16_t x2, uint16_t y)
{
    gfx_checkbounds_x(x1);
    gfx_checkbounds_x(x2);
    gfx_checkbounds_y(y);

    if (x2 < x1)
    {
        std::swap(x1, x2);
    }
    auto pageStart = Driver::mBuf + (y >> 3) * Driver::width();
    auto start = pageStart + x1;
    auto end = pageStart + x2;
    uint8_t mask = 1 << (y % 8);
    for (auto ptr = start; ptr <= end; ptr++)
    {
        *ptr |= mask;
    }
}
void vLine(uint16_t y1, uint16_t y2, uint16_t x)
{
    gfx_checkbounds_y(y1);
    gfx_checkbounds_y(y2);
    gfx_checkbounds_x(x);
    if (y1 > y2)
    {
        std::swap(y1, y2);
    }
    auto page1 = y1 >> 3;
    auto page2 = y2 >> 3;
    auto pByte1 = Driver::mBuf + Driver::width() * page1 + x;
    auto pByte2 = Driver::mBuf + Driver::width() * page2 + x;

    uint8_t ofs1 = y1 % 8;
    uint8_t mask1 = 0xff << ofs1;

    uint8_t ofs2 = y2 % 8;
    uint8_t mask2 = 0xff >> (7 - ofs2);
    if (page2 == page1) // line starts and ends on same page
    {
        *pByte1 |= (mask1 & mask2);
        return;
    }
    *pByte1 |= mask1;
    *pByte2 |= mask2;
    for (auto pByte = pByte1 + Driver::width(); pByte < pByte2; pByte += Driver::width())
    {
        *pByte = 0xff;
    }
}

void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    /* Check for overflow */
    if (x0 >= Driver::width()) {
        x0 = Driver::width() - 1;
    }
    if (x1 >= Driver::width()) {
        x1 = Driver::width() - 1;
    }
    if (y0 >= Driver::height()) {
        y0 = Driver::height() - 1;
    }
    if (y1 >= Driver::height()) {
        y1 = Driver::height() - 1;
    }

    int16_t dx = (x0 < x1) ? (x1 - x0) : (x0 - x1);
    int16_t dy = (y0 < y1) ? (y1 - y0) : (y0 - y1);

    if (dx == 0)
    {
        vLine(y0, y1, x0);
        return;
    }

    if (dy == 0)
    {
        hLine(x0, x1, y0);
        return;
    }

    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = ((dx > dy) ? dx : -dy) / 2;
    for(;;)
    {
        setPixel(x0, y0);
        if (x0 == x1 && y0 == y1)
        {
            return;
        }
        if (err > -dx) {
            err -= dy;
            x0 += sx;
        }
        if (err < dy) {
            err += dx;
            y0 += sy;
        }
    }
}

void drawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    /* Check input parameters */
    if (
        x >= Driver::width() ||
        y >= Driver::height()
    ) {
        /* Return error */
        return;
    }

    /* Check width and height */
    if ((x + w) >= Driver::width()) {
        w = Driver::width() - x;
    }
    if ((y + h) >= Driver::height()) {
        h = Driver::height() - y;
    }

    drawLine(x, y, x + w, y);         // top
    drawLine(x, y + h, x + w, y + h); // bottom
    drawLine(x, y, x, y + h);         // left
    drawLine(x + w, y, x + w, y + h); // right
}

void drawFilledRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint8_t i;

    /* Check input parameters */
    if (
        x >= Driver::width() ||
        y >= Driver::height()
    ) {
        /* Return error */
        return;
    }

    /* Check width and height */
    if ((x + w) >= Driver::width()) {
        w = Driver::width() - x;
    }
    if ((y + h) >= Driver::height()) {
        h = Driver::height() - y;
    }

    /* Draw lines */
    for (i = 0; i <= h; i++) {
        /* Draw lines */
        drawLine(x, y + i, x + w, y + i);
    }
}

void drawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint8_t color)
{
    /* Draw lines */
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x3, y3, color);
    drawLine(x3, y3, x1, y1, color);
}


void drawFilledTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3)
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
        drawLine(x, y, x3, y3);

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

void drawCircle(int16_t x0, int16_t y0, int16_t r)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    setPixel(x0, y0 + r);
    setPixel(x0, y0 - r);
    setPixel(x0 + r, y0);
    setPixel(x0 - r, y0);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        setPixel(x0 + x, y0 + y);
        setPixel(x0 - x, y0 + y);
        setPixel(x0 + x, y0 - y);
        setPixel(x0 - x, y0 - y);

        setPixel(x0 + y, y0 + x);
        setPixel(x0 - y, y0 + x);
        setPixel(x0 + y, y0 - x);
        setPixel(x0 - y, y0 - x);
    }
}

void drawFilledCircle(int16_t x0, int16_t y0, int16_t r)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    setPixel(x0, y0 + r);
    setPixel(x0, y0 - r);
    setPixel(x0 + r, y0);
    setPixel(x0 - r, y0);
    drawLine(x0 - r, y0, x0 + r, y0);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        drawLine(x0 - x, y0 + y, x0 + x, y0 + y);
        drawLine(x0 + x, y0 - y, x0 - x, y0 - y);

        drawLine(x0 + y, y0 + x, x0 - y, y0 + x);
        drawLine(x0 + y, y0 - x, x0 - y, y0 - x);
    }
}

void invertRect(int16_t x, int16_t y, int16_t width, int16_t height)
{
    if (x >= Driver::width() || y >= Driver::height())
    {
        return;
    }
    if (y < 0)
    {
        if (-y < height)
        {
            height += y;
            y = 0;
        }
        else
        {
            return;
        }
    }
    if (x < 0)
    {
        if (width > -x)
        {
            width += x;
            x = 0;
        }
        else
        {
            return;
        }
    }
    int16_t xEnd = x + width;
    if (xEnd > Driver::width())
    {
        xEnd = Driver::width();
        width = Driver::width() - x;
    }
    uint8_t yBottom = y + height - 1;
    if (yBottom >= Driver::height())
    {
        yBottom = Driver::height() - 1;
        // no need to update height - it's not used further below
    }

    uint8_t page = y / 8;
    uint8_t endPage = yBottom / 8;

    uint8_t yTopOffset = y % 8;
    uint8_t yBottomOffset = 7 - (yBottom % 8);
    auto wend = Driver::mBuf + page * Driver::width() + width;
    uint8_t xorMask = 0xff << yTopOffset;
    do
    {
        if (page == endPage)
        {
            xorMask &= (0xff >> yBottomOffset);
        }
        for(auto wptr = wend - width; wptr < wend; wptr++)
        {
            *wptr ^= xorMask;
        }
        xorMask = 0xff;
        wend += Driver::width();
    }
    while(++page <= endPage);
}
};
#endif
