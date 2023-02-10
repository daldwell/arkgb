#include "Typedefs.h"
#include "Control.h"
#include "Interrupt.h"
#include "Logger.h"
#include <assert.h>

const int MASK_0 = 1;
const int MASK_1 = 1<<1;
const int MASK_2 = 1<<2;
const int MASK_3 = 1<<3;

byte JOYP = 0xFF;
byte Direction = 0xFF;
byte Action = 0xFF;

void UpdateKeyReg(byte * reg, int mask, bool pressed) 
{
    if (pressed) {
        *reg = ~(mask);
    } else {
        *reg |= mask;
    }
}

void KeyHandle(JoyPadKey k, bool pressed) 
{
    switch(k)
    {
        case Up:
            UpdateKeyReg(&Direction, MASK_2, pressed);
            break;
        case Down:
            UpdateKeyReg(&Direction, MASK_3, pressed);
            break;
        case Left:
            UpdateKeyReg(&Direction, MASK_1, pressed);
            break;
        case Right:
            UpdateKeyReg(&Direction, MASK_0, pressed);
            break;
        case Start:
            UpdateKeyReg(&Action, MASK_3, pressed);
            break;
        case Select:
            UpdateKeyReg(&Action, MASK_2, pressed);
            break;
        case A:
            UpdateKeyReg(&Action, MASK_0, pressed);
            break;
        case B:
            UpdateKeyReg(&Action, MASK_1, pressed);
            break;
        default:
            Log("Key not mapped to register", ERROR);
            exit(1);
    }
}

JoyPadKey KeyMap(SDL_Scancode c) 
{
    switch (c)
    {
        case SDL_SCANCODE_LEFT:
            return Left;
        case SDL_SCANCODE_RIGHT:
            return Right;
        case SDL_SCANCODE_UP:
            return Up;
        case SDL_SCANCODE_DOWN:
            return Down;
        case SDL_SCANCODE_RETURN:
            return Start;
        case SDL_SCANCODE_BACKSPACE:
            return Select;
        case SDL_SCANCODE_S:
            return A;
        case SDL_SCANCODE_A:
            return B;
    };
    
    Log("SDL Key not mapped", DEBUG);
    return None;
}

void ControlComponent::EventHandler(SDL_Event * e)
{
    if ( e->type == SDL_KEYUP || e->type == SDL_KEYDOWN )
    {
        JoyPadKey key = KeyMap(e->key.keysym.scancode);

        if (key == None)
            return;

        bool pressed = (e->type == SDL_KEYDOWN) ? true : false;
        KeyHandle(key, pressed);
    }
}

void ControlComponent::PokeByte(word, byte)
{
    // not implemented
}

byte ControlComponent::PeekByte(word)
{
    // not implemented
}

void ControlComponent::Cycle()
{
    // not implemented
}

void ControlComponent::Reset()
{
    // not implemented
}

bool ControlComponent::MemoryMapped(word addr)
{
    return false;
}