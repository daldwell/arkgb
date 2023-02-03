#include "Typedefs.h"

#pragma once

enum JoyPadKey
{
    Up = 0,
    Down,
    Left,
    Right,
    Start,
    Select,
    A,
    B,
    None
};


extern byte JOYP;
extern byte Direction;
extern byte Action;

void ProcessKey(JoyPadKey, bool);