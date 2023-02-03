#include <SDL.h>
#include <cstdio>
#include <assert.h>
#include "Mmu.h"
#include "Cpu.h"
#include "Typedefs.h"
#include "Audio.h"
#include "Timer.h"
#include "GUnit.h"

int audioCycles;
struct AudioRegisters audioRegs;
SDL_AudioDeviceID audio_device_id;

// Audio buffer data
float mixData[2048];
int bufferCursor;

byte dutyWaveTable[4]
{
    0x1,
    0x3,
    0xF,
    0xFC
};

byte fsStep;
bool fsEdge = true;

float currentVol;

byte wav[0x10]; 

AudioComponent::AudioComponent()
{
    Reset();
}

byte AudioComponent::PeekByte(word addr)
{
    byte retValue = 0xFF;
        // Channel 1
    if (addr == 0xFF10) {
        retValue = pulseChannel1.sweep | 0x80;
    } else if (addr == 0xFF11) {
        retValue = pulseChannel1.duty | 0x3F;
    } else if (addr == 0xFF12) {
        retValue = pulseChannel1.volume;
    } else if (addr == 0xFF13) {
        retValue = pulseChannel1.freq | 0xFF;
    } else if (addr == 0xFF14) {
        retValue = pulseChannel1.trigger | 0xBF;
    }

    // Channel 2
    if (addr == 0xFF15) {
        retValue = pulseChannel2.sweep | 0xFF;
    } else if (addr == 0xFF16) {
        retValue = pulseChannel2.duty | 0x3F;
    } else if (addr == 0xFF17) {
        retValue = pulseChannel2.volume;
    } else if (addr == 0xFF18) {
        retValue = pulseChannel2.freq | 0xFF;
    } else if (addr == 0xFF19) {
        retValue = pulseChannel2.trigger | 0xBF;
    }

    // Channel 3
    if (addr == 0xFF1A) {
        retValue = waveChannel.dac | 0x7F;
    } else if (addr == 0xFF1B) {
        retValue = waveChannel.length | 0xFF;
    } else if (addr == 0xFF1C) {
        retValue = waveChannel.volume | 0x9F;
    } else if (addr == 0xFF1D) {
        retValue = waveChannel.freq | 0xFF;
    } else if (addr == 0xFF1E) {
        retValue = waveChannel.trigger | 0xBF;
    }

    // Unmapped audio regs
    if (addr >= 0xFF27 && addr <= 0xFF2F) {
        retValue = 0xFF;
    }

    // Channel 4
    if (addr == 0xFF1F) {
        retValue = noiseChannel.pad | 0xFF;
    } else if (addr == 0xFF20) {
        retValue = noiseChannel.length | 0xFF;
    } else if (addr == 0xFF21) {
        retValue = noiseChannel.volume;
    } else if (addr == 0xFF22) {
        retValue = noiseChannel.clock;
    } else if (addr == 0xFF23) {
        retValue = noiseChannel.trigger | 0xBF;
    }

    // Control
    if (addr == 0xFF24) {
        retValue = audioRegs.ctrl.vinVol;
    } else if (addr == 0xFF25) {
        retValue = audioRegs.ctrl.lrEnable;
    } else if (addr == 0xFF26) {
        retValue = audioRegs.ctrl.power | 0x70;
        // Set channel status flags
        retValue += pulseChannel1.channelEnabled;
        retValue += pulseChannel2.channelEnabled << 1;
        retValue += waveChannel.channelEnabled << 2;
        retValue += noiseChannel.channelEnabled << 3;
    }    

    // Wave table
    if (addr >= 0xFF30 && addr <= 0xFF3F)
        return wav[addr&0xF];

    return retValue;
}

