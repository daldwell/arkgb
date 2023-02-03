#include <stdlib.h>
#include <assert.h>
#include "Cpu.h"
#include "Mmu.h"
#include "opc.h"
#include "Interrupt.h"
#include "typedefs.h"
#include "GUnit.h"

#define cycle 4

Instruction instTbl[256];
Instruction instTblX[256];

// Conditional flag helpers

byte complement(byte a, byte b)
{
    return ~(a)+b;
}

void setZConditional(byte r)
{
    if (r == 0) setFlag(ZF, true); else setFlag(ZF, false); 
}

void setHConditionalA(byte a, byte b, byte c = 0)
{
    if ((((a & 0xf) + (b & 0xf) + (c & 0xf)) & 0x10) == 0x10) setFlag(HF, true); else setFlag(HF, false);
}

// void setHConditionalS(byte a, byte b, byte c = 0)
// {
//     if ((a & 0xf) < ((b & 0xf) + (c & 0xf))) setFlag(HF, true); else setFlag(HF, false);
// }

void setHConditionalS(byte a, byte b, byte c = 0)
{
    if ((((a & 0xf) + (~b & 0xf) + (c^1 & 0xf)) & 0x10) == 0x10) setFlag(HF, false); else setFlag(HF, true);
}

void setCConditionalA(byte a, byte b, byte c = 0)
{
    if ((((a & 0xff) + (b & 0xff) + (c & 0xff)) & 0x100) == 0x100) setFlag(CF, true); else setFlag(CF, false);
}

// void setCConditionalS(byte a, byte b, byte c = 0)
// {
//     if ((a & 0xff) < ((b & 0xff) + (c & 0xff))) setFlag(CF, true); else setFlag(CF, false);
// }

void setCConditionalS(byte a, byte b, byte c = 0)
{
    if ((((a & 0xff) + (~b & 0xff) + (c^1 & 0xff)) & 0x100) == 0x100) setFlag(CF, false); else setFlag(CF, true);
}

// NOP opcode impl
void NOP()
{
    return;
}

void PUSH(word * a)
{
    regs.SP -= 2;
    mmu.PokeWord(regs.SP, *a);
}

void POP(word * a)
{
    *a = mmu.PeekWord(regs.SP);
    regs.SP += 2;
}

// Edge case as F register needs special attention (lower 4 bits are not set)
void POPAF(word * a)
{
    *a = (mmu.PeekWord(regs.SP) & 0xFFF0);
    regs.SP += 2;
}

// LD opcode impl
void LD(byte * a, byte b)
{
    *a = b;
}

void LD(word * a, word b)
{
    *a = b;
}

void LD(word a, byte b)
{
    mmu.PokeByte(a, b);
}

void LD(word a, word b)
{
    mmu.PokeWord(a, b);
}

// LDH opcode impl
void LDH(byte * a, byte b)
{
    LD(a, b);
}

// LDH opcode impl
void LDH(word a, byte b)
{
    mmu.PokeByte(a, b);
}

// INC opcode impl
void INC(byte * a)
{
    *a += 1;
     setZConditional(*a);
     setFlag(NF, false);
     setHConditionalA(*a-1, 1);
}

void INC(word a)
{
    byte b = mmu.PeekByte(a);
    INC(&b);
    mmu.PokeByte(a, b);
}


void INC(word * a)
{
    *a += 1;
}

// DEC opcode impl
void DEC(byte * a) 
{
    *a -= 1;
    setZConditional(*a);
    setFlag(NF, true);
    setHConditionalS(*a+1, 1);
}

void DEC(word a) 
{
    byte b = mmu.PeekByte(a);
    DEC(&b);
    mmu.PokeByte(a, b);
}

void DEC(word * a) 
{
    *a -= 1;
}

// ADD opcode impl
void ADD(byte * a, byte b)
{
    // Check carry flags pre-computation as we need uncomputed operand A
    setHConditionalA(*a, b);
    setCConditionalA(*a, b);
    // Compute result and check remaining flags
    *a += b;
    setZConditional(*a);
    setFlag(NF, false);
}

void ADD(word * a, word b)
{
    // Get the carry flag status for the lower 8 bits
    setCConditionalA(*a, b);

    // ... and feed this into the calculation for the higher 8 bits
    setHConditionalA(*a>>8, b>>8, getFlag(CF));
    setCConditionalA(*a>>8, b>>8, getFlag(CF));    
    *a += b;
    setFlag(NF, false);
}

void ADD_HL(word * a, sbyte b)
{
    // For 16 bit add operation, calculate the flags for the lower 8 bits only (GB manual incorrectly states upper 16 bits)
    // Check carry flags pre-computation as we need uncomputed operand A
    setHConditionalA(*a, b);
    setCConditionalA(*a, b);
    *a += b;
    setFlag(ZF, false);
    setFlag(NF, false);
}

// ADC opcode impl
void ADC(byte * a, byte b)
{
    bool c = getFlag(CF);
    setHConditionalA(*a, b, c);
    setCConditionalA(*a, b, c);
    *a += b + c;
    setZConditional(*a);
    setFlag(NF, false);
}

// SUB opcode impl
void SUB(byte * a, byte b)
{
    // Check carry flags pre-computation as we need uncomputed operand A
    setHConditionalS(*a, b);
    setCConditionalS(*a, b);
    // Compute result and check remaining flags
    *a -= b;
    setZConditional(*a);
    setFlag(NF, true);
}

void SUB(byte b)
{
    SUB(&regs.A, b);
}

void SUB(byte * b)
{
    SUB(&regs.A, *b);
}

void SUB(word a) 
{
    byte b = mmu.PeekByte(a);
    SUB(&b);
}

// SBC opcode impl
void SBC(byte * a, byte b)
{
    bool c = getFlag(CF);
    setHConditionalS(*a, b, c);
    setCConditionalS(*a, b, c);
    *a -= b + c;
    setZConditional(*a);
    setFlag(NF, true);

}

// AND opcode impl
void AND(byte a)
{
    regs.A &= a;
    setZConditional(regs.A);
    setFlag(NF, false);
    setFlag(HF, true);
    setFlag(CF, false);
}

void AND(byte * a)
{
    AND(*a);
}

void AND(word a) 
{
    byte b = mmu.PeekByte(a);
    AND(&b);
}


// AND opcode impl
void OR(byte b)
{
    regs.A |= b;
    setZConditional(regs.A);
    setFlag(NF, false);
    setFlag(HF, false);
    setFlag(CF, false);
}

void OR(byte * b)
{
    OR(*b);
}

void OR(word a) 
{
    byte b = mmu.PeekByte(a);
    OR(&b);
}

// XOR opcode impl
void XOR(byte a)
{
    regs.A ^= a;
    setZConditional(regs.A);
    setFlag(NF, false);
    setFlag(HF, false);
    setFlag(CF, false);
}

void XOR(byte * a)
{
    XOR(*a);
}

void XOR(word a) 
{
    byte b = mmu.PeekByte(a);
    XOR(&b);
}

// CP opcode impl
void CP(byte b)
{
    byte tmp = regs.A;
    SUB(&tmp, b);
}

void CP(byte * b)
{
    CP(*b);
}

void CP(word a) 
{
    byte b = mmu.PeekByte(a);
    CP(&b);
}

// RLCA opcode impl
void RLCA()
{
    setFlag(CF, (0x80&regs.A)>>7);
    regs.A <<= 1;
    regs.A += getFlag(CF);
    setFlag(NF, false);
    setFlag(HF, false);
    setFlag(ZF, false);
}

// RRCA opcode impl
void RRCA()
{
    setFlag(CF, 0x1&regs.A);
    regs.A >>= 1;
    if (getFlag(CF)) regs.A |= 0x80; else regs.A &= 0x7F;
    setFlag(NF, false);
    setFlag(HF, false);
    setFlag(ZF, false);
}

// RLA opcode impl
void RLA()
{
    bool tmp = getFlag(CF);
    setFlag(CF, 0x80&regs.A);
    regs.A <<= 1;
    if (tmp) regs.A |= 1; else regs.A &= 0xfe;
    setFlag(NF, false);
    setFlag(HF, false);
    setFlag(ZF, false);
}

// RRA opcode impl
// TODO: flags
void RRA()
{
    bool tmp = getFlag(CF);
    setFlag(CF, 0x1&regs.A);
    regs.A >>= 1;
    if (tmp) regs.A |= 0x80; else regs.A &= 0x7F;
    setFlag(NF, false);
    setFlag(HF, false);
    setFlag(ZF, false);
}

// STOP opcode impl
// TODO: 
void STOP(int)
{
    //stop = true;
}

// HALT opcode impl
void HALT()
{
    halt = true;
}

// DAA opcode impl
void DAA()
{
    if (getFlag(NF)) {
         if (getFlag(CF)) regs.A -= 0x60;
         if (getFlag(HF)) regs.A -= 0x06;
    } else {
        if (getFlag(CF) || (regs.A > 0x99) ) {
            // Check for invalid BCD result
            //assert(getFlag(CF) != true || ((regs.A&0xf0)>>4) <= (0x2+getFlag(HF)));
            regs.A += 0x60;
            setFlag(CF, true);
        }

        if (getFlag(HF) || ((regs.A&0xf) > 0x09)) {
            // We can't add 6 to 4 because this produces an invalid BCD result of 0xA. 
            // This shouldn't happen because this condition indicates an invalid BCD calculation to start with (i.e. 0x79 + 0xB)
            //assert(getFlag(HF) != true || (regs.A&0xf) <= 0x3);
            regs.A += 0x06;
        }
    }

    setZConditional(regs.A);
    setFlag(HF, false);
}

// JR opcode impl
void JR(sbyte a)
{
    regs.PC += (a+2);
    regs.jmp = true;
}

void JR(JumpCode jc, sbyte a)
{
    switch (jc)
    {
        case Z:
            if (getFlag(ZF)) JR(a);
            break;
        case NZ:
            if (!getFlag(ZF)) JR(a);
            break;
        case C:
            if (getFlag(CF)) JR(a);
            break;
        case NC:
            if (!getFlag(CF)) JR(a);
            break;
    }    
}

// JP opcode impl
void JP(word a)
{
    regs.PC = a;
    regs.jmp = true;
}

void JP(word * a)
{
    JP(*a);
}

