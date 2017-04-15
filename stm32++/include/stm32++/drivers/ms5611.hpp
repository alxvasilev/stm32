#ifndef MS5611_HPP_INCLUDED
#define MS5611_HPP_INCLUDED

#include <stdint.h>
#include <string.h> //for memcpy
#include <stm32++/timeutl.h>

template <class IO>
class MS5611
{
public:
    enum: uint8_t
    {
        kCmdReset = 0x1e,
        kCmdPromReadBase = 0xA0,
        kCmdConvertD1Base = 0x40,
        kCmdConvertD2Base = 0x50,
        kCmdAdcRead = 0x00
    };
    enum: uint8_t
    {
        kCalPressSens = 0,
        kCalPressOffs = 1,
        kCalPressSensTCoef = 2,
        kCalPressOffsTCoef = 3,
        kCalTRef = 4,
        kCalTCoeff = 5
    };
    MS5611(IO& io, uint8_t addr=0x77): mIo(io), mAddr(addr){}
    bool init()
    {
        (volatile bool)mIo.isDeviceConnected(mAddr);
        usDelay(10);
        if (!reset())
        {
            dbg("ms5611 not found on I2C bus");
            return false;
        }
        if (!loadCalibrationData())
            return false;
        return true;
    }
    bool reset()
    {
        if (!mIo.startSend(mAddr, true))
            return false;
        if (!mIo.sendByteTimeout(kCmdReset))
            return false;
        mIo.stop();
        msDelay(3);
        return true;
    }
    int32_t temp() const { return mTemp; }
    int32_t pressure() const { return mPressure; }
protected:
    IO& mIo;
    uint8_t mAddr;
    uint16_t mCalData[6];
    int32_t mTemp = 0;
    int32_t mPressure = 0;
    bool sendCmd(uint8_t cmd)
    {
        if (!mIo.startSend(mAddr, true))
            return false;
        bool ret = mIo.sendByteTimeout(cmd);
        mIo.stop();
        if (ret)
            return true;
        return false;
    }
    bool loadCalibrationData()
    {
        uint16_t reserved;
        uint16_t crc;
        uint8_t* ptr = (uint8_t*)mCalData;
        for (uint8_t idx = 0; idx < 8; idx++)
        {
            if (!sendCmd(kCmdPromReadBase + (idx * 2)))
                return false;
            if (!mIo.startRecv(mAddr, true))
                return false;
            uint8_t* dest;
            if (idx == 0)
            {
                dest = (uint8_t*)(&reserved);
            }
            else if (idx == 7)
            {
                dest = (uint8_t*)(&crc);
            }
            else
            {
                dest = ptr;
                ptr += 2;
            }
            //sensor sends data in big endian format
            uint16_t ret = mIo.recvByteTimeout();
            if (ret == 0xffff)
                return false;
            dest[1] = ret;
            ret = mIo.recvByteTimeout();
            if (ret == 0xffff)
                return false;
            dest[0] = ret;
            if (!mIo.stopTimeout())
                return false;
        }
        assert(ptr == (uint8_t*)mCalData+12);
        uint16_t crcBuf[8];
        memcpy(crcBuf+1, mCalData, sizeof(mCalData));
        crcBuf[0] = reserved;
        crcBuf[7] = crc;
        return crc4(crcBuf);
    }
    uint16_t usNeededForOsr(uint8_t osr)
    {
        switch(osr)
        {
            case 0: return 600;
            case 2: return 1200;
            case 4: return 2500;
            case 6: return 4600;
            case 8:
            default:
                    return 9100;
        }
    }
    uint32_t getRawMeasurement(uint8_t baseCmd, uint8_t osr)
    {
        if (!sendCmd(baseCmd+osr))
            return 0;
        mIo.stop();
        usDelay(usNeededForOsr(osr));
        if (!sendCmd(kCmdAdcRead))
            return 0;

        if (!mIo.startRecv(mAddr, true))
            return 0;

        uint32_t result = 0;
        result |= (((uint32_t)mIo.recvByte()) << 16);
        result |= (((uint32_t)mIo.recvByte()) << 8);
        result |= (((uint32_t)mIo.recvByte()));
        mIo.stop();
        return result;
    }
public:
    uint32_t getRawTemp(uint8_t osr)
    {
        return getRawMeasurement(kCmdConvertD2Base, osr);
    }
    uint32_t getRawPressure(uint8_t osr)
    {
        return getRawMeasurement(kCmdConvertD1Base, osr);
    }
    void sample(uint8_t osr=8)
    {
        uint32_t rawTemp = getRawTemp(osr);
        uint32_t rawPress = getRawPressure(osr);
        int64_t dt = rawTemp - ((int64_t)mCalData[kCalTRef] * (1<<8));
        mTemp = 2000 + (dt * mCalData[kCalTCoeff])/(1<<23);
        int64_t off2, sens2;
        bool corr2;
        if (mTemp < 2000)
        {
            corr2 = true;
            uint32_t t2 = (dt*dt) / (1 << 31);
            uint32_t m2000 = (mTemp-2000);
            off2 = 5*(m2000*m2000) / 2;
            sens2 = off2 / 2;
            if (mTemp < -15)
            {
                uint32_t p1500 = mTemp+1500;
                p1500*=p1500;
                off2 = off2 + 7*p1500;
                sens2 = sens2 + 11*p1500/2;
            }
            mTemp = mTemp - t2;
        }
        else
        {
            corr2 = false;
        }
        int64_t off = ((int64_t)mCalData[kCalPressOffs]) * (1<<16) + (mCalData[kCalPressOffsTCoef]*dt) / (1<<7);
        int64_t sens = ((int64_t)mCalData[kCalPressSens]) * (1<<15) + (mCalData[kCalPressSensTCoef]*dt) / (1<<8);
        if (corr2)
        {
            off -= off2;
            sens -= sens2;
        }
        mPressure = (((int64_t)rawPress * sens) / (1<<21) - off) / (1<<15);
    }
    static bool crc4(uint16_t n_prom[])
    {
        uint16_t n_rem = 0x00;
        uint16_t crc_read = n_prom[7]; //save read CRC
        n_prom[7] &= 0xFF00; //CRC byte is replaced by 0
        for (int cnt = 0; cnt < 16; cnt++) // operation is performed on bytes
        { // choose LSB or MSB
            if (cnt & 1)
                n_rem ^= n_prom[cnt>>1] & 0x00FF;
            else
                n_rem ^= n_prom[cnt>>1] >> 8;

            for (uint8_t n_bit = 8; n_bit > 0; n_bit--)
            {
                if (n_rem & (0x8000))
                    n_rem = (n_rem << 1) ^ 0x3000;
                else
                    n_rem = (n_rem << 1);
            }
        }
        n_rem >>= 12; // final 4-bit reminder is CRC code
        n_prom[7]=crc_read; // restore the crc_read to its original place
        return ((0x000F & crc_read) == n_rem);
    }
};
/*
Usage:
#include <stm32++/i2c.hpp>

int main()
{
    rcc_clock_setup_in_hse_8mhz_out_72mhz();
    dwt_enable_cycle_counter();
    nsi2c::I2c<I2C2> i2c;
    i2c.init();
    MS5611<nsi2c::I2c<I2C2>> sens(i2c);
    sens.init();
    usDelay(1000);
    sens.sample();
    float pres = sens.pressure();
    for(;;)
    {
        sens.sample();
        pres = (pres*9 + sens.pressure()) / 10;
        tprintf("temp = %, press = %\n",
                fmtFp<2>(float((sens.temp()))/100),
                fmtFp<2>(pres/100));
    }
}
*/