void AudioComponent::PokeByte(word addr, byte value)
{

    // Ignore register writes if APU is off (except for control register)
    if (!(audioRegs.ctrl.power&0x80) && addr < 0xFF26) {
        return;
    }

    // Channel 1
    if (addr == 0xFF10) {
        pulseChannel1.sweep = value;
    } else if (addr == 0xFF11) {
        pulseChannel1.duty = value;
        pulseChannel1.lengthCounter = 0x40-(value&0x3F);
    } else if (addr == 0xFF12) {
        pulseChannel1.volume = value;
        if (!(pulseChannel1.volume&0xF8)) pulseChannel1.channelEnabled = false;
    } else if (addr == 0xFF13) {
        pulseChannel1.freq = value;
    } else if (addr == 0xFF14) {
        pulseChannel1.trigger = value;
        if (value&0x80) pulseChannel1.Trigger();
    }

    // Channel 2
    if (addr == 0xFF16) {
        pulseChannel2.duty = value;
        pulseChannel2.lengthCounter = 0x40-(value&0x3F);
    } else if (addr == 0xFF17) {
        pulseChannel2.volume = value;
        if (!(pulseChannel2.volume&0xF8)) pulseChannel2.channelEnabled = false;
    } else if (addr == 0xFF18) {
        pulseChannel2.freq = value;
    } else if (addr == 0xFF19) {
        pulseChannel2.trigger = value;
        if (value&0x80) pulseChannel2.Trigger();
    }

    // Channel 3
    if (addr == 0xFF1A) {
        waveChannel.dac = value;
        if (!(waveChannel.dac&0x80)) waveChannel.channelEnabled = false;
    } else if (addr == 0xFF1B) {
        waveChannel.length = value;
        waveChannel.lengthCounter = 0x100-(value);
    } else if (addr == 0xFF1C) {
        waveChannel.volume = value;
    } else if (addr == 0xFF1D) {
        waveChannel.freq = value;
    } else if (addr == 0xFF1E) {
        waveChannel.trigger = value;
        if (value&0x80) waveChannel.Trigger();
    }

    // Channel 4
    if (addr == 0xFF20) {
        noiseChannel.length = value;
        noiseChannel.lengthCounter = 0x40-(value&0x3F);
    } else if (addr == 0xFF21) {
        noiseChannel.volume = value;
        if (!(noiseChannel.volume&0xF8)) noiseChannel.channelEnabled = false;
    } else if (addr == 0xFF22) {
        noiseChannel.clock = value;
    } else if (addr == 0xFF23) {
        noiseChannel.trigger = value;
        if (value&0x80) noiseChannel.Trigger();
    }

    // Control
    if (addr == 0xFF24) {
        audioRegs.ctrl.vinVol = value;
    } else if (addr == 0xFF25) {
        audioRegs.ctrl.lrEnable = value;
    } else if (addr == 0xFF26) {
        // Clear audio registers if powering down APU
        if (!(value&0x80)) {
            for (int i = 0; i < 0x16; i++) {
                PokeByte(0xFF10 + i, 0);                
            }
            fsEdge = false;
        }
        // Set power to off
        audioRegs.ctrl.power &= 0x0F;
        audioRegs.ctrl.power += (value&0xF0);
    }

    // Wave table
    if (addr >= 0xFF30 && addr <= 0xFF3F)
        wav[addr&0xF] = value;

    return;    
}

bool AudioComponent::MemoryMapped(word addr)
{
    // Audio registers
    if (addr >= 0xFF10 && addr <= 0xFF2F) {
        return true;
    }

    // Wave table
    if (addr >= 0xFF30 && addr <= 0xFF3F) {
        return true;
    }

    return false;
}

// Length Counter Sweep
// Used by all channels
void AChannel::LengthCounter()
{
    // Disable if length counter 0
    if ((trigger&0x40) && runLengthCounterSweep) {

        if (lengthCounter > 0) {
            lengthCounter -= 1;
        }

        if (lengthCounter == 0) {
            channelEnabled = false;
        }
    }
    runLengthCounterSweep = false;
}

// Envelope sweep
// Used by channel 1,2 and 4
void AChannel::EnvelopeSweep()
{
    byte adjustment = 0;

    // Adjust envelope from frame sequencer
    if (runEnvelopeSweep && (volume & 0x7) != 0) {

        if (periodTimer > 0) {
            periodTimer -= 1;
        }

        if (periodTimer == 0) {
            if ((volume & 0x7) > 0) {
                periodTimer = (volume & 0x7);
            } else {
                periodTimer = 8;
            }

            if ( (currentVolume < 0xF && (volume&0x8)) || (currentVolume > 0x0 && !(volume&0x8)) ) {
                if (volume&0x8) {
                    adjustment = +1;
                } else {
                    adjustment = -1;
                }
                currentVolume += adjustment;
            }
        }
    }
    runEnvelopeSweep = false;
}

PulseChannel1::PulseChannel1()
{
    Trigger();
}


