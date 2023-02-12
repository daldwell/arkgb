#include <ctime>
#include "Typedefs.h"

class RtcRegister
{
    public:
        void LatchClock();
        void SetMap(word);
        byte * GetMap();
    private:
        word map;
        byte sec;
        byte min;
        byte hour;
        byte dl;
        byte dh;
};