void JP(JumpCode jc, word a)
{
    switch (jc)
    {
        case Z:
            if (getFlag(ZF)) JP(a);
            break;
        case NZ:
            if (!getFlag(ZF)) JP(a);
            break;
        case C:
            if (getFlag(CF)) JP(a);
            break;
        case NC:
            if (!getFlag(CF)) JP(a);
            break;
    }  
}

// CALL opcode impl
void CALL(word a, int length=3)
{
    // Get next address (PC + length of operand, always word length for CALL instruction)
    word tmp = regs.PC+length;
    PUSH(&tmp);
    regs.PC = a;
    // Add extra cycles for successful jump
    regs.jmp = true;
}

void CALL(JumpCode jc, word a)
{
    switch (jc)
    {
        case Z:
            if (getFlag(ZF)) CALL(a);
            break;
        case NZ:
            if (!getFlag(ZF)) CALL(a);
            break;
        case C:
            if (getFlag(CF)) CALL(a);
            break;
        case NC:
            if (!getFlag(CF)) CALL(a);
            break;
    }
}


// RST opcode impl
void RST(byte a)
{
    CALL(a, 1);
}


// RET opcode impl
void RET()
{
    POP(&regs.PC);
    regs.jmp = true;
}

void RET(JumpCode jc)
{
    switch (jc)
    {
        case Z:
            if (getFlag(ZF)) RET();
            break;
        case NZ:
            if (!getFlag(ZF)) RET();
            break;
        case C:
            if (getFlag(CF)) RET();
            break;
        case NC:
            if (!getFlag(CF)) RET();
            break;
    } 
}

// DI opcode impl
void DI()
{
    IMECounter = 0;
    IMERegister = false;
}

// EI opcode impl
void EI()
{
    IMECounter = 2;
}

// RETI opcode impl
void RETI()
{
    EI();
    RET();
}

// LDH opcode impl
void LDHL()
{
    word tmp = regs.SP;
    ADD_HL(&tmp, (sbyte)mmu.PeekByte(regs.PC+1));
    LD(&regs.HL, tmp);
}

void RETI(JumpCode jc)
{
    switch (jc)
    {
        case Z:
            if (getFlag(ZF)) RETI();
            break;
        case NZ:
            if (!getFlag(ZF)) RETI();
            break;
        case C:
            if (getFlag(CF)) RETI();
            break;
        case NC:
            if (!getFlag(CF)) RETI();
            break;
    }   
}

// CPL opcode impl
void CPL()
{
    setFlag(HF, true);
    setFlag(NF, true);
    regs.A = ~regs.A;
}

// SCF opcode impl
void SCF()
{
    setFlag(HF, false);
    setFlag(NF, false);
    setFlag(CF, true);
}

// CCF opcode impl
void CCF()
{
    setFlag(HF, false);
    setFlag(NF, false);
    bool cF = getFlag(CF);
    setFlag(CF, (cF ^= 0x1));  
}

// PREFIX opcode impl
void PREFIX(byte a)
{
    // call extended instruction
    instTblX[a].step();
}

void RLC(byte * a)
{
    byte tmp = (0x80&*a)>>7;
    setFlag(CF, tmp);
    *a <<= 1;
    *a += tmp;
    setFlag(NF, false);
    setFlag(HF, false);
    setZConditional(*a);
}

void RLC(word a) 
{
    byte b = mmu.PeekByte(a);
    RLC(&b);
    mmu.PokeByte(a, b);
}

void RRC(byte * a)
{
    byte tmp = (0x1&*a);
    setFlag(CF, tmp);
    *a >>= 1;
    *a |= (tmp<<7);
    setFlag(NF, false);
    setFlag(HF, false);
    setZConditional(*a);
}

void RRC(word a) 
{
    byte b = mmu.PeekByte(a);
    RRC(&b);
    mmu.PokeByte(a, b);
}

void RL(byte * a)
{
    byte tmpC = getFlag(CF);
    setFlag(CF, (0x80&*a));
    *a = (*a<<1) + tmpC;
    setZConditional(*a);
    setFlag(HF, false);
    setFlag(NF, false);
}

void RL(word a) 
{
    byte b = mmu.PeekByte(a);
    RL(&b);
    mmu.PokeByte(a, b);
}

void RR(byte * a)
{
    byte tmpC = getFlag(CF);
    setFlag(CF, (0x1 & *a));
    *a = ((tmpC<<7)) + (*a>>1);
    setZConditional(*a);
    setFlag(HF, false);
    setFlag(NF, false);
}

void RR(word a) 
{
    byte b = mmu.PeekByte(a);
    RR(&b);
    mmu.PokeByte(a, b);
}

void SLA(byte * a)
{
    setFlag(CF, (0x80&*a)>>7);
    *a <<= 1;
    *a &= 0xFE;
    setZConditional(*a);
    setFlag(HF, false);
    setFlag(NF, false);
}

void SLA(word a) 
{
    byte b = mmu.PeekByte(a);
    SLA(&b);
    mmu.PokeByte(a, b);
}

void SRA(byte * a)
{
    setFlag(CF, (0x1 & *a));
    *a = (0x80&*a) + (*a>>1);
    setZConditional(*a);
    setFlag(HF, false);
    setFlag(NF, false);
}

void SRA(word a) 
{
    byte b = mmu.PeekByte(a);
    SRA(&b);
    mmu.PokeByte(a, b);
}


void SWAP(byte * a)
{
    *a = ((*a << 4) & 0xFF | (*a >> 4));
    setZConditional(*a);
    setFlag(CF, false);
    setFlag(HF, false);
    setFlag(NF, false);
}

void SWAP(word a) 
{
    byte b = mmu.PeekByte(a);
    SWAP(&b);
    mmu.PokeByte(a, b);
}

void SRL(byte * a)
{
    setFlag(CF, (0x1 & *a));
    *a >>= 1;
    setZConditional(*a);
    setFlag(HF, false);
    setFlag(NF, false);
}

void SRL(word a) 
{
    byte b = mmu.PeekByte(a);
    SRL(&b);
    mmu.PokeByte(a, b);
}

void BIT(byte a, byte * b)
{
    setFlag(ZF, ((1<<a) & *b) ? false : true);
    setFlag(HF, 1);
    setFlag(NF, 0);
}

void BIT(byte a, word b) 
{
    byte c = mmu.PeekByte(b);
    BIT(a, &c);
}


void RES(byte a, byte * b)
{
    *b &= ~(1 << a);
}

void RES(byte a, word b)
{
    byte c = mmu.PeekByte(b);
    RES(a, &c);
    mmu.PokeByte(b, c);
}

void SET(byte a, byte * b)
{
    *b |= (1 << a);
}

void SET(byte a, word b)
{
    byte c = mmu.PeekByte(b);
    SET(a, &c);
    mmu.PokeByte(b, c);
}


