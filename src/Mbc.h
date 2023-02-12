#include "Typedefs.h"
#include "Rtc.h"

enum RamMode {
    RamDisabled = 0,
    RamEnabled,
    RamRtc
};

class MbcBase
{
    public:
        virtual void PokeByte(word, byte) = 0;
        byte GetRomBank();
        byte GetRamBank();
        RamMode GetRamMode();
        RtcRegister GetRtcRegister();
    protected:
        RtcRegister rtc;
        byte rom_bnk_no = 1; // ROM banking starts at 1 (except for MBC5 which can set it to 0)
        byte ram_bnk_no = 0;
        RamMode ramMode;
};

class Mbc0 : public MbcBase
{
    public:
        void PokeByte(word, byte) override;
};

class Mbc1 : public MbcBase
{
    public:
        void PokeByte(word, byte) override;
};

class Mbc3 : public MbcBase
{
    public:
        void PokeByte(word, byte) override;
};

class Mbc5 : public MbcBase
{
    public:
        void PokeByte(word, byte) override;
};