// Helper function to calculate new frequency for channel 1
int PulseChannel1::CalculateFrequency()
{
    int newFrequency = shadowFrequency >> (sweep & 0x7);

    // Is decrementing is the sweep direction.
    if ((sweep & 0x8)) {
        newFrequency = shadowFrequency - newFrequency;
    } else {
        newFrequency = shadowFrequency + newFrequency;
    }

    // Overflow check
    if (newFrequency > 2047) {
        channelEnabled = false;
    }

    return newFrequency;
}

void PulseChannel1::Trigger()
{
    byte sweepPeriod;
    channelEnabled = true;

    // Frequency sweep setup
    shadowFrequency = ((trigger & 0x7) << 8) + freq;

    // Set freq sweep period
    sweepPeriod = (sweep & 0x70) >> 4;
    if (sweepPeriod > 0) {
        sweepTimer = sweepPeriod;
    } else {
        sweepTimer = 0x8;
    }

    // Freq sweep enabled if sweep period OR shift greater than 0
    sweepEnabled = (((sweep & 0x70) >> 4) > 0 || (sweep & 0x7) > 0);

    // Frequency sweep overflow check
    if ((sweep & 0x7) > 0) {
        CalculateFrequency();
    }

    PulseChannel2::Trigger();
}

PulseChannel2::PulseChannel2()
{
    Trigger();
}

void PulseChannel2::Trigger()
{
    channelEnabled = true;

    // Envelope sweep setup
    freqTimer += (2048 - (((trigger & 0x7) << 8) + freq)) * 4;
    periodTimer = (volume & 0x7);
    currentVolume = ((volume & 0xF0) >> 4);

    // Length counter
    if (lengthCounter == 0) {
        lengthCounter = 0x40;
    }

    // Handle DAC disabled  
    if (!(volume&0xF8)) {
        channelEnabled = false;
    }
}

WaveChannel::WaveChannel()
{
    Trigger();
}

void WaveChannel::Trigger()
{
    channelEnabled = true;
    freqTimer += (2048 - (((trigger & 0x7) << 8) + freq)) * 2;
    // Reset the position but leave the current sample
    wavPos = 0;

    // Length counter
    if (lengthCounter == 0) {
        lengthCounter = 0x100;
    } 

    // Handle DAC disabled  
    if (!(dac&0x80)) {
        channelEnabled = false;
    }
}

NoiseChannel::NoiseChannel()
{
    Trigger();
}

void NoiseChannel::Trigger()
{
    byte shiftAmount = (clock&0xF0) >> 4;
    byte divisorCode = (clock&0x7);

    channelEnabled = true;
    // Reset LFSR
    LFSR = 0x7FFF;

    // Envelope sweep setup
    freqTimer += (divisorCode > 0 ? (divisorCode << 4) : 8) << shiftAmount;
    periodTimer = (volume & 0x7);
    currentVolume = ((volume & 0xF0) >> 4);

    // Length counter
    if (lengthCounter == 0) {
        lengthCounter = 0x40;
    }

    // Handle DAC disabled  
    if (!(volume&0xF8)) {
        channelEnabled = false;
    }
}

void PulseChannel1::Cycle()
{
    byte adjustment;
    int newFrequency;
    byte sweepPeriod;

    // Adjust frequency from frame sequencer
    if (runFrequencySweep) {

        if (sweepTimer > 0) {
            sweepTimer -= 1;
        }

        if (sweepTimer == 0) {
            sweepPeriod = (sweep & 0x70) >> 4;
            if (sweepPeriod > 0) {
                sweepTimer = sweepPeriod;
            } else {
                sweepTimer = 0x8;
            }

            if (sweepEnabled && sweepPeriod > 0) {
                newFrequency = CalculateFrequency();

                if (newFrequency <= 2047 && (sweep & 0x7) > 0) {
                    // Least significant bits
                    freq = (0 | (newFrequency & 0xFF)); 
                    // Most significant bits
                    trigger &= ~(0x7); 
                    trigger += ((newFrequency&0x700) >> 8);

                    shadowFrequency = newFrequency;

                    // Overflow check
                    CalculateFrequency();
                }
            }
        }
    }

    runFrequencySweep = false;

    PulseChannel2::Cycle();
}

