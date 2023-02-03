#include "Typedefs.h"

#pragma once

extern int audioCycles;

// Channel variables
struct AudioRegisters
{
    struct _ctrl
    {
        byte vinVol;
        byte lrEnable;
        byte power;
    } ctrl;
};

// Audio classes
class AChannel
{
    public:
        virtual void Trigger() = 0;
        virtual void Cycle() = 0;
    protected:
        void EnvelopeSweep();
        void LengthCounter();
        // Helpers
        bool channelEnabled;
        bool runEnvelopeSweep;
        float channelData;
        int freqTimer;
        bool runLengthCounterSweep;
        int lengthCounter;
        // Registers
        byte volume;
        byte freq;
        byte trigger;
        byte periodTimer;
        byte currentVolume;
};

class PulseChannel2 : AChannel
{
    public:
        PulseChannel2();
        ~PulseChannel2() {};
        void Trigger();
        void Cycle();
    protected:
        // Helpers
        byte dutyPos;
        // Registers
        byte sweep;
        byte duty;
        friend class PulseChannel1;
        friend class AudioComponent;
};


class PulseChannel1 : PulseChannel2
{
    public:
        PulseChannel1();
        ~PulseChannel1() {};
        void Trigger();
        void Cycle();
    protected:
        int CalculateFrequency();
        // Helpers
        bool runFrequencySweep;
        bool sweepEnabled;
        int shadowFrequency;
        int sweepTimer;
        friend class AudioComponent;
};

class WaveChannel : AChannel
{
    public:
        WaveChannel();
        ~WaveChannel() {};
        void Trigger();
        void Cycle();
    protected:
        // Helpers
        byte wavSample;
        byte wavPos;
        // Registers
        byte dac;
        byte length;
        friend class AudioComponent;
};

class NoiseChannel : AChannel
{
    public:
        NoiseChannel();
        ~NoiseChannel() {};
        void Trigger();
        void Cycle();
    protected:
        // Helpers
        word LFSR;
        // Registers
        byte pad; // required for byte padding - not actually used
        byte length;
        byte clock;
        friend class AudioComponent;
};

class AudioComponent : GComponent
{
    public:
        AudioComponent();
        ~AudioComponent() {};
        void PokeByte(word, byte);
        byte PeekByte(word);
        void PokeWord(word, word);
        word PeekWord(word);
        void Cycle();
        void Reset();
    protected:
        bool MemoryMapped(word);
        PulseChannel1 pulseChannel1;
        PulseChannel2 pulseChannel2;
        WaveChannel waveChannel;
        NoiseChannel noiseChannel;
        friend class MmuComponent;
};