void initOpc()
{
    // Unprefixed instructions
    instTbl[0x00] = {"NOP", "NOP", 1, {4}, [] () { NOP(); }};
    instTbl[0x01] = {"LD", "LD BC,d16", 3, {12}, [] () { LD(&regs.BC,mmu.PeekWord(regs.PC+1)); }};
    instTbl[0x02] = {"LD", "LD (BC),A", 1, {8}, [] () { LD(regs.BC,regs.A); }};
    instTbl[0x03] = {"INC", "INC BC", 1, {8}, [] () { INC(&regs.BC); }};
    instTbl[0x04] = {"INC", "INC B", 1, {4}, [] () { INC(&regs.B); }};
    instTbl[0x05] = {"DEC", "DEC B", 1, {4}, [] () { DEC(&regs.B); }};
    instTbl[0x06] = {"LD", "LD B,d8", 2, {8}, [] () { LD(&regs.B,mmu.PeekByte(regs.PC+1)); }};
    instTbl[0x07] = {"RLCA", "RLCA", 1, {4}, [] () { RLCA(); }};
    instTbl[0x08] = {"LD", "LD (a16),SP", 3, {20}, [] () { LD(mmu.PeekWord(regs.PC+1),regs.SP); }};
    instTbl[0x09] = {"ADD", "ADD HL,BC", 1, {8}, [] () { ADD(&regs.HL,regs.BC); }};
    instTbl[0x0a] = {"LD", "LD A,(BC)", 1, {8}, [] () { LD(&regs.A,mmu.PeekByte(regs.BC)); }};
    instTbl[0x0b] = {"DEC", "DEC BC", 1, {8}, [] () { DEC(&regs.BC); }};
    instTbl[0x0c] = {"INC", "INC C", 1, {4}, [] () { INC(&regs.C); }};
    instTbl[0x0d] = {"DEC", "DEC C", 1, {4}, [] () { DEC(&regs.C); }};
    instTbl[0x0e] = {"LD", "LD C,d8", 2, {8}, [] () { LD(&regs.C,mmu.PeekByte(regs.PC+1)); }};
    instTbl[0x0f] = {"RRCA", "RRCA", 1, {4}, [] () { RRCA(); }};
    instTbl[0x10] = {"STOP", "STOP 0", 1, {4}, [] () { STOP(0); }};
    instTbl[0x11] = {"LD", "LD DE,d16", 3, {12}, [] () { LD(&regs.DE,mmu.PeekWord(regs.PC+1)); }};
    instTbl[0x12] = {"LD", "LD (DE),A", 1, {8}, [] () { LD(regs.DE,regs.A); }};
    instTbl[0x13] = {"INC", "INC DE", 1, {8}, [] () { INC(&regs.DE); }};
    instTbl[0x14] = {"INC", "INC D", 1, {4}, [] () { INC(&regs.D); }};
    instTbl[0x15] = {"DEC", "DEC D", 1, {4}, [] () { DEC(&regs.D); }};
    instTbl[0x16] = {"LD", "LD D,d8", 2, {8}, [] () { LD(&regs.D,mmu.PeekByte(regs.PC+1)); }};
    instTbl[0x17] = {"RLA", "RLA", 1, {4}, [] () { RLA(); }};
    instTbl[0x18] = {"JR", "JR r8", 2, {12}, [] () { JR((sbyte)mmu.PeekByte(regs.PC+1)); }};
    instTbl[0x19] = {"ADD", "ADD HL,DE", 1, {8}, [] () { ADD(&regs.HL,regs.DE); }};
    instTbl[0x1a] = {"LD", "LD A,(DE)", 1, {8}, [] () { LD(&regs.A,mmu.PeekByte(regs.DE)); }};
    instTbl[0x1b] = {"DEC", "DEC DE", 1, {8}, [] () { DEC(&regs.DE); }};
    instTbl[0x1c] = {"INC", "INC E", 1, {4}, [] () { INC(&regs.E); }};
    instTbl[0x1d] = {"DEC", "DEC E", 1, {4}, [] () { DEC(&regs.E); }};
    instTbl[0x1e] = {"LD", "LD E,d8", 2, {8}, [] () { LD(&regs.E,mmu.PeekByte(regs.PC+1)); }};
    instTbl[0x1f] = {"RRA", "RRA", 1, {4}, [] () { RRA(); }};
    instTbl[0x20] = {"JR", "JR NZ,r8", 2, {12,8}, [] () { JR(NZ,(sbyte)mmu.PeekByte(regs.PC+1)); }};
    instTbl[0x21] = {"LD", "LD HL,d16", 3, {12}, [] () { LD(&regs.HL,mmu.PeekWord(regs.PC+1)); }};
    instTbl[0x22] = {"LD", "LD (HL+),A", 1, {8}, [] () { LD(regs.HL++,regs.A); }};
    instTbl[0x23] = {"INC", "INC HL", 1, {8}, [] () { INC(&regs.HL); }};
    instTbl[0x24] = {"INC", "INC H", 1, {4}, [] () { INC(&regs.H); }};
    instTbl[0x25] = {"DEC", "DEC H", 1, {4}, [] () { DEC(&regs.H); }};
    instTbl[0x26] = {"LD", "LD H,d8", 2, {8}, [] () { LD(&regs.H,mmu.PeekByte(regs.PC+1)); }};
    instTbl[0x27] = {"DAA", "DAA", 1, {4}, [] () { DAA(); }};
    instTbl[0x28] = {"JR", "JR Z,r8", 2, {12,8}, [] () { JR(Z,(sbyte)mmu.PeekByte(regs.PC+1)); }};
    instTbl[0x29] = {"ADD", "ADD HL,HL", 1, {8}, [] () { ADD(&regs.HL,regs.HL); }};
    instTbl[0x2a] = {"LD", "LD A,(HL+)", 1, {8}, [] () { LD(&regs.A,mmu.PeekByte(regs.HL++)); }};
    instTbl[0x2b] = {"DEC", "DEC HL", 1, {8}, [] () { DEC(&regs.HL); }};
    instTbl[0x2c] = {"INC", "INC L", 1, {4}, [] () { INC(&regs.L); }};
    instTbl[0x2d] = {"DEC", "DEC L", 1, {4}, [] () { DEC(&regs.L); }};
    instTbl[0x2e] = {"LD", "LD L,d8", 2, {8}, [] () { LD(&regs.L,mmu.PeekByte(regs.PC+1)); }};
    instTbl[0x2f] = {"CPL", "CPL", 1, {4}, [] () { CPL(); }};
    instTbl[0x30] = {"JR", "JR NC,r8", 2, {12,8}, [] () { JR(NC,(sbyte)mmu.PeekByte(regs.PC+1)); }};
    instTbl[0x31] = {"LD", "LD SP,d16", 3, {12}, [] () { LD(&regs.SP,mmu.PeekWord(regs.PC+1)); }};
    instTbl[0x32] = {"LD", "LD (HL-),A", 1, {8}, [] () { LD(regs.HL--,regs.A); }};
    instTbl[0x33] = {"INC", "INC SP", 1, {8}, [] () { INC(&regs.SP); }};
    instTbl[0x34] = {"INC", "INC (HL)", 1, {12}, [] () { INC(regs.HL); }};
    instTbl[0x35] = {"DEC", "DEC (HL)", 1, {12}, [] () { DEC(regs.HL); }};
    instTbl[0x36] = {"LD", "LD (HL),d8", 2, {12}, [] () { LD(regs.HL,mmu.PeekByte(regs.PC+1)); }};
    instTbl[0x37] = {"SCF", "SCF", 1, {4}, [] () { SCF(); }};
    instTbl[0x38] = {"JR", "JR C,r8", 2, {12,8}, [] () { JR(C,(sbyte)mmu.PeekByte(regs.PC+1)); }};
    instTbl[0x39] = {"ADD", "ADD HL,SP", 1, {8}, [] () { ADD(&regs.HL,regs.SP); }};
    instTbl[0x3a] = {"LD", "LD A,(HL-)", 1, {8}, [] () { LD(&regs.A,mmu.PeekByte(regs.HL--)); }};
    instTbl[0x3b] = {"DEC", "DEC SP", 1, {8}, [] () { DEC(&regs.SP); }};
    instTbl[0x3c] = {"INC", "INC A", 1, {4}, [] () { INC(&regs.A); }};
    instTbl[0x3d] = {"DEC", "DEC A", 1, {4}, [] () { DEC(&regs.A); }};
    instTbl[0x3e] = {"LD", "LD A,d8", 2, {8}, [] () { LD(&regs.A,mmu.PeekByte(regs.PC+1)); }};
    instTbl[0x3f] = {"CCF", "CCF", 1, {4}, [] () { CCF(); }};
    instTbl[0x40] = {"LD", "LD B,B", 1, {4}, [] () { LD(&regs.B,regs.B); }};
    instTbl[0x41] = {"LD", "LD B,C", 1, {4}, [] () { LD(&regs.B,regs.C); }};
    instTbl[0x42] = {"LD", "LD B,D", 1, {4}, [] () { LD(&regs.B,regs.D); }};
    instTbl[0x43] = {"LD", "LD B,E", 1, {4}, [] () { LD(&regs.B,regs.E); }};
    instTbl[0x44] = {"LD", "LD B,H", 1, {4}, [] () { LD(&regs.B,regs.H); }};
    instTbl[0x45] = {"LD", "LD B,L", 1, {4}, [] () { LD(&regs.B,regs.L); }};
    instTbl[0x46] = {"LD", "LD B,(HL)", 1, {8}, [] () { LD(&regs.B,mmu.PeekByte(regs.HL)); }};
    instTbl[0x47] = {"LD", "LD B,A", 1, {4}, [] () { LD(&regs.B,regs.A); }};
    instTbl[0x48] = {"LD", "LD C,B", 1, {4}, [] () { LD(&regs.C,regs.B); }};
    instTbl[0x49] = {"LD", "LD C,C", 1, {4}, [] () { LD(&regs.C,regs.C); }};
    instTbl[0x4a] = {"LD", "LD C,D", 1, {4}, [] () { LD(&regs.C,regs.D); }};
    instTbl[0x4b] = {"LD", "LD C,E", 1, {4}, [] () { LD(&regs.C,regs.E); }};
    instTbl[0x4c] = {"LD", "LD C,H", 1, {4}, [] () { LD(&regs.C,regs.H); }};
    instTbl[0x4d] = {"LD", "LD C,L", 1, {4}, [] () { LD(&regs.C,regs.L); }};
    instTbl[0x4e] = {"LD", "LD C,(HL)", 1, {8}, [] () { LD(&regs.C,mmu.PeekByte(regs.HL)); }};
    instTbl[0x4f] = {"LD", "LD C,A", 1, {4}, [] () { LD(&regs.C,regs.A); }};
    instTbl[0x50] = {"LD", "LD D,B", 1, {4}, [] () { LD(&regs.D,regs.B); }};
    instTbl[0x51] = {"LD", "LD D,C", 1, {4}, [] () { LD(&regs.D,regs.C); }};
    instTbl[0x52] = {"LD", "LD D,D", 1, {4}, [] () { LD(&regs.D,regs.D); }};
    instTbl[0x53] = {"LD", "LD D,E", 1, {4}, [] () { LD(&regs.D,regs.E); }};
    instTbl[0x54] = {"LD", "LD D,H", 1, {4}, [] () { LD(&regs.D,regs.H); }};
    instTbl[0x55] = {"LD", "LD D,L", 1, {4}, [] () { LD(&regs.D,regs.L); }};
    instTbl[0x56] = {"LD", "LD D,(HL)", 1, {8}, [] () { LD(&regs.D,mmu.PeekByte(regs.HL)); }};
    instTbl[0x57] = {"LD", "LD D,A", 1, {4}, [] () { LD(&regs.D,regs.A); }};
    instTbl[0x58] = {"LD", "LD E,B", 1, {4}, [] () { LD(&regs.E,regs.B); }};
    instTbl[0x59] = {"LD", "LD E,C", 1, {4}, [] () { LD(&regs.E,regs.C); }};
    instTbl[0x5a] = {"LD", "LD E,D", 1, {4}, [] () { LD(&regs.E,regs.D); }};
    instTbl[0x5b] = {"LD", "LD E,E", 1, {4}, [] () { LD(&regs.E,regs.E); }};
    instTbl[0x5c] = {"LD", "LD E,H", 1, {4}, [] () { LD(&regs.E,regs.H); }};
    instTbl[0x5d] = {"LD", "LD E,L", 1, {4}, [] () { LD(&regs.E,regs.L); }};
    instTbl[0x5e] = {"LD", "LD E,(HL)", 1, {8}, [] () { LD(&regs.E,mmu.PeekByte(regs.HL)); }};
    instTbl[0x5f] = {"LD", "LD E,A", 1, {4}, [] () { LD(&regs.E,regs.A); }};
    instTbl[0x60] = {"LD", "LD H,B", 1, {4}, [] () { LD(&regs.H,regs.B); }};
    instTbl[0x61] = {"LD", "LD H,C", 1, {4}, [] () { LD(&regs.H,regs.C); }};
    instTbl[0x62] = {"LD", "LD H,D", 1, {4}, [] () { LD(&regs.H,regs.D); }};
    instTbl[0x63] = {"LD", "LD H,E", 1, {4}, [] () { LD(&regs.H,regs.E); }};
    instTbl[0x64] = {"LD", "LD H,H", 1, {4}, [] () { LD(&regs.H,regs.H); }};
    instTbl[0x65] = {"LD", "LD H,L", 1, {4}, [] () { LD(&regs.H,regs.L); }};
    instTbl[0x66] = {"LD", "LD H,(HL)", 1, {8}, [] () { LD(&regs.H,mmu.PeekByte(regs.HL)); }};
    instTbl[0x67] = {"LD", "LD H,A", 1, {4}, [] () { LD(&regs.H,regs.A); }};
    instTbl[0x68] = {"LD", "LD L,B", 1, {4}, [] () { LD(&regs.L,regs.B); }};
    instTbl[0x69] = {"LD", "LD L,C", 1, {4}, [] () { LD(&regs.L,regs.C); }};
    instTbl[0x6a] = {"LD", "LD L,D", 1, {4}, [] () { LD(&regs.L,regs.D); }};
    instTbl[0x6b] = {"LD", "LD L,E", 1, {4}, [] () { LD(&regs.L,regs.E); }};
    instTbl[0x6c] = {"LD", "LD L,H", 1, {4}, [] () { LD(&regs.L,regs.H); }};
    instTbl[0x6d] = {"LD", "LD L,L", 1, {4}, [] () { LD(&regs.L,regs.L); }};
    instTbl[0x6e] = {"LD", "LD L,(HL)", 1, {8}, [] () { LD(&regs.L,mmu.PeekByte(regs.HL)); }};
    instTbl[0x6f] = {"LD", "LD L,A", 1, {4}, [] () { LD(&regs.L,regs.A); }};
    instTbl[0x70] = {"LD", "LD (HL),B", 1, {8}, [] () { LD(regs.HL,regs.B); }};
    instTbl[0x71] = {"LD", "LD (HL),C", 1, {8}, [] () { LD(regs.HL,regs.C); }};
    instTbl[0x72] = {"LD", "LD (HL),D", 1, {8}, [] () { LD(regs.HL,regs.D); }};
    instTbl[0x73] = {"LD", "LD (HL),E", 1, {8}, [] () { LD(regs.HL,regs.E); }};
    instTbl[0x74] = {"LD", "LD (HL),H", 1, {8}, [] () { LD(regs.HL,regs.H); }};
    instTbl[0x75] = {"LD", "LD (HL),L", 1, {8}, [] () { LD(regs.HL,regs.L); }};
    instTbl[0x76] = {"HALT", "HALT", 1, {4}, [] () { HALT(); }};
    instTbl[0x77] = {"LD", "LD (HL),A", 1, {8}, [] () { LD(regs.HL,regs.A); }};
    instTbl[0x78] = {"LD", "LD A,B", 1, {4}, [] () { LD(&regs.A,regs.B); }};
    instTbl[0x79] = {"LD", "LD A,C", 1, {4}, [] () { LD(&regs.A,regs.C); }};
    instTbl[0x7a] = {"LD", "LD A,D", 1, {4}, [] () { LD(&regs.A,regs.D); }};
    instTbl[0x7b] = {"LD", "LD A,E", 1, {4}, [] () { LD(&regs.A,regs.E); }};
    instTbl[0x7c] = {"LD", "LD A,H", 1, {4}, [] () { LD(&regs.A,regs.H); }};
    instTbl[0x7d] = {"LD", "LD A,L", 1, {4}, [] () { LD(&regs.A,regs.L); }};
    instTbl[0x7e] = {"LD", "LD A,(HL)", 1, {8}, [] () { LD(&regs.A,mmu.PeekByte(regs.HL)); }};
    instTbl[0x7f] = {"LD", "LD A,A", 1, {4}, [] () { LD(&regs.A,regs.A); }};
    instTbl[0x80] = {"ADD", "ADD A,B", 1, {4}, [] () { ADD(&regs.A,regs.B); }};
    instTbl[0x81] = {"ADD", "ADD A,C", 1, {4}, [] () { ADD(&regs.A,regs.C); }};
    instTbl[0x82] = {"ADD", "ADD A,D", 1, {4}, [] () { ADD(&regs.A,regs.D); }};
    instTbl[0x83] = {"ADD", "ADD A,E", 1, {4}, [] () { ADD(&regs.A,regs.E); }};
    instTbl[0x84] = {"ADD", "ADD A,H", 1, {4}, [] () { ADD(&regs.A,regs.H); }};
    instTbl[0x85] = {"ADD", "ADD A,L", 1, {4}, [] () { ADD(&regs.A,regs.L); }};
    instTbl[0x86] = {"ADD", "ADD A,(HL)", 1, {8}, [] () { ADD(&regs.A,mmu.PeekByte(regs.HL)); }};
    instTbl[0x87] = {"ADD", "ADD A,A", 1, {4}, [] () { ADD(&regs.A,regs.A); }};
    instTbl[0x88] = {"ADC", "ADC A,B", 1, {4}, [] () { ADC(&regs.A,regs.B); }};
    instTbl[0x89] = {"ADC", "ADC A,C", 1, {4}, [] () { ADC(&regs.A,regs.C); }};
    instTbl[0x8a] = {"ADC", "ADC A,D", 1, {4}, [] () { ADC(&regs.A,regs.D); }};
    instTbl[0x8b] = {"ADC", "ADC A,E", 1, {4}, [] () { ADC(&regs.A,regs.E); }};
    instTbl[0x8c] = {"ADC", "ADC A,H", 1, {4}, [] () { ADC(&regs.A,regs.H); }};
    instTbl[0x8d] = {"ADC", "ADC A,L", 1, {4}, [] () { ADC(&regs.A,regs.L); }};
    instTbl[0x8e] = {"ADC", "ADC A,(HL)", 1, {8}, [] () { ADC(&regs.A,mmu.PeekByte(regs.HL)); }};
    instTbl[0x8f] = {"ADC", "ADC A,A", 1, {4}, [] () { ADC(&regs.A,regs.A); }};
    instTbl[0x90] = {"SUB", "SUB B", 1, {4}, [] () { SUB(&regs.B); }};
    instTbl[0x91] = {"SUB", "SUB C", 1, {4}, [] () { SUB(&regs.C); }};
    instTbl[0x92] = {"SUB", "SUB D", 1, {4}, [] () { SUB(&regs.D); }};
    instTbl[0x93] = {"SUB", "SUB E", 1, {4}, [] () { SUB(&regs.E); }};
    instTbl[0x94] = {"SUB", "SUB H", 1, {4}, [] () { SUB(&regs.H); }};
    instTbl[0x95] = {"SUB", "SUB L", 1, {4}, [] () { SUB(&regs.L); }};
    instTbl[0x96] = {"SUB", "SUB (HL)", 1, {8}, [] () { SUB(regs.HL); }};
    instTbl[0x97] = {"SUB", "SUB A", 1, {4}, [] () { SUB(&regs.A); }};
    instTbl[0x98] = {"SBC", "SBC A,B", 1, {4}, [] () { SBC(&regs.A,regs.B); }};
    instTbl[0x99] = {"SBC", "SBC A,C", 1, {4}, [] () { SBC(&regs.A,regs.C); }};
    instTbl[0x9a] = {"SBC", "SBC A,D", 1, {4}, [] () { SBC(&regs.A,regs.D); }};
    instTbl[0x9b] = {"SBC", "SBC A,E", 1, {4}, [] () { SBC(&regs.A,regs.E); }};
    instTbl[0x9c] = {"SBC", "SBC A,H", 1, {4}, [] () { SBC(&regs.A,regs.H); }};
    instTbl[0x9d] = {"SBC", "SBC A,L", 1, {4}, [] () { SBC(&regs.A,regs.L); }};
    instTbl[0x9e] = {"SBC", "SBC A,(HL)", 1, {8}, [] () { SBC(&regs.A,mmu.PeekByte(regs.HL)); }};
    instTbl[0x9f] = {"SBC", "SBC A,A", 1, {4}, [] () { SBC(&regs.A,regs.A); }};
    instTbl[0xa0] = {"AND", "AND B", 1, {4}, [] () { AND(&regs.B); }};
    instTbl[0xa1] = {"AND", "AND C", 1, {4}, [] () { AND(&regs.C); }};
    instTbl[0xa2] = {"AND", "AND D", 1, {4}, [] () { AND(&regs.D); }};
    instTbl[0xa3] = {"AND", "AND E", 1, {4}, [] () { AND(&regs.E); }};
    instTbl[0xa4] = {"AND", "AND H", 1, {4}, [] () { AND(&regs.H); }};
    instTbl[0xa5] = {"AND", "AND L", 1, {4}, [] () { AND(&regs.L); }};
    instTbl[0xa6] = {"AND", "AND (HL)", 1, {8}, [] () { AND(regs.HL); }};
    instTbl[0xa7] = {"AND", "AND A", 1, {4}, [] () { AND(&regs.A); }};
    instTbl[0xa8] = {"XOR", "XOR B", 1, {4}, [] () { XOR(&regs.B); }};
    instTbl[0xa9] = {"XOR", "XOR C", 1, {4}, [] () { XOR(&regs.C); }};
    instTbl[0xaa] = {"XOR", "XOR D", 1, {4}, [] () { XOR(&regs.D); }};
    instTbl[0xab] = {"XOR", "XOR E", 1, {4}, [] () { XOR(&regs.E); }};
    instTbl[0xac] = {"XOR", "XOR H", 1, {4}, [] () { XOR(&regs.H); }};
    instTbl[0xad] = {"XOR", "XOR L", 1, {4}, [] () { XOR(&regs.L); }};
    instTbl[0xae] = {"XOR", "XOR (HL)", 1, {8}, [] () { XOR(regs.HL); }};
    instTbl[0xaf] = {"XOR", "XOR A", 1, {4}, [] () { XOR(&regs.A); }};
    instTbl[0xb0] = {"OR", "OR B", 1, {4}, [] () { OR(&regs.B); }};
    instTbl[0xb1] = {"OR", "OR C", 1, {4}, [] () { OR(&regs.C); }};
    instTbl[0xb2] = {"OR", "OR D", 1, {4}, [] () { OR(&regs.D); }};
    instTbl[0xb3] = {"OR", "OR E", 1, {4}, [] () { OR(&regs.E); }};
    instTbl[0xb4] = {"OR", "OR H", 1, {4}, [] () { OR(&regs.H); }};
    instTbl[0xb5] = {"OR", "OR L", 1, {4}, [] () { OR(&regs.L); }};
    instTbl[0xb6] = {"OR", "OR (HL)", 1, {8}, [] () { OR(regs.HL); }};
    instTbl[0xb7] = {"OR", "OR A", 1, {4}, [] () { OR(&regs.A); }};
    instTbl[0xb8] = {"CP", "CP B", 1, {4}, [] () { CP(&regs.B); }};
    instTbl[0xb9] = {"CP", "CP C", 1, {4}, [] () { CP(&regs.C); }};
    instTbl[0xba] = {"CP", "CP D", 1, {4}, [] () { CP(&regs.D); }};
    instTbl[0xbb] = {"CP", "CP E", 1, {4}, [] () { CP(&regs.E); }};
    instTbl[0xbc] = {"CP", "CP H", 1, {4}, [] () { CP(&regs.H); }};
    instTbl[0xbd] = {"CP", "CP L", 1, {4}, [] () { CP(&regs.L); }};
    instTbl[0xbe] = {"CP", "CP (HL)", 1, {8}, [] () { CP(regs.HL); }};
    instTbl[0xbf] = {"CP", "CP A", 1, {4}, [] () { CP(&regs.A); }};
    instTbl[0xc0] = {"RET", "RET NZ", 1, {20,8}, [] () { RET(NZ); }};
    instTbl[0xc1] = {"POP", "POP BC", 1, {12}, [] () { POP(&regs.BC); }};
    instTbl[0xc2] = {"JP", "JP NZ,a16", 3, {16,12}, [] () { JP(NZ,mmu.PeekWord(regs.PC+1)); }};
    instTbl[0xc3] = {"JP", "JP a16", 3, {16}, [] () { JP(mmu.PeekWord(regs.PC+1)); }};
    instTbl[0xc4] = {"CALL", "CALL NZ,a16", 3, {24,12}, [] () { CALL(NZ,mmu.PeekWord(regs.PC+1)); }};
    instTbl[0xc5] = {"PUSH", "PUSH BC", 1, {16}, [] () { PUSH(&regs.BC); }};
    instTbl[0xc6] = {"ADD", "ADD A,d8", 2, {8}, [] () { ADD(&regs.A,mmu.PeekByte(regs.PC+1)); }};
    instTbl[0xc7] = {"RST", "RST 00H", 1, {16}, [] () { RST(0x00); }};
    instTbl[0xc8] = {"RET", "RET Z", 1, {20,8}, [] () { RET(Z); }};
    instTbl[0xc9] = {"RET", "RET", 1, {16}, [] () { RET(); }};
    instTbl[0xca] = {"JP", "JP Z,a16", 3, {16,12}, [] () { JP(Z,mmu.PeekWord(regs.PC+1)); }};
    instTbl[0xcb] = {"PREFIX", "PREFIX CB", 1, {0}, [] () { PREFIX(mmu.PeekByte(regs.PC+1)); }}; // manually modified
    instTbl[0xcc] = {"CALL", "CALL Z,a16", 3, {24,12}, [] () { CALL(Z,mmu.PeekWord(regs.PC+1)); }};
    instTbl[0xcd] = {"CALL", "CALL a16", 3, {24}, [] () { CALL(mmu.PeekWord(regs.PC+1)); }};
    instTbl[0xce] = {"ADC", "ADC A,d8", 2, {8}, [] () { ADC(&regs.A,mmu.PeekByte(regs.PC+1)); }};
    instTbl[0xcf] = {"RST", "RST 08H", 1, {16}, [] () { RST(0x08); }};
    instTbl[0xd0] = {"RET", "RET NC", 1, {20,8}, [] () { RET(NC); }};
    instTbl[0xd1] = {"POP", "POP DE", 1, {12}, [] () { POP(&regs.DE); }};
    instTbl[0xd2] = {"JP", "JP NC,a16", 3, {16,12}, [] () { JP(NC,mmu.PeekWord(regs.PC+1)); }};
    instTbl[0xd4] = {"CALL", "CALL NC,a16", 3, {24,12}, [] () { CALL(NC,mmu.PeekWord(regs.PC+1)); }};
    instTbl[0xd5] = {"PUSH", "PUSH DE", 1, {16}, [] () { PUSH(&regs.DE); }};
    instTbl[0xd6] = {"SUB", "SUB d8", 2, {8}, [] () { SUB(mmu.PeekByte(regs.PC+1)); }};
    instTbl[0xd7] = {"RST", "RST 10H", 1, {16}, [] () { RST(0x10); }};
    instTbl[0xd8] = {"RET", "RET C", 1, {20,8}, [] () { RET(C); }};
    instTbl[0xd9] = {"RETI", "RETI", 1, {16}, [] () { RETI(); }};
    instTbl[0xda] = {"JP", "JP C,a16", 3, {16,12}, [] () { JP(C,mmu.PeekWord(regs.PC+1)); }};
    instTbl[0xdc] = {"CALL", "CALL C,a16", 3, {24,12}, [] () { CALL(C,mmu.PeekWord(regs.PC+1)); }};
    instTbl[0xde] = {"SBC", "SBC A,d8", 2, {8}, [] () { SBC(&regs.A,mmu.PeekByte(regs.PC+1)); }};
    instTbl[0xdf] = {"RST", "RST 18H", 1, {16}, [] () { RST(0x18); }};
    instTbl[0xe0] = {"LDH", "LDH (a8),A", 2, {12}, [] () { LDH(0xFF00+mmu.PeekByte(regs.PC+1),regs.A); }};
    instTbl[0xe1] = {"POP", "POP HL", 1, {12}, [] () { POP(&regs.HL); }};
    instTbl[0xe2] = {"LD", "LD (C),A", 1, {8}, [] () { LD(0xFF00+regs.C,regs.A); }};
    instTbl[0xe5] = {"PUSH", "PUSH HL", 1, {16}, [] () { PUSH(&regs.HL); }};
    instTbl[0xe6] = {"AND", "AND d8", 2, {8}, [] () { AND(mmu.PeekByte(regs.PC+1)); }};
    instTbl[0xe7] = {"RST", "RST 20H", 1, {16}, [] () { RST(0x20); }};
    instTbl[0xe8] = {"ADD", "ADD SP,r8", 2, {16}, [] () { ADD_HL(&regs.SP,(sbyte)mmu.PeekByte(regs.PC+1)); }};
    instTbl[0xe9] = {"JP", "JP (HL)", 1, {4}, [] () { JP(regs.HL); }}; // Manually modified (get HL direct instead of mem)
    instTbl[0xea] = {"LD", "LD (a16),A", 3, {16}, [] () { LD(mmu.PeekWord(regs.PC+1),regs.A); }};
    instTbl[0xee] = {"XOR", "XOR d8", 2, {8}, [] () { XOR(mmu.PeekByte(regs.PC+1)); }};
    instTbl[0xef] = {"RST", "RST 28H", 1, {16}, [] () { RST(0x28); }};
    instTbl[0xf0] = {"LDH", "LDH A,(a8)", 2, {12}, [] () { LDH(&regs.A,mmu.PeekByte(0xFF00+mmu.PeekByte(regs.PC+1))); }};
    instTbl[0xf1] = {"POP", "POP AF", 1, {12}, [] () { POPAF(&regs.AF); }};
    instTbl[0xf2] = {"LD", "LD A,(C)", 1, {8}, [] () { LD(&regs.A,mmu.PeekByte(0xFF00+regs.C)); }};
    instTbl[0xf3] = {"DI", "DI", 1, {4}, [] () { DI(); }};
    instTbl[0xf5] = {"PUSH", "PUSH AF", 1, {16}, [] () { PUSH(&regs.AF); }};
    instTbl[0xf6] = {"OR", "OR d8", 2, {8}, [] () { OR(mmu.PeekByte(regs.PC+1)); }};
    instTbl[0xf7] = {"RST", "RST 30H", 1, {16}, [] () { RST(0x30); }};
    instTbl[0xf8] = {"LD", "LD HL,SP+r8", 2, {12}, [] () { LDHL(); }}; // Manually modified (new LDHL func)
    instTbl[0xf9] = {"LD", "LD SP,HL", 1, {8}, [] () { LD(&regs.SP,regs.HL); }};
    instTbl[0xfa] = {"LD", "LD A,(a16)", 3, {16}, [] () { LD(&regs.A,mmu.PeekByte(mmu.PeekWord(regs.PC+1))); }};
    instTbl[0xfb] = {"EI", "EI", 1, {4}, [] () { EI(); }};
    instTbl[0xfe] = {"CP", "CP d8", 2, {8}, [] () { CP(mmu.PeekByte(regs.PC+1)); }};
    instTbl[0xff] = {"RST", "RST 38H", 1, {16}, [] () { RST(0x38); }};

    // CB prefixed instructions
    instTblX[0x00] = {"RLC", "RLC B", 2, {8}, [] () { RLC(&regs.B); }};
    instTblX[0x01] = {"RLC", "RLC C", 2, {8}, [] () { RLC(&regs.C); }};
    instTblX[0x02] = {"RLC", "RLC D", 2, {8}, [] () { RLC(&regs.D); }};
    instTblX[0x03] = {"RLC", "RLC E", 2, {8}, [] () { RLC(&regs.E); }};
    instTblX[0x04] = {"RLC", "RLC H", 2, {8}, [] () { RLC(&regs.H); }};
    instTblX[0x05] = {"RLC", "RLC L", 2, {8}, [] () { RLC(&regs.L); }};
    instTblX[0x06] = {"RLC", "RLC (HL)", 2, {16}, [] () { RLC(regs.HL); }};
    instTblX[0x07] = {"RLC", "RLC A", 2, {8}, [] () { RLC(&regs.A); }};
    instTblX[0x08] = {"RRC", "RRC B", 2, {8}, [] () { RRC(&regs.B); }};
    instTblX[0x09] = {"RRC", "RRC C", 2, {8}, [] () { RRC(&regs.C); }};
    instTblX[0x0a] = {"RRC", "RRC D", 2, {8}, [] () { RRC(&regs.D); }};
    instTblX[0x0b] = {"RRC", "RRC E", 2, {8}, [] () { RRC(&regs.E); }};
    instTblX[0x0c] = {"RRC", "RRC H", 2, {8}, [] () { RRC(&regs.H); }};
    instTblX[0x0d] = {"RRC", "RRC L", 2, {8}, [] () { RRC(&regs.L); }};
    instTblX[0x0e] = {"RRC", "RRC (HL)", 2, {16}, [] () { RRC(regs.HL); }};
    instTblX[0x0f] = {"RRC", "RRC A", 2, {8}, [] () { RRC(&regs.A); }};
    instTblX[0x10] = {"RL", "RL B", 2, {8}, [] () { RL(&regs.B); }};
    instTblX[0x11] = {"RL", "RL C", 2, {8}, [] () { RL(&regs.C); }};
    instTblX[0x12] = {"RL", "RL D", 2, {8}, [] () { RL(&regs.D); }};
    instTblX[0x13] = {"RL", "RL E", 2, {8}, [] () { RL(&regs.E); }};
    instTblX[0x14] = {"RL", "RL H", 2, {8}, [] () { RL(&regs.H); }};
    instTblX[0x15] = {"RL", "RL L", 2, {8}, [] () { RL(&regs.L); }};
    instTblX[0x16] = {"RL", "RL (HL)", 2, {16}, [] () { RL(regs.HL); }};
    instTblX[0x17] = {"RL", "RL A", 2, {8}, [] () { RL(&regs.A); }};
    instTblX[0x18] = {"RR", "RR B", 2, {8}, [] () { RR(&regs.B); }};
    instTblX[0x19] = {"RR", "RR C", 2, {8}, [] () { RR(&regs.C); }};
    instTblX[0x1a] = {"RR", "RR D", 2, {8}, [] () { RR(&regs.D); }};
    instTblX[0x1b] = {"RR", "RR E", 2, {8}, [] () { RR(&regs.E); }};
    instTblX[0x1c] = {"RR", "RR H", 2, {8}, [] () { RR(&regs.H); }};
    instTblX[0x1d] = {"RR", "RR L", 2, {8}, [] () { RR(&regs.L); }};
    instTblX[0x1e] = {"RR", "RR (HL)", 2, {16}, [] () { RR(regs.HL); }};
    instTblX[0x1f] = {"RR", "RR A", 2, {8}, [] () { RR(&regs.A); }};
    instTblX[0x20] = {"SLA", "SLA B", 2, {8}, [] () { SLA(&regs.B); }};
    instTblX[0x21] = {"SLA", "SLA C", 2, {8}, [] () { SLA(&regs.C); }};
    instTblX[0x22] = {"SLA", "SLA D", 2, {8}, [] () { SLA(&regs.D); }};
    instTblX[0x23] = {"SLA", "SLA E", 2, {8}, [] () { SLA(&regs.E); }};
    instTblX[0x24] = {"SLA", "SLA H", 2, {8}, [] () { SLA(&regs.H); }};
    instTblX[0x25] = {"SLA", "SLA L", 2, {8}, [] () { SLA(&regs.L); }};
    instTblX[0x26] = {"SLA", "SLA (HL)", 2, {16}, [] () { SLA(regs.HL); }};
    instTblX[0x27] = {"SLA", "SLA A", 2, {8}, [] () { SLA(&regs.A); }};
    instTblX[0x28] = {"SRA", "SRA B", 2, {8}, [] () { SRA(&regs.B); }};
    instTblX[0x29] = {"SRA", "SRA C", 2, {8}, [] () { SRA(&regs.C); }};
    instTblX[0x2a] = {"SRA", "SRA D", 2, {8}, [] () { SRA(&regs.D); }};
    instTblX[0x2b] = {"SRA", "SRA E", 2, {8}, [] () { SRA(&regs.E); }};
    instTblX[0x2c] = {"SRA", "SRA H", 2, {8}, [] () { SRA(&regs.H); }};
    instTblX[0x2d] = {"SRA", "SRA L", 2, {8}, [] () { SRA(&regs.L); }};
    instTblX[0x2e] = {"SRA", "SRA (HL)", 2, {16}, [] () { SRA(regs.HL); }};
    instTblX[0x2f] = {"SRA", "SRA A", 2, {8}, [] () { SRA(&regs.A); }};
    instTblX[0x30] = {"SWAP", "SWAP B", 2, {8}, [] () { SWAP(&regs.B); }};
    instTblX[0x31] = {"SWAP", "SWAP C", 2, {8}, [] () { SWAP(&regs.C); }};
    instTblX[0x32] = {"SWAP", "SWAP D", 2, {8}, [] () { SWAP(&regs.D); }};
    instTblX[0x33] = {"SWAP", "SWAP E", 2, {8}, [] () { SWAP(&regs.E); }};
    instTblX[0x34] = {"SWAP", "SWAP H", 2, {8}, [] () { SWAP(&regs.H); }};
    instTblX[0x35] = {"SWAP", "SWAP L", 2, {8}, [] () { SWAP(&regs.L); }};
    instTblX[0x36] = {"SWAP", "SWAP (HL)", 2, {16}, [] () { SWAP(regs.HL); }};
    instTblX[0x37] = {"SWAP", "SWAP A", 2, {8}, [] () { SWAP(&regs.A); }};
    instTblX[0x38] = {"SRL", "SRL B", 2, {8}, [] () { SRL(&regs.B); }};
    instTblX[0x39] = {"SRL", "SRL C", 2, {8}, [] () { SRL(&regs.C); }};
    instTblX[0x3a] = {"SRL", "SRL D", 2, {8}, [] () { SRL(&regs.D); }};
    instTblX[0x3b] = {"SRL", "SRL E", 2, {8}, [] () { SRL(&regs.E); }};
    instTblX[0x3c] = {"SRL", "SRL H", 2, {8}, [] () { SRL(&regs.H); }};
    instTblX[0x3d] = {"SRL", "SRL L", 2, {8}, [] () { SRL(&regs.L); }};
    instTblX[0x3e] = {"SRL", "SRL (HL)", 2, {16}, [] () { SRL(regs.HL); }};
    instTblX[0x3f] = {"SRL", "SRL A", 2, {8}, [] () { SRL(&regs.A); }};
    instTblX[0x40] = {"BIT", "BIT 0,B", 2, {8}, [] () { BIT(0x0,&regs.B); }};
    instTblX[0x41] = {"BIT", "BIT 0,C", 2, {8}, [] () { BIT(0x0,&regs.C); }};
    instTblX[0x42] = {"BIT", "BIT 0,D", 2, {8}, [] () { BIT(0x0,&regs.D); }};
    instTblX[0x43] = {"BIT", "BIT 0,E", 2, {8}, [] () { BIT(0x0,&regs.E); }};
    instTblX[0x44] = {"BIT", "BIT 0,H", 2, {8}, [] () { BIT(0x0,&regs.H); }};
    instTblX[0x45] = {"BIT", "BIT 0,L", 2, {8}, [] () { BIT(0x0,&regs.L); }};
    instTblX[0x46] = {"BIT", "BIT 0,(HL)", 2, {12}, [] () { BIT(0x0,regs.HL); }};
    instTblX[0x47] = {"BIT", "BIT 0,A", 2, {8}, [] () { BIT(0x0,&regs.A); }};
    instTblX[0x48] = {"BIT", "BIT 1,B", 2, {8}, [] () { BIT(0x1,&regs.B); }};
    instTblX[0x49] = {"BIT", "BIT 1,C", 2, {8}, [] () { BIT(0x1,&regs.C); }};
    instTblX[0x4a] = {"BIT", "BIT 1,D", 2, {8}, [] () { BIT(0x1,&regs.D); }};
    instTblX[0x4b] = {"BIT", "BIT 1,E", 2, {8}, [] () { BIT(0x1,&regs.E); }};
    instTblX[0x4c] = {"BIT", "BIT 1,H", 2, {8}, [] () { BIT(0x1,&regs.H); }};
    instTblX[0x4d] = {"BIT", "BIT 1,L", 2, {8}, [] () { BIT(0x1,&regs.L); }};
    instTblX[0x4e] = {"BIT", "BIT 1,(HL)", 2, {12}, [] () { BIT(0x1,regs.HL); }};
    instTblX[0x4f] = {"BIT", "BIT 1,A", 2, {8}, [] () { BIT(0x1,&regs.A); }};
    instTblX[0x50] = {"BIT", "BIT 2,B", 2, {8}, [] () { BIT(0x2,&regs.B); }};
    instTblX[0x51] = {"BIT", "BIT 2,C", 2, {8}, [] () { BIT(0x2,&regs.C); }};
    instTblX[0x52] = {"BIT", "BIT 2,D", 2, {8}, [] () { BIT(0x2,&regs.D); }};
    instTblX[0x53] = {"BIT", "BIT 2,E", 2, {8}, [] () { BIT(0x2,&regs.E); }};
    instTblX[0x54] = {"BIT", "BIT 2,H", 2, {8}, [] () { BIT(0x2,&regs.H); }};
    instTblX[0x55] = {"BIT", "BIT 2,L", 2, {8}, [] () { BIT(0x2,&regs.L); }};
    instTblX[0x56] = {"BIT", "BIT 2,(HL)", 2, {12}, [] () { BIT(0x2,regs.HL); }};
    instTblX[0x57] = {"BIT", "BIT 2,A", 2, {8}, [] () { BIT(0x2,&regs.A); }};
    instTblX[0x58] = {"BIT", "BIT 3,B", 2, {8}, [] () { BIT(0x3,&regs.B); }};
    instTblX[0x59] = {"BIT", "BIT 3,C", 2, {8}, [] () { BIT(0x3,&regs.C); }};
    instTblX[0x5a] = {"BIT", "BIT 3,D", 2, {8}, [] () { BIT(0x3,&regs.D); }};
    instTblX[0x5b] = {"BIT", "BIT 3,E", 2, {8}, [] () { BIT(0x3,&regs.E); }};
    instTblX[0x5c] = {"BIT", "BIT 3,H", 2, {8}, [] () { BIT(0x3,&regs.H); }};
    instTblX[0x5d] = {"BIT", "BIT 3,L", 2, {8}, [] () { BIT(0x3,&regs.L); }};
    instTblX[0x5e] = {"BIT", "BIT 3,(HL)", 2, {12}, [] () { BIT(0x3,regs.HL); }};
    instTblX[0x5f] = {"BIT", "BIT 3,A", 2, {8}, [] () { BIT(0x3,&regs.A); }};
    instTblX[0x60] = {"BIT", "BIT 4,B", 2, {8}, [] () { BIT(0x4,&regs.B); }};
    instTblX[0x61] = {"BIT", "BIT 4,C", 2, {8}, [] () { BIT(0x4,&regs.C); }};
    instTblX[0x62] = {"BIT", "BIT 4,D", 2, {8}, [] () { BIT(0x4,&regs.D); }};
    instTblX[0x63] = {"BIT", "BIT 4,E", 2, {8}, [] () { BIT(0x4,&regs.E); }};
    instTblX[0x64] = {"BIT", "BIT 4,H", 2, {8}, [] () { BIT(0x4,&regs.H); }};
    instTblX[0x65] = {"BIT", "BIT 4,L", 2, {8}, [] () { BIT(0x4,&regs.L); }};
    instTblX[0x66] = {"BIT", "BIT 4,(HL)", 2, {12}, [] () { BIT(0x4,regs.HL); }};
    instTblX[0x67] = {"BIT", "BIT 4,A", 2, {8}, [] () { BIT(0x4,&regs.A); }};
    instTblX[0x68] = {"BIT", "BIT 5,B", 2, {8}, [] () { BIT(0x5,&regs.B); }};
    instTblX[0x69] = {"BIT", "BIT 5,C", 2, {8}, [] () { BIT(0x5,&regs.C); }};
    instTblX[0x6a] = {"BIT", "BIT 5,D", 2, {8}, [] () { BIT(0x5,&regs.D); }};
    instTblX[0x6b] = {"BIT", "BIT 5,E", 2, {8}, [] () { BIT(0x5,&regs.E); }};
    instTblX[0x6c] = {"BIT", "BIT 5,H", 2, {8}, [] () { BIT(0x5,&regs.H); }};
    instTblX[0x6d] = {"BIT", "BIT 5,L", 2, {8}, [] () { BIT(0x5,&regs.L); }};
    instTblX[0x6e] = {"BIT", "BIT 5,(HL)", 2, {12}, [] () { BIT(0x5,regs.HL); }};
    instTblX[0x6f] = {"BIT", "BIT 5,A", 2, {8}, [] () { BIT(0x5,&regs.A); }};
    instTblX[0x70] = {"BIT", "BIT 6,B", 2, {8}, [] () { BIT(0x6,&regs.B); }};
    instTblX[0x71] = {"BIT", "BIT 6,C", 2, {8}, [] () { BIT(0x6,&regs.C); }};
    instTblX[0x72] = {"BIT", "BIT 6,D", 2, {8}, [] () { BIT(0x6,&regs.D); }};
    instTblX[0x73] = {"BIT", "BIT 6,E", 2, {8}, [] () { BIT(0x6,&regs.E); }};
    instTblX[0x74] = {"BIT", "BIT 6,H", 2, {8}, [] () { BIT(0x6,&regs.H); }};
    instTblX[0x75] = {"BIT", "BIT 6,L", 2, {8}, [] () { BIT(0x6,&regs.L); }};
    instTblX[0x76] = {"BIT", "BIT 6,(HL)", 2, {12}, [] () { BIT(0x6,regs.HL); }};
    instTblX[0x77] = {"BIT", "BIT 6,A", 2, {8}, [] () { BIT(0x6,&regs.A); }};
    instTblX[0x78] = {"BIT", "BIT 7,B", 2, {8}, [] () { BIT(0x7,&regs.B); }};
    instTblX[0x79] = {"BIT", "BIT 7,C", 2, {8}, [] () { BIT(0x7,&regs.C); }};
    instTblX[0x7a] = {"BIT", "BIT 7,D", 2, {8}, [] () { BIT(0x7,&regs.D); }};
    instTblX[0x7b] = {"BIT", "BIT 7,E", 2, {8}, [] () { BIT(0x7,&regs.E); }};
    instTblX[0x7c] = {"BIT", "BIT 7,H", 2, {8}, [] () { BIT(0x7,&regs.H); }};
    instTblX[0x7d] = {"BIT", "BIT 7,L", 2, {8}, [] () { BIT(0x7,&regs.L); }};
    instTblX[0x7e] = {"BIT", "BIT 7,(HL)", 2, {12}, [] () { BIT(0x7,regs.HL); }};
    instTblX[0x7f] = {"BIT", "BIT 7,A", 2, {8}, [] () { BIT(0x7,&regs.A); }};
    instTblX[0x80] = {"RES", "RES 0,B", 2, {8}, [] () { RES(0x0,&regs.B); }};
    instTblX[0x81] = {"RES", "RES 0,C", 2, {8}, [] () { RES(0x0,&regs.C); }};
    instTblX[0x82] = {"RES", "RES 0,D", 2, {8}, [] () { RES(0x0,&regs.D); }};
    instTblX[0x83] = {"RES", "RES 0,E", 2, {8}, [] () { RES(0x0,&regs.E); }};
    instTblX[0x84] = {"RES", "RES 0,H", 2, {8}, [] () { RES(0x0,&regs.H); }};
    instTblX[0x85] = {"RES", "RES 0,L", 2, {8}, [] () { RES(0x0,&regs.L); }};
    instTblX[0x86] = {"RES", "RES 0,(HL)", 2, {16}, [] () { RES(0x0,regs.HL); }};
    instTblX[0x87] = {"RES", "RES 0,A", 2, {8}, [] () { RES(0x0,&regs.A); }};
    instTblX[0x88] = {"RES", "RES 1,B", 2, {8}, [] () { RES(0x1,&regs.B); }};
    instTblX[0x89] = {"RES", "RES 1,C", 2, {8}, [] () { RES(0x1,&regs.C); }};
    instTblX[0x8a] = {"RES", "RES 1,D", 2, {8}, [] () { RES(0x1,&regs.D); }};
    instTblX[0x8b] = {"RES", "RES 1,E", 2, {8}, [] () { RES(0x1,&regs.E); }};
    instTblX[0x8c] = {"RES", "RES 1,H", 2, {8}, [] () { RES(0x1,&regs.H); }};
    instTblX[0x8d] = {"RES", "RES 1,L", 2, {8}, [] () { RES(0x1,&regs.L); }};
    instTblX[0x8e] = {"RES", "RES 1,(HL)", 2, {16}, [] () { RES(0x1,regs.HL); }};
    instTblX[0x8f] = {"RES", "RES 1,A", 2, {8}, [] () { RES(0x1,&regs.A); }};
    instTblX[0x90] = {"RES", "RES 2,B", 2, {8}, [] () { RES(0x2,&regs.B); }};
    instTblX[0x91] = {"RES", "RES 2,C", 2, {8}, [] () { RES(0x2,&regs.C); }};
    instTblX[0x92] = {"RES", "RES 2,D", 2, {8}, [] () { RES(0x2,&regs.D); }};
    instTblX[0x93] = {"RES", "RES 2,E", 2, {8}, [] () { RES(0x2,&regs.E); }};
    instTblX[0x94] = {"RES", "RES 2,H", 2, {8}, [] () { RES(0x2,&regs.H); }};
    instTblX[0x95] = {"RES", "RES 2,L", 2, {8}, [] () { RES(0x2,&regs.L); }};
    instTblX[0x96] = {"RES", "RES 2,(HL)", 2, {16}, [] () { RES(0x2,regs.HL); }};
    instTblX[0x97] = {"RES", "RES 2,A", 2, {8}, [] () { RES(0x2,&regs.A); }};
    instTblX[0x98] = {"RES", "RES 3,B", 2, {8}, [] () { RES(0x3,&regs.B); }};
    instTblX[0x99] = {"RES", "RES 3,C", 2, {8}, [] () { RES(0x3,&regs.C); }};
    instTblX[0x9a] = {"RES", "RES 3,D", 2, {8}, [] () { RES(0x3,&regs.D); }};
    instTblX[0x9b] = {"RES", "RES 3,E", 2, {8}, [] () { RES(0x3,&regs.E); }};
    instTblX[0x9c] = {"RES", "RES 3,H", 2, {8}, [] () { RES(0x3,&regs.H); }};
    instTblX[0x9d] = {"RES", "RES 3,L", 2, {8}, [] () { RES(0x3,&regs.L); }};
    instTblX[0x9e] = {"RES", "RES 3,(HL)", 2, {16}, [] () { RES(0x3,regs.HL); }};
    instTblX[0x9f] = {"RES", "RES 3,A", 2, {8}, [] () { RES(0x3,&regs.A); }};
    instTblX[0xa0] = {"RES", "RES 4,B", 2, {8}, [] () { RES(0x4,&regs.B); }};
    instTblX[0xa1] = {"RES", "RES 4,C", 2, {8}, [] () { RES(0x4,&regs.C); }};
    instTblX[0xa2] = {"RES", "RES 4,D", 2, {8}, [] () { RES(0x4,&regs.D); }};
    instTblX[0xa3] = {"RES", "RES 4,E", 2, {8}, [] () { RES(0x4,&regs.E); }};
    instTblX[0xa4] = {"RES", "RES 4,H", 2, {8}, [] () { RES(0x4,&regs.H); }};
    instTblX[0xa5] = {"RES", "RES 4,L", 2, {8}, [] () { RES(0x4,&regs.L); }};
    instTblX[0xa6] = {"RES", "RES 4,(HL)", 2, {16}, [] () { RES(0x4,regs.HL); }};
    instTblX[0xa7] = {"RES", "RES 4,A", 2, {8}, [] () { RES(0x4,&regs.A); }};
    instTblX[0xa8] = {"RES", "RES 5,B", 2, {8}, [] () { RES(0x5,&regs.B); }};
    instTblX[0xa9] = {"RES", "RES 5,C", 2, {8}, [] () { RES(0x5,&regs.C); }};
    instTblX[0xaa] = {"RES", "RES 5,D", 2, {8}, [] () { RES(0x5,&regs.D); }};
    instTblX[0xab] = {"RES", "RES 5,E", 2, {8}, [] () { RES(0x5,&regs.E); }};
    instTblX[0xac] = {"RES", "RES 5,H", 2, {8}, [] () { RES(0x5,&regs.H); }};
    instTblX[0xad] = {"RES", "RES 5,L", 2, {8}, [] () { RES(0x5,&regs.L); }};
    instTblX[0xae] = {"RES", "RES 5,(HL)", 2, {16}, [] () { RES(0x5,regs.HL); }};
    instTblX[0xaf] = {"RES", "RES 5,A", 2, {8}, [] () { RES(0x5,&regs.A); }};
    instTblX[0xb0] = {"RES", "RES 6,B", 2, {8}, [] () { RES(0x6,&regs.B); }};
    instTblX[0xb1] = {"RES", "RES 6,C", 2, {8}, [] () { RES(0x6,&regs.C); }};
    instTblX[0xb2] = {"RES", "RES 6,D", 2, {8}, [] () { RES(0x6,&regs.D); }};
    instTblX[0xb3] = {"RES", "RES 6,E", 2, {8}, [] () { RES(0x6,&regs.E); }};
    instTblX[0xb4] = {"RES", "RES 6,H", 2, {8}, [] () { RES(0x6,&regs.H); }};
    instTblX[0xb5] = {"RES", "RES 6,L", 2, {8}, [] () { RES(0x6,&regs.L); }};
    instTblX[0xb6] = {"RES", "RES 6,(HL)", 2, {16}, [] () { RES(0x6,regs.HL); }};
    instTblX[0xb7] = {"RES", "RES 6,A", 2, {8}, [] () { RES(0x6,&regs.A); }};
    instTblX[0xb8] = {"RES", "RES 7,B", 2, {8}, [] () { RES(0x7,&regs.B); }};
    instTblX[0xb9] = {"RES", "RES 7,C", 2, {8}, [] () { RES(0x7,&regs.C); }};
    instTblX[0xba] = {"RES", "RES 7,D", 2, {8}, [] () { RES(0x7,&regs.D); }};
    instTblX[0xbb] = {"RES", "RES 7,E", 2, {8}, [] () { RES(0x7,&regs.E); }};
    instTblX[0xbc] = {"RES", "RES 7,H", 2, {8}, [] () { RES(0x7,&regs.H); }};
    instTblX[0xbd] = {"RES", "RES 7,L", 2, {8}, [] () { RES(0x7,&regs.L); }};
    instTblX[0xbe] = {"RES", "RES 7,(HL)", 2, {16}, [] () { RES(0x7,regs.HL); }};
    instTblX[0xbf] = {"RES", "RES 7,A", 2, {8}, [] () { RES(0x7,&regs.A); }};
    instTblX[0xc0] = {"SET", "SET 0,B", 2, {8}, [] () { SET(0x0,&regs.B); }};
    instTblX[0xc1] = {"SET", "SET 0,C", 2, {8}, [] () { SET(0x0,&regs.C); }};
    instTblX[0xc2] = {"SET", "SET 0,D", 2, {8}, [] () { SET(0x0,&regs.D); }};
    instTblX[0xc3] = {"SET", "SET 0,E", 2, {8}, [] () { SET(0x0,&regs.E); }};
    instTblX[0xc4] = {"SET", "SET 0,H", 2, {8}, [] () { SET(0x0,&regs.H); }};
    instTblX[0xc5] = {"SET", "SET 0,L", 2, {8}, [] () { SET(0x0,&regs.L); }};
    instTblX[0xc6] = {"SET", "SET 0,(HL)", 2, {16}, [] () { SET(0x0,regs.HL); }};
    instTblX[0xc7] = {"SET", "SET 0,A", 2, {8}, [] () { SET(0x0,&regs.A); }};
    instTblX[0xc8] = {"SET", "SET 1,B", 2, {8}, [] () { SET(0x1,&regs.B); }};
    instTblX[0xc9] = {"SET", "SET 1,C", 2, {8}, [] () { SET(0x1,&regs.C); }};
    instTblX[0xca] = {"SET", "SET 1,D", 2, {8}, [] () { SET(0x1,&regs.D); }};
    instTblX[0xcb] = {"SET", "SET 1,E", 2, {8}, [] () { SET(0x1,&regs.E); }};
    instTblX[0xcc] = {"SET", "SET 1,H", 2, {8}, [] () { SET(0x1,&regs.H); }};
    instTblX[0xcd] = {"SET", "SET 1,L", 2, {8}, [] () { SET(0x1,&regs.L); }};
    instTblX[0xce] = {"SET", "SET 1,(HL)", 2, {16}, [] () { SET(0x1,regs.HL); }};
    instTblX[0xcf] = {"SET", "SET 1,A", 2, {8}, [] () { SET(0x1,&regs.A); }};
    instTblX[0xd0] = {"SET", "SET 2,B", 2, {8}, [] () { SET(0x2,&regs.B); }};
    instTblX[0xd1] = {"SET", "SET 2,C", 2, {8}, [] () { SET(0x2,&regs.C); }};
    instTblX[0xd2] = {"SET", "SET 2,D", 2, {8}, [] () { SET(0x2,&regs.D); }};
    instTblX[0xd3] = {"SET", "SET 2,E", 2, {8}, [] () { SET(0x2,&regs.E); }};
    instTblX[0xd4] = {"SET", "SET 2,H", 2, {8}, [] () { SET(0x2,&regs.H); }};
    instTblX[0xd5] = {"SET", "SET 2,L", 2, {8}, [] () { SET(0x2,&regs.L); }};
    instTblX[0xd6] = {"SET", "SET 2,(HL)", 2, {16}, [] () { SET(0x2,regs.HL); }};
    instTblX[0xd7] = {"SET", "SET 2,A", 2, {8}, [] () { SET(0x2,&regs.A); }};
    instTblX[0xd8] = {"SET", "SET 3,B", 2, {8}, [] () { SET(0x3,&regs.B); }};
    instTblX[0xd9] = {"SET", "SET 3,C", 2, {8}, [] () { SET(0x3,&regs.C); }};
    instTblX[0xda] = {"SET", "SET 3,D", 2, {8}, [] () { SET(0x3,&regs.D); }};
    instTblX[0xdb] = {"SET", "SET 3,E", 2, {8}, [] () { SET(0x3,&regs.E); }};
    instTblX[0xdc] = {"SET", "SET 3,H", 2, {8}, [] () { SET(0x3,&regs.H); }};
    instTblX[0xdd] = {"SET", "SET 3,L", 2, {8}, [] () { SET(0x3,&regs.L); }};
    instTblX[0xde] = {"SET", "SET 3,(HL)", 2, {16}, [] () { SET(0x3,regs.HL); }};
    instTblX[0xdf] = {"SET", "SET 3,A", 2, {8}, [] () { SET(0x3,&regs.A); }};
    instTblX[0xe0] = {"SET", "SET 4,B", 2, {8}, [] () { SET(0x4,&regs.B); }};
    instTblX[0xe1] = {"SET", "SET 4,C", 2, {8}, [] () { SET(0x4,&regs.C); }};
    instTblX[0xe2] = {"SET", "SET 4,D", 2, {8}, [] () { SET(0x4,&regs.D); }};
    instTblX[0xe3] = {"SET", "SET 4,E", 2, {8}, [] () { SET(0x4,&regs.E); }};
    instTblX[0xe4] = {"SET", "SET 4,H", 2, {8}, [] () { SET(0x4,&regs.H); }};
    instTblX[0xe5] = {"SET", "SET 4,L", 2, {8}, [] () { SET(0x4,&regs.L); }};
    instTblX[0xe6] = {"SET", "SET 4,(HL)", 2, {16}, [] () { SET(0x4,regs.HL); }};
    instTblX[0xe7] = {"SET", "SET 4,A", 2, {8}, [] () { SET(0x4,&regs.A); }};
    instTblX[0xe8] = {"SET", "SET 5,B", 2, {8}, [] () { SET(0x5,&regs.B); }};
    instTblX[0xe9] = {"SET", "SET 5,C", 2, {8}, [] () { SET(0x5,&regs.C); }};
    instTblX[0xea] = {"SET", "SET 5,D", 2, {8}, [] () { SET(0x5,&regs.D); }};
    instTblX[0xeb] = {"SET", "SET 5,E", 2, {8}, [] () { SET(0x5,&regs.E); }};
    instTblX[0xec] = {"SET", "SET 5,H", 2, {8}, [] () { SET(0x5,&regs.H); }};
    instTblX[0xed] = {"SET", "SET 5,L", 2, {8}, [] () { SET(0x5,&regs.L); }};
    instTblX[0xee] = {"SET", "SET 5,(HL)", 2, {16}, [] () { SET(0x5,regs.HL); }};
    instTblX[0xef] = {"SET", "SET 5,A", 2, {8}, [] () { SET(0x5,&regs.A); }};
    instTblX[0xf0] = {"SET", "SET 6,B", 2, {8}, [] () { SET(0x6,&regs.B); }};
    instTblX[0xf1] = {"SET", "SET 6,C", 2, {8}, [] () { SET(0x6,&regs.C); }};
    instTblX[0xf2] = {"SET", "SET 6,D", 2, {8}, [] () { SET(0x6,&regs.D); }};
    instTblX[0xf3] = {"SET", "SET 6,E", 2, {8}, [] () { SET(0x6,&regs.E); }};
    instTblX[0xf4] = {"SET", "SET 6,H", 2, {8}, [] () { SET(0x6,&regs.H); }};
    instTblX[0xf5] = {"SET", "SET 6,L", 2, {8}, [] () { SET(0x6,&regs.L); }};
    instTblX[0xf6] = {"SET", "SET 6,(HL)", 2, {16}, [] () { SET(0x6,regs.HL); }};
    instTblX[0xf7] = {"SET", "SET 6,A", 2, {8}, [] () { SET(0x6,&regs.A); }};
    instTblX[0xf8] = {"SET", "SET 7,B", 2, {8}, [] () { SET(0x7,&regs.B); }};
    instTblX[0xf9] = {"SET", "SET 7,C", 2, {8}, [] () { SET(0x7,&regs.C); }};
    instTblX[0xfa] = {"SET", "SET 7,D", 2, {8}, [] () { SET(0x7,&regs.D); }};
    instTblX[0xfb] = {"SET", "SET 7,E", 2, {8}, [] () { SET(0x7,&regs.E); }};
    instTblX[0xfc] = {"SET", "SET 7,H", 2, {8}, [] () { SET(0x7,&regs.H); }};
    instTblX[0xfd] = {"SET", "SET 7,L", 2, {8}, [] () { SET(0x7,&regs.L); }};
    instTblX[0xfe] = {"SET", "SET 7,(HL)", 2, {16}, [] () { SET(0x7,regs.HL); }};
    instTblX[0xff] = {"SET", "SET 7,A", 2, {8}, [] () { SET(0x7,&regs.A); }};
}