void PulseChannel2::Cycle()
{
    // Clear Channel data
    channelData = 0;

    // Adjust envelope from frame sequencer
    EnvelopeSweep();

    // Advance channel frequency timer
    // Step wave duty when timer reaches zero
    freqTimer -= 4;
    if (freqTimer <= 0) {
        dutyPos = (dutyPos+1) % 8;
        freqTimer += (2048 - (((trigger & 0x7) << 8) + freq)) * 4;
    }

    // Disable if length counter 0
    LengthCounter();
    
    // Silence if channel is disabled
    if (!channelEnabled) {
        channelData = 0;
        return;
    }

    channelData = ((dutyWaveTable[(duty & 0xC0) >> 6] & (1<<dutyPos)) >> dutyPos) * (0.1 * currentVolume);

}

void WaveChannel::Cycle()
{
    byte sample;

    // Clear channel data
    channelData = 0;

    // Advance channel frequency timer
    // Step wave duty when timer reaches zero
    freqTimer -= 4;
    if (freqTimer <= 0) {
        
        // Reload wave sample if crossing 2 byte boundary
        wavPos = (wavPos+1) % 32;
        if (wavPos % 2 == 0) {
            wavSample = mmu.PeekByte(0xFF30 + (wavPos / 2));
        }

        freqTimer += (2048 - (((trigger & 0x7) << 8) + freq)) * 2;
        //printf("frer %x, trig %x, freq %x, wavsample %x, sample %x, pos %x\n", freq,((trigger & 0x7) << 8),freqTimer, wavSample, sample, (wavPos/2));
    }

    if (wavPos % 2 == 0) {
        sample = wavSample >> 4;
    } else {
        sample = wavSample & 0xF;
    }

    // Volume control
    switch ((volume & 0x60) >> 5) 
    {
        case 0:
            sample >>= 4;
            break;
        case 1:
            break;
        case 2:
            sample >>= 1;
            break;
        case 3:
            sample >>= 2;
            break;
    }


    // Disable if length counter 0
    if ((trigger & 0x40) && runLengthCounterSweep) {

        if (lengthCounter > 0) {
            lengthCounter -= 1;
        }

        if (lengthCounter == 0) {
            channelEnabled = false;
        }
    }
    runLengthCounterSweep = false;
    
    // Silence if channel is disabled OR DAC is off
    if (!channelEnabled) {
        channelData = 0;
        return;
    }

    channelData = sample*0.1; 
}

void NoiseChannel::Cycle()
{
    byte adjustment;
    byte xorResult;
    byte shiftAmount = (clock&0xF0) >> 4;
    byte divisorCode = (clock&0x7);
    bool widthMode = (clock&0x8);
    float leftChannel;
    float rightChannel;

    // Clear channel data
    channelData = 0;

    // Adjust envelope from frame sequencer
    EnvelopeSweep();

    //Peform LFSR cycle
    freqTimer -= 4;

    if (freqTimer <= 0) {
        freqTimer += (divisorCode > 0 ? (divisorCode << 4) : 8) << shiftAmount;

        xorResult = (LFSR & 0x1) ^ ((LFSR & 0x2) >> 1);
        LFSR = (LFSR >> 1) | (xorResult << 14);

        if (widthMode) {
            LFSR &= ~(1 << 6);
            LFSR |= xorResult << 6;
        }
    }

    // Disable if length counter 0
    LengthCounter();

    // Silence if channel is disabled OR DAC is off
    if (!channelEnabled) {
        channelData = 0;
        return;
    }

    channelData = (~(LFSR) & 0x01) * (0.1 * currentVolume);
}

