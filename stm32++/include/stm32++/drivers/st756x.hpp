#ifndef STM32PP_ST756x_H
#define STM32PP_ST756x_H

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <stm32++/timeutl.hpp>
#include <stm32++/spi.hpp>
#include <stm32++/gfx.hpp>
#include <memory.h>

#define ST756x_LCD_CMD_DISPLAY_OFF           0xAE
#define ST756x_LCD_CMD_DISPLAY_ON            0xAF

#define ST756x_LCD_CMD_SET_DISP_START_LINE   0x40
#define ST756x_LCD_CMD_SET_PAGE              0xB0

#define ST756x_LCD_CMD_SET_COLUMN_UPPER      0x10
#define ST756x_LCD_CMD_SET_COLUMN_LOWER      0x00

#define ST756x_LCD_CMD_SET_SEG_NORMAL        0xA0
#define ST756x_LCD_CMD_SET_SEG_REVERSE       0xA1

#define ST756x_LCD_CMD_SET_COM_NORMAL        0xC0
#define ST756x_LCD_CMD_SET_COM_REVERSE       0xC8

#define ST756x_LCD_CMD_SET_DISP_NORMAL       0xA6
#define ST756x_LCD_CMD_SET_DISP_INVERSE      0xA7

#define ST756x_LCD_CMD_SET_ALLPTS_NORMAL     0xA4
#define ST756x_LCD_CMD_SET_ALLPTS_ON         0xA5

#define ST756x_LCD_CMD_SET_BIAS_9            0xA2
#define ST756x_LCD_CMD_SET_BIAS_7            0xA3

#define ST756x_LCD_CMD_POWER_ON              0x28 | 0b0111 // = 0x2f
#define ST756x_LCD_CMD_POWER_OFF             0x28

#define ST756x_LCD_CMD_SET_VREG_RATIO        0x20
#define ST756x_LCD_CMD_SET_EV                0x81
#define ST756x_LCD_CMD_RESET                 0xE2

#define ST756x_LCD_CMD_RMW                   0xE0
#define ST756x_LCD_CMD_RMW_CLEAR             0xEE
/*
#define ST7565_LCD_CMD_SET_STATIC_OFF        0xAC
#define ST7565_LCD_CMD_SET_STATIC_ON         0xAD
#define ST7565_LCD_CMD_SET_STATIC_REG        0x0
#define ST7565_LCD_CMD_SET_BOOSTER_FIRST     0xF8
#define ST7565_LCD_CMD_SET_BOOSTER_234       0
#define ST7565_LCD_CMD_SET_BOOSTER_5         1
#define ST7565_LCD_CMD_SET_BOOSTER_6         3
#define ST7565_LCD_CMD_NOP                   0xE3
#define ST7565_LCD_CMD_TEST                  0xF0
*/

template <class IO, class RstPin, class DtCmdPin, int16_t Width=128, int16_t Height=64>
class ST7567_Driver
{
protected:
    IO& mIo;
    enum: uint16_t { kBufSize = Width * Height / 8 };
    uint8_t mBuf[kBufSize];
public:
    ST7567_Driver(IO& io): mIo(io) {}
    uint8_t* rawBuf() { return mBuf; }
    static int16_t width() { return Width; }
    static int16_t height() { return Height; }
    void cmd(uint8_t byte) { mIo.send(byte); }
    void setContrast(uint8_t val)
    {
        cmd(ST756x_LCD_CMD_SET_EV); // set EV command
        cmd(val); // EV value
    }
    void powerOn() { cmd(ST756x_LCD_CMD_POWER_ON); }
    void powerOff() { cmd(ST756x_LCD_CMD_POWER_OFF); }
    void displayOn() { cmd(ST756x_LCD_CMD_DISPLAY_ON); }
    void displayOff() { cmd(ST756x_LCD_CMD_DISPLAY_OFF); }
    void updateScreen(void)
    {
        int ofs = 0;
        for (int page = 0; page < Height / 8; page++)
        {
            cmd(ST756x_LCD_CMD_SET_PAGE | page);  // set page address
            cmd(ST756x_LCD_CMD_SET_COLUMN_UPPER); // set column hi  nibble 0
            cmd(0x00);                            // set column low nibble 0
            nsDelay(500);
            DtCmdPin::set();

            int end = ofs + Width;
            while(ofs < end)
            {
                mIo.send(mBuf[ofs++]);
            }
            nsDelay(500);
            DtCmdPin::clear();
        }
    }
    bool init()
    {
        static_assert(Height % 8 == 0);
        memset(mBuf, 0x00, 1024);  // clear display buffer
        RstPin::enableClockAndSetMode(GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL);
        if (DtCmdPin::kClockId != RstPin::kClockId)
        {
            rcc_periph_clock_enable(DtCmdPin::kClockId);
        }
        DtCmdPin::setMode(GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL);

        DtCmdPin::clear();
        RstPin::clear();
        usDelay(10);
        RstPin::set();
        usDelay(10);

        /* Send Commands */
        cmd(ST756x_LCD_CMD_SET_BIAS_7); /* Setup 1/7th Bias Level */
        cmd(ST756x_LCD_CMD_SET_SEG_NORMAL); /* Horizontal (SEG) direction */
        cmd(ST756x_LCD_CMD_SET_COM_REVERSE); /* Vertical (COM) direction */
        cmd(ST756x_LCD_CMD_SET_VREG_RATIO | 0x2); /* Set LCD operating voltage */
        setContrast(0x18);
        powerOn();
        msDelay(10);
        displayOn();
        updateScreen();
        return true;
    }
};
template <class IO, class RstPin, class DtCmdPin, uint16_t W=128, uint16_t H=64>
using ST7567 = DisplayGfx<ST7567_Driver<IO, RstPin, DtCmdPin, W, H>>;

#endif