void AudioComponent::Cycle() 
{
    bool incFs = false;
    word out = 0;

    // TODO: Dirty rotten hack
    timeRegs.DIV -= cpuCycles;

    // Run if APU is turned on
    if (audioRegs.ctrl.power&0x80) {

        for (int i = 0; i < (cpuCycles/4); i++) {
                
            audioCycles += 4;
            timeRegs.DIV += 4;

            out = (timeRegs.DIV & 0x1000);
            if (!out && fsEdge) {
                fsEdge = false;
                incFs = true;
            } else if (out) {
                fsEdge = true;
            }

            // Frame sequencer goes up at fixed rate 512hz
            if (incFs) {
                incFs = false;
                fsStep++;
                fsStep = fsStep % 8;

                // Enable frequency sweep for c1
                if (fsStep == 2 || fsStep == 6) {
                    pulseChannel1.runFrequencySweep = true;
                }

                // Enable length counter sweep for all channels
                if (fsStep % 2 == 0) {
                    pulseChannel1.runLengthCounterSweep = true;
                    pulseChannel2.runLengthCounterSweep = true;
                    waveChannel.runLengthCounterSweep = true;
                    noiseChannel.runLengthCounterSweep = true;
                }

                // Enable envelope sweep for c1/c2/c3
                if (fsStep == 7) {
                    pulseChannel1.runEnvelopeSweep = true;
                    pulseChannel2.runEnvelopeSweep = true;
                    noiseChannel.runEnvelopeSweep = true;
                }

            }

            // Cycle audio channels
            pulseChannel1.Cycle();
            pulseChannel2.Cycle();
            waveChannel.Cycle();
            noiseChannel.Cycle();

        }
    }

    if (audioCycles >= 87) {
        // Fill audio buffer every 1/48000 of a second

        // Left Channel
        // Add and average channel amplitudes, taking into account panning registers and DAC 
        mixData[2*bufferCursor+0] += (audioRegs.ctrl.lrEnable&0x10) ? pulseChannel1.channelData : 0;
        mixData[2*bufferCursor+0] += (audioRegs.ctrl.lrEnable&0x20) ? pulseChannel2.channelData : 0;
        mixData[2*bufferCursor+0] += (audioRegs.ctrl.lrEnable&0x40) ? waveChannel.channelData : 0;
        mixData[2*bufferCursor+0] += (audioRegs.ctrl.lrEnable&0x80) ? noiseChannel.channelData : 0;
        mixData[2*bufferCursor+0] /= 4;
        mixData[2*bufferCursor+0] = (mixData[2*bufferCursor+0] / 7.5);
        mixData[2*bufferCursor+0] *= (audioRegs.ctrl.vinVol&0x70>>4);

        // Right Channel
        // Add and average channel amplitudes, taking into account panning registers
        mixData[2*bufferCursor+1] += (audioRegs.ctrl.lrEnable&0x01) ? pulseChannel1.channelData : 0;
        mixData[2*bufferCursor+1] += (audioRegs.ctrl.lrEnable&0x02) ? pulseChannel2.channelData : 0;
        mixData[2*bufferCursor+1] += (audioRegs.ctrl.lrEnable&0x04) ? waveChannel.channelData : 0;
        mixData[2*bufferCursor+1] += (audioRegs.ctrl.lrEnable&0x08) ? noiseChannel.channelData : 0;
        mixData[2*bufferCursor+1] /= 4;
        mixData[2*bufferCursor+1] = (mixData[2*bufferCursor+1] / 7.5);
        mixData[2*bufferCursor+1] *= (audioRegs.ctrl.vinVol&0x7);

        bufferCursor++;

        audioCycles -= 87;

        // Playback audio when buffer is full
        if (bufferCursor >= 1024) {
            bufferCursor = 0;
           
            SDL_QueueAudio(audio_device_id, &mixData, 8192);
            while (SDL_GetQueuedAudioSize(audio_device_id) >= 8192);
        }
    }
}

void AudioComponent::Reset() 
{
    // Set frequency timers
    pulseChannel1.Trigger();
    pulseChannel2.Trigger();
    waveChannel.Trigger();
    noiseChannel.Trigger();

    uint64_t samples_played = 0;

    if(SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        fprintf(stderr, "Error initializing SDL. SDL_Error: %s\n", SDL_GetError());
        exit(1);        
    }


    SDL_AudioSpec audio_spec_want, audio_spec;
    SDL_memset(&audio_spec_want, 0, sizeof(audio_spec_want));

    // UPDATE 20/11/22 - changed 'audioCycles = 0' to 'audioCycles -= 87' - tetris and SML now sound perfect at 48000 and this also fixed some fuzzy tones at higher pitches
    // Original comment: fix timing issues throughout ArkGB - sample rate should be 48000 but this is too fast for SML and tetris in ArkGB's current state. 
    // A timer is running too fast, or a cycle count is not right somewhere
    // NOTE- currently SML sounds perfect at 46000hz, tetris sounds perfect at 44100hz. 
    audio_spec_want.freq     = 48000;
    audio_spec_want.format   = AUDIO_F32;
    audio_spec_want.channels = 2;
    audio_spec_want.samples  = 1024;
    audio_spec_want.userdata = (void*)&samples_played;

    audio_device_id = SDL_OpenAudioDevice(
        NULL, 0,
        &audio_spec_want, &audio_spec,
        SDL_AUDIO_ALLOW_FORMAT_CHANGE
    );

    if(!audio_device_id)
    {
        fprintf(stderr, "Error creating SDL audio device. SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }
     SDL_PauseAudioDevice(audio_device_id, 0);

}
