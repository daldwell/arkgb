#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "Mmu.h"
#include "Cpu.h"
#include "Opc.h"
#include "Display.h"
#include "GUnit.h"

#define ASSERT_EQUALS(expr1, expr2)   ((expr1 == expr2)?(void)0:__ASSERT_EQUALS(#expr1, #expr2, expr1, expr2,__FILE__,__LINE__));
#define ASSERT_FLAGS(Zv,Hv,Nv,Cv) ASSERT_EQUALS(getFlag(ZF), Zv) ASSERT_EQUALS(getFlag(HF), Hv) ASSERT_EQUALS(getFlag(NF), Nv) ASSERT_EQUALS(getFlag(CF), Cv) 

char errorBuffer[1024];

void __ASSERT_EQUALS(const char * expr1, const char * expr2, int v1, int v2, const char * file, int line)
{
    char buf[128] = "\0";
    sprintf(buf, "\n    Assertion failed, file %s, line %d, %s == %s, expected result: 0x%04x, actual result: 0x%04x", file, line, expr1, expr2, v2, v1);
    strcat(errorBuffer, buf);
}

void beforeEach()
{
    errorBuffer[0] = '\0';
    //displayReset();
    GUReset();
    regs.PC = 0;
}

void test(const char * description, void(*testCase)())
{
    beforeEach();
    testCase();

    if (errorBuffer[0] != '\0') {
        printf("\n%-80s ... FAILED", description);
        printf("%s", errorBuffer);
    } else {
        printf("\n%-80s ... PASSED", description);
    }
}

void testLD8()
{
    test("Load value into reg", [] () {
        byte value = 0x24;
        mmu.PokeByte(regs.PC+1, value);
        instTbl[LD_B_d8_].step();
        ASSERT_EQUALS(regs.B, value);
    });

    test("Load mem value into reg", [] () {
        byte value = 0x24;
        word mem = 0x203a;
        regs.HL = mem;
        mmu.PokeByte(mem, value);
        instTbl[LD_C_mem_HL_].step();
        ASSERT_EQUALS(regs.C, value);
    });

    test("Load reg into mem", [] () {
        byte value = 0x24;
        word mem = 0x203a;
        regs.HL = mem;
        regs.D = value;
        instTbl[LD_mem_HL_D_].step();
        ASSERT_EQUALS(mmu.PeekByte(mem), value);
    });

    test("Load immediate into mem", [] () {
        byte value = 0x24;
        word mem = 0x216b;
        regs.HL = mem;
        mmu.PokeByte(regs.PC+1, value);
        instTbl[LD_mem_HL_d8_].step();
        ASSERT_EQUALS(mmu.PeekByte(mem), value);
    });

    test("Load ff+c into reg", [] () {
        byte value = 0x17;
        regs.C = 0x95;
        mmu.PokeByte(0xFF95, value);
        instTbl[LD_A_mem_C_].step();
        ASSERT_EQUALS(regs.A, value);
    });

    test("Load reg into ff+c", [] () {
        byte value = 0x88;
        regs.A = value;
        regs.C = 0x90;
        instTbl[LD_mem_C_A_].step();
        ASSERT_EQUALS(mmu.PeekByte(0xFF90), value);
    });

    test("Load ff+mem into reg", [] () {
        byte value = 0x34;
        mmu.PokeByte(regs.PC+1, 0x75);
        mmu.PokeByte(0xFF75, value);
        instTbl[LDH_A_mem_a8_].step();
        ASSERT_EQUALS(regs.A, value);
    });

    test("Load reg into ff+mem", [] () {
        byte value = 0x37;
        regs.A = value;
        mmu.PokeByte(regs.PC+1, 0x70);    
        instTbl[LDH_mem_a8_A_].step();
        ASSERT_EQUALS(mmu.PeekByte(0xFF70), value);
    });

    // TODO: rework lcd register access
    // test("Load mem into reg, check that the mmu maps this to the LCD register", [] () {
    //     word mem = 0xFF44;
    //     byte value = 0x1;
    //     mmu.PokeWord(regs.PC+1, mem);
    //     //mmu.PokeByte(mem, value);
    //     lcdRegs.LY = value;
    //     instTbl[LD_A_mem_a16_].step();
    //     ASSERT_EQUALS(regs.A, 0x1);
    // });

    // test("Load reg into mem, check that the mmu maps this to the LCD register", [] () {
    //     word mem = 0xFF40;
    //     byte value = 0x3;    
    //     regs.A = value;
    //     mmu.PokeWord(regs.PC+1, mem);    
    //     instTbl[LD_mem_a16_A_].step();
    //     ASSERT_EQUALS(lcdRegs.LCDC, value);
    // });

    test("Load mem value into reg, check HL dec", [] () {
        word mem = 0x204a;
        byte value = 0x3;    
        regs.HL = mem;
        mmu.PokeByte(mem, value);
        instTbl[LD_A_mem_HL_minus_].step();
        ASSERT_EQUALS(regs.A, value);
        ASSERT_EQUALS(regs.HL, mem-1);
    });

    test("Load reg into mem, check HL dec", [] () {
        word mem = 0x205a;
        byte value = 0x3;    
        regs.HL = mem;
        regs.A = value;
        instTbl[LD_mem_HL_minus_A_].step();
        ASSERT_EQUALS(mmu.PeekByte(mem), value);
        ASSERT_EQUALS(regs.HL, mem-1);
    });

    test("Load mem value into reg, check HL inc", [] () {
        word mem = 0x206a;
        byte value = 0x3;    
        regs.HL = mem;
        mmu.PokeByte(mem, value);
        instTbl[LD_A_mem_HL_plus_].step();
        ASSERT_EQUALS(regs.A, value);
        ASSERT_EQUALS(regs.HL, mem+1);
    });

    test("Load reg into mem, check HL inc", [] () {
        word mem = 0x207a;
        byte value = 0x3;    
        regs.HL = mem;
        regs.A = value;
        instTbl[LD_mem_HL_plus_A_].step();
        ASSERT_EQUALS(mmu.PeekByte(mem),value);
        ASSERT_EQUALS(regs.HL,mem+1);    
    });

}

void testLD16()
{
     test("Load HL", [] () {
        mmu.PokeWord(regs.PC+1, 0x3A5B);    
        instTbl[LD_HL_d16_].step();
        ASSERT_EQUALS(regs.H,0x3A);
        ASSERT_EQUALS(regs.L,0x5B);    
    });   

     test("Load SP HL", [] () {
        regs.HL = 0x4A5C;
        instTbl[LD_SP_HL_].step();
        ASSERT_EQUALS(regs.SP,0x4A5C);
    });

     test("Load SP HL+r8", [] () {
        regs.SP = 0xFFF8;
        mmu.PokeByte(regs.PC+1, 0x2);    
        instTbl[LD_HL_SP_plusr8_].step();
        ASSERT_EQUALS(regs.SP,0xFFF8);
        ASSERT_EQUALS(regs.HL,0xFFFA);
        ASSERT_FLAGS(0, 0, 0, 0);
    });      

     test("Load SP HL+r8 overflow half carry", [] () {
        regs.SP = 0x0FFF;
        mmu.PokeByte(regs.PC+1, 0x1);    
        instTbl[LD_HL_SP_plusr8_].step();
        ASSERT_EQUALS(regs.SP,0x0FFF);
        ASSERT_EQUALS(regs.HL,0x1000);
        ASSERT_FLAGS(0, 1, 0, 0);
    });      

     test("Load SP HL+r8 overflow carry", [] () {
        regs.SP = 0xFFFF;
        mmu.PokeByte(regs.PC+1, 0x1);    
        instTbl[LD_HL_SP_plusr8_].step();
        ASSERT_EQUALS(regs.SP,0xFFFF);
        ASSERT_EQUALS(regs.HL,0x0);
        ASSERT_FLAGS(0, 1, 0, 1);
    });      

     test("Load mem SP", [] () {
        word mem = 0x100;
        regs.SP = 0xFFF8;
        mmu.PokeWord(regs.PC+1, mem);    
        mmu.PokeWord(mem, 0);    
        instTbl[LD_mem_a16_SP_].step();
        ASSERT_EQUALS(mmu.PeekByte(mem),0xF8);
        ASSERT_EQUALS(mmu.PeekByte(mem+1),0xFF);
    });      
}

void testSP()
{
    test("Push stack", [] () {
        byte value = 0xF7;
        word mem = 0x8000;
        regs.SP = mem;
        regs.AF = value;
        instTbl[PUSH_AF_].step();
        ASSERT_EQUALS(mmu.PeekByte(mem-2),value);
        ASSERT_EQUALS(regs.SP,mem-2);
    });

    test("Pop stack", [] () {
        byte value = 0xF7;
        word mem = 0x8000;
        regs.DE = 0xE9;
        regs.SP = mem;
        regs.AF = value;
        instTbl[PUSH_AF_].step();
        instTbl[POP_DE_].step();
        ASSERT_EQUALS(regs.SP,mem);
        ASSERT_EQUALS(regs.DE,value);
    });
}

void testArithmetic8()
{
    test("Add", [] () {
        regs.A = 0x1;
        regs.B = 0x2;
        instTbl[ADD_A_B_].step();
        ASSERT_EQUALS(regs.A,0x3);
        ASSERT_FLAGS(0, 0, 0, 0);
    });

    test("Add overflow half carry", [] () {
        regs.A = 0xf;
        regs.B = 0x5;
        instTbl[ADD_A_B_].step();
        ASSERT_EQUALS(regs.A,0x14);
        ASSERT_FLAGS(0, 1, 0, 0);
    });

    test("Add overflow carry + half carry", [] () {
        regs.A = 0x3A;
        regs.B = 0xC6;
        instTbl[ADD_A_B_].step();
        ASSERT_EQUALS(regs.A,0);
        ASSERT_FLAGS(1, 1, 0, 1);
    });

    test("Add carry 0", [] () {
        setFlag(CF, false);
        regs.A = 0x1;
        regs.B = 0x2;
        instTbl[ADC_A_B_].step();
        ASSERT_EQUALS(regs.A,0x3);
        ASSERT_FLAGS(0, 0, 0, 0);
    });

    test("Add carry 0 overflow half carry", [] () {
        setFlag(CF, false);
        regs.A = 0xf;
        regs.B = 0x5;
        instTbl[ADC_A_B_].step();
        ASSERT_EQUALS(regs.A,0x14);
        ASSERT_FLAGS(0, 1, 0, 0);
    });

    test("Add carry 0 overflow carry + half carry", [] () {
        setFlag(CF, false);
        regs.A = 0x3A;
        regs.B = 0xC6;
        instTbl[ADC_A_B_].step();
        ASSERT_EQUALS(regs.A,0);
        ASSERT_FLAGS(1, 1, 0, 1);
    });

    test("Add carry 1", [] () {
        setFlag(CF, true);
        regs.A = 0x01;
        regs.B = 0xff;
        instTbl[ADC_A_B_].step();
        ASSERT_EQUALS(regs.A,0x1);
        ASSERT_FLAGS(0, 1, 0, 1);
    });

    test("Add carry 1 overflow carry", [] () {
        setFlag(CF, true);
        regs.A = 0xE1;
        mmu.PokeByte(regs.PC+1, 0x3B);
        instTbl[ADC_A_d8_].step();
        ASSERT_EQUALS(regs.A,0x1D);
        ASSERT_FLAGS(0, 0, 0, 1);
    });

    test("Add carry 1 overflow carry + half carry", [] () {
        setFlag(CF, true);
        regs.A = 0xE1;
        regs.HL = 0x9000;
        mmu.PokeByte(regs.HL, 0x1E);
        instTbl[ADC_A_mem_HL_].step();
        ASSERT_EQUALS(regs.A, 0);
        ASSERT_FLAGS(1, 1, 0, 1);
    });

    test("Sub", [] () {
        regs.A = 0x3E;
        regs.E = 0x3E;
        instTbl[SUB_E_].step();
        ASSERT_EQUALS(regs.A,0x0);
        ASSERT_FLAGS(1, 0, 1, 0);
    });

    test("Sub overflow half carry (borrow)", [] () {
        regs.A = 0x3E;
        mmu.PokeByte(regs.PC+1, 0xF);
        instTbl[SUB_d8_].step();
        ASSERT_EQUALS(regs.A,0x2F);
        ASSERT_FLAGS(0, 1, 1, 0);    
    });

    test("Sub overflow carry (borrow)", [] () {
        regs.A = 0x3E;
        regs.HL = 0x9500;
        mmu.PokeByte(regs.HL, 0x40);
        instTbl[SUB_mem_HL_].step();
        ASSERT_EQUALS(regs.A,0xFE);
        ASSERT_FLAGS(0, 0, 1, 1);    
    });

    test("Sub carry 0", [] () {
        setFlag(CF, false);
        regs.A = 0x3E;
        regs.E = 0x3E;
        instTbl[SBC_A_E_].step();
        ASSERT_EQUALS(regs.A,0x0);
        ASSERT_FLAGS(1, 0, 1, 0);
    });

    test("Sub carry 0 overflow half carry (borrow)", [] () {
        setFlag(CF, false);
        regs.A = 0x3E;
        mmu.PokeByte(regs.PC+1, 0xF);
        instTbl[SBC_A_d8_].step();
        ASSERT_EQUALS(regs.A,0x2F);
        ASSERT_FLAGS(0, 1, 1, 0);    
    });

    test("Sub carry 0 overflow carry (borrow)", [] () {
        setFlag(CF, false);
        regs.A = 0x3E;
        regs.HL = 0x9500;
        mmu.PokeByte(regs.HL, 0x40);
        instTbl[SBC_A_mem_HL_].step();
        ASSERT_EQUALS(regs.A,0xFE);
        ASSERT_FLAGS(0, 0, 1, 1);    
    });

    test("Sub carry 1", [] () {
        setFlag(CF, true);
        regs.A = 0x3B;
        regs.H = 0x2A;
        instTbl[SBC_A_H_].step();
        ASSERT_EQUALS(regs.A,0x10);
        ASSERT_FLAGS(0, 0, 1, 0);
    });

    test("Sub carry 1 zero flag", [] () {
        setFlag(CF, true);
        regs.A = 0x3B;
        mmu.PokeByte(regs.PC+1, 0x3A);
        instTbl[SBC_A_d8_].step();
        ASSERT_EQUALS(regs.A,0x0);
        ASSERT_FLAGS(1, 0, 1, 0);    
    });

    test("Sub carry 1 overflow half carry + carry (borrow)", [] () {
        setFlag(CF, true);
        regs.A = 0x3B;
        regs.HL = 0x9500;
        mmu.PokeByte(regs.HL, 0x4F);
        instTbl[SBC_A_mem_HL_].step();
        ASSERT_EQUALS(regs.A,0xEB);
        ASSERT_FLAGS(0, 1, 1, 1);    
    });

    test("Increment", [] () {
        regs.A = 0x3B;
        instTbl[INC_A_].step();
        ASSERT_EQUALS(regs.A,0x3C);
        ASSERT_FLAGS(0, 0, 0, 0);    
    });

    test("Increment half carry", [] () {
        regs.A = 0x0F;
        instTbl[INC_A_].step();
        ASSERT_EQUALS(regs.A,0x10);
        ASSERT_FLAGS(0, 1, 0, 0);    
    });

    test("Increment overflow half carry", [] () {
        regs.A = 0xFF;
        instTbl[INC_A_].step();
        ASSERT_EQUALS(regs.A,0x0);
        ASSERT_FLAGS(1, 1, 0, 0);    
    });

    test("Increment mem", [] () {
        regs.HL = 0x2002;
        mmu.PokeByte(regs.HL, 0x50);
        instTbl[INC_mem_HL_].step();
        ASSERT_EQUALS(mmu.PeekByte(regs.HL),0x51);
        ASSERT_FLAGS(0, 0, 0, 0);    
    });

    test("Decrement", [] () {
        regs.A = 0x3B;
        instTbl[DEC_A_].step();
        ASSERT_EQUALS(regs.A,0x3A);
        ASSERT_FLAGS(0, 0, 1, 0);    
    });

    test("Decrement overflow half carry", [] () {
        regs.A = 0x0;
        instTbl[DEC_A_].step();
        ASSERT_EQUALS(regs.A,0xFF);
        ASSERT_FLAGS(0, 1, 1, 0);    
    });

    test("Decrement mem", [] () {
        regs.HL = 0x2002;
        mmu.PokeByte(regs.HL, 0x01);
        instTbl[DEC_mem_HL_].step();
        ASSERT_EQUALS(mmu.PeekByte(regs.HL),0);
        ASSERT_FLAGS(1, 0, 1, 0);    
    });

}

void testArithmetic16()
{
    test("Add HL BC", [] () {
        regs.HL = 0x8A23;
        regs.BC = 0x0605;
        instTbl[ADD_HL_BC_].step();
        ASSERT_EQUALS(regs.HL,0x9028);
        ASSERT_FLAGS(0, 1, 0, 0);
    });

    test("Add HL HL", [] () {
        regs.HL = 0x8A23;
        instTbl[ADD_HL_HL_].step();
        ASSERT_EQUALS(regs.HL,0x1446);
        ASSERT_FLAGS(0, 1, 0, 1);
    });

    test("Add SP", [] () {
        regs.SP = 0xFF8;
        mmu.PokeByte(regs.PC+1, 0x02);
        instTbl[ADD_SP_r8_].step();
        ASSERT_EQUALS(regs.SP,0xFFA);
        ASSERT_FLAGS(0, 0, 0, 0);
    });

    test("Add SP overflow", [] () {
        regs.SP = 0xFFFF;
        mmu.PokeByte(regs.PC+1, 0x02);
        instTbl[ADD_SP_r8_].step();
        ASSERT_EQUALS(regs.SP,0x1);
        ASSERT_FLAGS(0, 1, 0, 1);
    });

    test("Add SP signed", [] () {
        regs.SP = 0xFFFF;
        mmu.PokeByte(regs.PC+1, 0xFF);
        instTbl[ADD_SP_r8_].step();
        ASSERT_EQUALS(regs.SP,0xFFFE);
        ASSERT_FLAGS(0, 1, 0, 1);
    });

    test("Add SP signed underflow", [] () {
        regs.SP = 0x0000;
        mmu.PokeByte(regs.PC+1, 0xFF);
        instTbl[ADD_SP_r8_].step();
        ASSERT_EQUALS(regs.SP,0xFFFF);
        ASSERT_FLAGS(0, 1, 0, 1);
    });

    test("INC DE", [] () {
        regs.DE = 0x235F;
        instTbl[INC_DE_].step();
        ASSERT_EQUALS(regs.DE,0x2360);
        ASSERT_FLAGS(0, 0, 0, 0);
    });

    test("INC DE overflow", [] () {
        regs.DE = 0xFFFF;
        instTbl[INC_DE_].step();
        ASSERT_EQUALS(regs.DE,0x0);
        ASSERT_FLAGS(0, 0, 0, 0);
    });

    test("DEC DE", [] () {
        regs.DE = 0x235F;
        instTbl[DEC_DE_].step();
        ASSERT_EQUALS(regs.DE,0x235E);
        ASSERT_FLAGS(0, 0, 0, 0);
    });

    test("DEC DE underflow", [] () {
        regs.DE = 0x0;
        instTbl[DEC_DE_].step();
        ASSERT_EQUALS(regs.DE,0xFFFF);
        ASSERT_FLAGS(0, 0, 0, 0);
    });
}

void testLogical()
{
      test("AND L", [] () {
        regs.A = 0x5A;
        regs.L = 0x3F;
        instTbl[AND_L_].step();
        ASSERT_EQUALS(regs.A,0x1A);
        ASSERT_FLAGS(0, 1, 0, 0);
    });  

      test("AND value", [] () {
        regs.A = 0x5A;
        mmu.PokeByte(regs.PC+1, 0x38);
        instTbl[AND_d8_].step();
        ASSERT_EQUALS(regs.A,0x18);
        ASSERT_FLAGS(0, 1, 0, 0);
    });  

    test("AND mem", [] () {
        regs.A = 0x5A;
        regs.HL = 0xB000;
        mmu.PokeByte(regs.HL, 0x0);
        instTbl[AND_mem_HL_].step();
        ASSERT_EQUALS(regs.A,0x0);
        ASSERT_FLAGS(1, 1, 0, 0);
    });  

    test("OR A", [] () {
        regs.A = 0x5A;
        instTbl[OR_A_].step();
        ASSERT_EQUALS(regs.A,0x5A);
        ASSERT_FLAGS(0, 0, 0, 0);
    });  

    test("OR value", [] () {
        regs.A = 0x5A;
        mmu.PokeByte(regs.PC+1, 0x3);
        instTbl[OR_d8_].step();
        ASSERT_EQUALS(regs.A,0x5B);
        ASSERT_FLAGS(0, 0, 0, 0);
    }); 

    test("OR value zero", [] () {
        regs.A = 0;
        mmu.PokeByte(regs.PC+1, 0);
        instTbl[OR_d8_].step();
        ASSERT_EQUALS(regs.A, 0);
        ASSERT_FLAGS(1, 0, 0, 0);
    });   

    test("OR mem", [] () {
        regs.A = 0x5A;
        regs.HL = 0xB000;
        mmu.PokeByte(regs.HL, 0xf);
        instTbl[OR_mem_HL_].step();
        ASSERT_EQUALS(regs.A,0x5F);
        ASSERT_FLAGS(0, 0, 0, 0);
    });     

    test("XOR A", [] () {
        regs.A = 0xFF;
        instTbl[XOR_A_].step();
        ASSERT_EQUALS(regs.A,0x0);
        ASSERT_FLAGS(1, 0, 0, 0);
    });  

    test("XOR value", [] () {
        regs.A = 0xFF;
        mmu.PokeByte(regs.PC+1, 0xF);
        instTbl[XOR_d8_].step();
        ASSERT_EQUALS(regs.A,0xF0);
        ASSERT_FLAGS(0, 0, 0, 0);
    }); 

    test("XOR mem", [] () {
        regs.A = 0xFF;
        regs.HL = 0xB000;
        mmu.PokeByte(regs.HL, 0x8A);
        instTbl[XOR_mem_HL_].step();
        ASSERT_EQUALS(regs.A,0x75);
        ASSERT_FLAGS(0, 0, 0, 0);
    });     

    test("CP A", [] () {
        regs.A = 0x3C;
        regs.B = 0x2F;
        instTbl[CP_B_].step();
        ASSERT_EQUALS(regs.A,0x3C);
        ASSERT_FLAGS(0, 1, 1, 0);
    });  

    test("CP value", [] () {
        regs.A = 0x3C;
        mmu.PokeByte(regs.PC+1, 0x3C);
        instTbl[CP_d8_].step();
        ASSERT_EQUALS(regs.A,0x3C);
        ASSERT_FLAGS(1, 0, 1, 0);
    }); 

    test("CP mem", [] () {
        regs.A = 0x3C;
        regs.HL = 0xB000;
        mmu.PokeByte(regs.HL, 0x40);
        instTbl[CP_mem_HL_].step();
        ASSERT_EQUALS(regs.A,0x3C);
        ASSERT_FLAGS(0, 0, 1, 1);
    });     
}

void testRotate()
{
    test("RLCA", [] () {
        regs.A = 0x85;
        setFlag(CF, false);
        instTbl[RLCA_].step();
        //TODO: gb programming manual seems to think the result is 0xA. How is that possible when the 7th bit is 1 and is carried to bit 0 which results in an odd number?
        //ASSERT_EQUALS(regs.A,0xA); 
        ASSERT_EQUALS(regs.A,0xB);
        ASSERT_FLAGS(0, 0, 0, 1);
    });

    test("RLA", [] () {
        regs.A = 0x95;
        setFlag(CF, true);
        instTbl[RLA_].step();
        ASSERT_EQUALS(regs.A,0x2B);
        ASSERT_FLAGS(0, 0, 0, 1);
    });

    test("RRCA", [] () {
        regs.A = 0x3B;
        setFlag(CF, false);
        instTbl[RRCA_].step();
        ASSERT_EQUALS(regs.A,0x9D);
        ASSERT_FLAGS(0, 0, 0, 1);
    });

    test("RRA", [] () {
        regs.A = 0x81;
        setFlag(CF, false);
        instTbl[RRA_].step();
        ASSERT_EQUALS(regs.A,0x40);
        ASSERT_FLAGS(0, 0, 0, 1);
    });

    //TODO: RLC

    //TODO: RL

    //TODO: RRC

    test("RR A", [] () {
        regs.A = 0x01;
        mmu.PokeByte(regs.PC+1, RR_A_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(regs.A,0x0);
        ASSERT_FLAGS(1, 0, 0, 1);
    }); 

    test("RR D", [] () {
        setFlag(CF, true);
        regs.D = 0x83;
        mmu.PokeByte(regs.PC+1, RR_D_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(regs.D,0xC1);
        ASSERT_FLAGS(0, 0, 0, 1);
    }); 

    test("RR HL", [] () {
        regs.HL = 0xB000;
        mmu.PokeByte(regs.HL, 0x8A);
        mmu.PokeByte(regs.PC+1, RR_mem_HL_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(mmu.PeekByte(regs.HL),0x45);
        ASSERT_FLAGS(0, 0, 0, 0);
    }); 

    test("SLA D", [] () {
        regs.D = 0x80;
        setFlag(CF, false);
        mmu.PokeByte(regs.PC+1, SLA_D_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(regs.D,0x0);
        ASSERT_FLAGS(1, 0, 0, 1);
    }); 

    test("SLA HL", [] () {
        regs.HL = 0xB000;
        mmu.PokeByte(regs.HL, 0xFF);
        setFlag(CF, false);
        mmu.PokeByte(regs.PC+1, SLA_mem_HL_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(mmu.PeekByte(regs.HL),0xFE);
        ASSERT_FLAGS(0, 0, 0, 1);
    }); 

    test("SRA A", [] () {
        regs.A = 0x8A;
        setFlag(CF, false);
        mmu.PokeByte(regs.PC+1, SRA_A_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(regs.A,0xC5);
        ASSERT_FLAGS(0, 0, 0, 0);
    }); 

    test("SRA HL", [] () {
        regs.HL = 0xB000;
        mmu.PokeByte(regs.HL, 0x01);
        setFlag(CF, false);
        mmu.PokeByte(regs.PC+1, SRA_mem_HL_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(mmu.PeekByte(regs.HL),0x0);
        ASSERT_FLAGS(1, 0, 0, 1);
    }); 

    test("SRL A", [] () {
        regs.A = 0x01;
        setFlag(CF, false);
        mmu.PokeByte(regs.PC+1, SRL_A_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(regs.A,0x0);
        ASSERT_FLAGS(1, 0, 0, 1);
    }); 

    test("SRL HL", [] () {
        regs.HL = 0xB000;
        mmu.PokeByte(regs.HL, 0xFF);
        setFlag(CF, false);
        mmu.PokeByte(regs.PC+1, SRL_mem_HL_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(mmu.PeekByte(regs.HL),0x7F);
        ASSERT_FLAGS(0, 0, 0, 1);
    }); 

    test("SWAP A", [] () {
        regs.A = 0x0;
        mmu.PokeByte(regs.PC+1, SWAP_A_);
        setFlag(CF, false);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(regs.A,0x0);
        ASSERT_FLAGS(1, 0, 0, 0);
    });

    test("SWAP HL", [] () {
        regs.HL = 0xB000;
        mmu.PokeByte(regs.HL, 0xFE);
        mmu.PokeByte(regs.PC+1, SWAP_mem_HL_);
        setFlag(CF, false);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(mmu.PeekByte(regs.HL),0xEF);
        ASSERT_FLAGS(0, 0, 0, 0);
    });    

}

void testBit()
{
     test("BIT A", [] () {
        regs.A = 0x80;
        mmu.PokeByte(regs.PC+1, BIT_7_A_);
        instTbl[PREFIX_CB_].step();
        ASSERT_FLAGS(0, 1, 0, 0);
     });

     test("BIT L", [] () {
        regs.L = 0xEF;
        mmu.PokeByte(regs.PC+1, BIT_4_L_);
        instTbl[PREFIX_CB_].step();
        ASSERT_FLAGS(1, 1, 0, 0);
     });

    test("BIT HL", [] () {
        regs.HL = 0xB000;
        mmu.PokeByte(regs.HL, 0xFE);
        mmu.PokeByte(regs.PC+1, BIT_0_mem_HL_);
        instTbl[PREFIX_CB_].step();
        ASSERT_FLAGS(1, 1, 0, 0);
     });

     test("BIT HL", [] () {
        regs.HL = 0xB000;
        mmu.PokeByte(regs.HL, 0xFE);
        mmu.PokeByte(regs.PC+1, BIT_1_mem_HL_);
        instTbl[PREFIX_CB_].step();
        ASSERT_FLAGS(0, 1, 0, 0);
     });

     test("SET 3 A", [] () {
        regs.A = 0x80;
        mmu.PokeByte(regs.PC+1, SET_3_A_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(regs.A, 0x88);
     });

     test("SET 7 L", [] () {
        regs.L = 0x3B;
        mmu.PokeByte(regs.PC+1, SET_7_L_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(regs.L, 0xBB);
     });

    test("SET 3 mem HL", [] () {
        regs.HL = 0xB000;
        mmu.PokeByte(regs.HL, 0x00);
        mmu.PokeByte(regs.PC+1, SET_3_mem_HL_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(mmu.PeekByte(regs.HL), 0x08);
     }); 

     test("RES 0 A", [] () {
        regs.A = 0x86;
        mmu.PokeByte(regs.PC+1, RES_7_A_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(regs.A, 0x06);
     });

    test("RES 2 A", [] () {
        regs.A = 0x86;
        mmu.PokeByte(regs.PC+1, RES_2_A_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(regs.A, 0x82);
     }); 

    test("RES 1 L", [] () {
        regs.L = 0x39;
        mmu.PokeByte(regs.PC+1, RES_1_L_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(regs.L, 0x39);
     }); 

    test("RES 3 mem HL", [] () {
        regs.HL = 0xB000;
        mmu.PokeByte(regs.HL, 0xF7);
        mmu.PokeByte(regs.PC+1, RES_3_mem_HL_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(mmu.PeekByte(regs.HL), 0xF7);
     }); 

    test("RES 1 mem HL", [] () {
        regs.HL = 0xB000;
        mmu.PokeByte(regs.HL, 0xF7);
        mmu.PokeByte(regs.PC+1, RES_1_mem_HL_);
        instTbl[PREFIX_CB_].step();
        ASSERT_EQUALS(mmu.PeekByte(regs.HL), 0xF5);
     }); 
}

void testJump()
{
    test("JP", [] () {
        mmu.PokeWord(regs.PC+1, 0x8000);
        instTbl[JP_a16_].step();
        ASSERT_EQUALS(regs.PC, 0x8000);

        mmu.PokeWord(regs.PC+1, 0x8600);
        instTbl[JP_a16_].step();
        ASSERT_EQUALS(regs.PC, 0x8600);
    });

    test("JP condition carry", [] () {

        setFlag(CF, false);
        mmu.PokeWord(regs.PC+1, 0x8000);
        instTbl[JP_C_a16_].step();
        ASSERT_EQUALS(regs.PC, 0x3);       

        setFlag(CF, true);
        mmu.PokeWord(regs.PC+1, 0x8700);
        instTbl[JP_C_a16_].step();
        ASSERT_EQUALS(regs.PC, 0x8700);
    });

    test("JP condition no carry", [] () {
        setFlag(CF, false);
        mmu.PokeWord(regs.PC+1, 0x8000);
        instTbl[JP_NC_a16_].step();
        ASSERT_EQUALS(regs.PC, 0x8000);
    });

    test("JP condition zero", [] () {
        setFlag(ZF, true);
        mmu.PokeWord(regs.PC+1, 0x8000);
        instTbl[JP_Z_a16_].step();
        ASSERT_EQUALS(regs.PC, 0x8000);
    });

    test("JP condition no zero", [] () {
        setFlag(ZF, false);
        mmu.PokeWord(regs.PC+1, 0x8000);
        instTbl[JP_NZ_a16_].step();
        ASSERT_EQUALS(regs.PC, 0x8000);
    });

    test("JR", [] () {
        mmu.PokeByte(regs.PC+1, 0x46);
        instTbl[JR_r8_].step();
        ASSERT_EQUALS(regs.PC, 0x48);

        mmu.PokeByte(regs.PC+1, 0x40);
        instTbl[JR_r8_].step();
        ASSERT_EQUALS(regs.PC, 0x8A);

        mmu.PokeByte(regs.PC+1, 0xFC);
        instTbl[JR_r8_].step();
        ASSERT_EQUALS(regs.PC, 0x88);
    });

    test("JR condition carry", [] () {

        setFlag(CF, false);
        mmu.PokeByte(regs.PC+1, 0x86);
        instTbl[JR_C_r8_].step();
        ASSERT_EQUALS(regs.PC, 2);       

        setFlag(CF, true);
        mmu.PokeByte(regs.PC+1, 0x46);
        instTbl[JR_C_r8_].step();
        ASSERT_EQUALS(regs.PC, 0x4A);
    });

    test("JR condition no carry", [] () {
        setFlag(CF, false);
        regs.PC = 0x100;
        mmu.PokeByte(regs.PC+1, 0xFD);
        instTbl[JR_NC_r8_].step();
        ASSERT_EQUALS(regs.PC, 0xFF);
    });

    test("JR condition zero", [] () {
        setFlag(ZF, true);
        mmu.PokeByte(regs.PC+1, 0x86);
        instTbl[JR_Z_r8_].step();
        ASSERT_EQUALS(regs.PC, 0x86);
    });

    test("JR condition no zero", [] () {
        regs.PC = 0xB000;
        setFlag(ZF, false);
        mmu.PokeByte(regs.PC+1, 0xFC);
        instTbl[JR_NZ_r8_].step();
        ASSERT_EQUALS(regs.PC, 0x1FFE);
    });

    test("JP SP", [] () {
        regs.HL = 0x8000;
        instTbl[JP_mem_HL_].step();
        ASSERT_EQUALS(regs.PC, 0x8000);
    });
}

void testCall()
{
    test("CALL", [] () {
        regs.PC = 0x8000;
        regs.SP = 0xFFFE;
        mmu.PokeWord(regs.PC+1, 0x1234);
        instTbl[CALL_a16_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFC);
        ASSERT_EQUALS(mmu.PeekWord(regs.SP), 0x8000+3);
        ASSERT_EQUALS(regs.PC, 0x1234);
    });

    test("CALL condition carry", [] () {
        setFlag(CF, true);
        regs.PC = 0x8000;
        regs.SP = 0xFFFE;
        mmu.PokeWord(regs.PC+1, 0x1234);
        instTbl[CALL_C_a16_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFC);
        ASSERT_EQUALS(mmu.PeekWord(regs.SP), 0x8000+3);
        ASSERT_EQUALS(regs.PC, 0x1234);
    });

    test("CALL condition no carry", [] () {
        setFlag(CF, false);
        regs.PC = 0x8000;
        regs.SP = 0xFFFE;
        mmu.PokeWord(regs.PC+1, 0x1234);
        instTbl[CALL_NC_a16_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFC);
        ASSERT_EQUALS(mmu.PeekWord(regs.SP), 0x8000+3);
        ASSERT_EQUALS(regs.PC, 0x1234);
    });

    test("CALL condition no carry where carry 1", [] () {
        setFlag(CF, true);
        regs.PC = 0x8000;
        regs.SP = 0xFFFE;
        mmu.PokeWord(regs.PC+1, 0x1234);
        instTbl[CALL_NC_a16_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFE);
        ASSERT_EQUALS(mmu.PeekWord(regs.SP), 0);
        ASSERT_EQUALS(regs.PC, 0x8003);
    });

    test("CALL condition zero", [] () {
        setFlag(ZF, true);
        regs.PC = 0x8000;
        regs.SP = 0xFFFE;
        mmu.PokeWord(regs.PC+1, 0x1234);
        instTbl[CALL_Z_a16_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFC);
        ASSERT_EQUALS(mmu.PeekWord(regs.SP), 0x8000+3);
        ASSERT_EQUALS(regs.PC, 0x1234);
    });

    test("CALL condition no zero", [] () {
        setFlag(ZF, false);
        regs.PC = 0x8000;
        regs.SP = 0xFFFE;
        mmu.PokeWord(regs.PC+1, 0x1234);
        instTbl[CALL_NZ_a16_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFC);
        ASSERT_EQUALS(mmu.PeekWord(regs.SP), 0x8000+3);
        ASSERT_EQUALS(regs.PC, 0x1234);
    });

    test("RET", [] () {
        regs.PC = 0x8000;
        regs.SP = 0xFFFE;
        mmu.PokeWord(regs.PC+1, 0x9000);
        instTbl[CALL_a16_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFC);
        ASSERT_EQUALS(mmu.PeekWord(regs.SP), 0x8000+3);
        ASSERT_EQUALS(regs.PC, 0x9000);
        instTbl[RET_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFE);
        ASSERT_EQUALS(regs.PC, 0x8003);
    });

    test("RETI", [] () {
        regs.PC = 0xB000;
        regs.SP = 0xFFFC;
        mmu.PokeWord(regs.SP, 0x8001);
        instTbl[RETI_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFE);
        ASSERT_EQUALS(regs.PC, 0x8001);
        //TODO: assert interrupt enable
    });

    test("RET Carry 0", [] () {
        setFlag(CF, false);
        regs.PC = 0x8000;
        regs.SP = 0xFFFE;
        mmu.PokeWord(regs.PC+1, 0x9000);
        instTbl[CALL_a16_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFC);
        ASSERT_EQUALS(mmu.PeekWord(regs.SP), 0x8000+3);
        ASSERT_EQUALS(regs.PC, 0x9000);
        instTbl[RET_C_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFC);
        ASSERT_EQUALS(regs.PC, 0x9001);
    });

    test("RET Carry 1", [] () {
        setFlag(CF, true);
        regs.PC = 0x8000;
        regs.SP = 0xFFFE;
        mmu.PokeWord(regs.PC+1, 0x9000);
        instTbl[CALL_a16_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFC);
        ASSERT_EQUALS(mmu.PeekWord(regs.SP), 0x8000+3);
        ASSERT_EQUALS(regs.PC, 0x9000);
        instTbl[RET_C_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFE);
        ASSERT_EQUALS(regs.PC, 0x8003);
    });

    test("RET No Carry 1", [] () {
        setFlag(CF, false);
        regs.PC = 0x8000;
        regs.SP = 0xFFFE;
        mmu.PokeWord(regs.PC+1, 0x9000);
        instTbl[CALL_a16_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFC);
        ASSERT_EQUALS(mmu.PeekWord(regs.SP), 0x8000+3);
        ASSERT_EQUALS(regs.PC, 0x9000);
        instTbl[RET_NC_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFE);
        ASSERT_EQUALS(regs.PC, 0x8003);
    });

    test("RET Zero ", [] () {
        setFlag(ZF, true);
        regs.PC = 0x8000;
        regs.SP = 0xFFFE;
        mmu.PokeWord(regs.PC+1, 0x9000);
        instTbl[CALL_a16_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFC);
        ASSERT_EQUALS(mmu.PeekWord(regs.SP), 0x8000+3);
        ASSERT_EQUALS(regs.PC, 0x9000);
        instTbl[RET_Z_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFE);
        ASSERT_EQUALS(regs.PC, 0x8003);
    });

    test("RET No Zero", [] () {
        setFlag(ZF, false);
        regs.PC = 0x8000;
        regs.SP = 0xFFFE;
        mmu.PokeWord(regs.PC+1, 0x9000);
        instTbl[CALL_a16_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFC);
        ASSERT_EQUALS(mmu.PeekWord(regs.SP), 0x8000+3);
        ASSERT_EQUALS(regs.PC, 0x9000);
        instTbl[RET_NZ_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFE);
        ASSERT_EQUALS(regs.PC, 0x8003);
    });

    test("RST 0", [] () {
        regs.PC = 0x8000;
        regs.SP = 0xFFFE;
        instTbl[RST_00H_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFC);
        ASSERT_EQUALS(mmu.PeekWord(regs.SP), 0x8000+1);
        ASSERT_EQUALS(regs.PC, 0x0000);
    });

    test("RST 1", [] () {
        regs.PC = 0x8000;
        regs.SP = 0xFFFE;
        instTbl[RST_08H_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFC);
        ASSERT_EQUALS(mmu.PeekWord(regs.SP), 0x8000+1);
        ASSERT_EQUALS(regs.PC, 0x0008);
    });

    test("RST 7", [] () {
        regs.PC = 0x8000;
        regs.SP = 0xFFFE;
        instTbl[RST_38H_].step();
        ASSERT_EQUALS(regs.SP, 0xFFFC);
        ASSERT_EQUALS(mmu.PeekWord(regs.SP), 0x8000+1);
        ASSERT_EQUALS(regs.PC, 0x0038);
    });
}

void testMisc()
{
    test("DAA", [] () {
        regs.A = 0x45;
        regs.B = 0x38;
        instTbl[ADD_A_B_].step();
        instTbl[DAA_].step();
        ASSERT_EQUALS(regs.A, 0x83);
        ASSERT_FLAGS(0, 0, 0, 0);
        instTbl[SUB_B_].step();
        instTbl[DAA_].step();
        ASSERT_EQUALS(regs.A, 0x45);
        ASSERT_FLAGS(0, 0, 1, 0);
    }); 

    test("DAA overflow", [] () {
        regs.A = 0x99;
        regs.B = 0x01;
        instTbl[ADD_A_B_].step();
        instTbl[DAA_].step();
        ASSERT_EQUALS(regs.A, 0x0);
        ASSERT_FLAGS(1, 0, 0, 1);
    }); 

    test("DAA overflow 2", [] () {
        regs.A = 0x99;
        regs.B = 0x09;
        instTbl[ADD_A_B_].step();
        instTbl[DAA_].step();
        ASSERT_EQUALS(regs.A, 0x08);
        ASSERT_FLAGS(0, 0, 0, 1);
    }); 

    test("CPL", [] () {
        regs.A = 0x99;
        instTbl[ADD_A_B_].step();
        instTbl[CPL_].step();
        ASSERT_EQUALS(regs.A, 0x66);
    }); 

    test("SCF", [] () {
        setFlag(CF, false);
        instTbl[ADD_A_B_].step();
        instTbl[SCF_].step();
        ASSERT_EQUALS(getFlag(CF), true);
    }); 

    test("CCF", [] () {
        setFlag(CF, true);
        instTbl[ADD_A_B_].step();
        instTbl[CCF_].step();
        ASSERT_EQUALS(getFlag(CF), false);
    }); 

    //TODO: DI

    //TODO: EI

    //TODO: HALT

    //TODO: STOP
}

void run(void(*testCase)())
{
    beforeEach();
    testCase();
}

void executeTests()
{
    // Check that opcodes are loaded
    assert(instTbl[NOP_].length != 0);
    
    // Execute test groups
    run(testLD8);
    run(testLD16);
    run(testSP);
    run(testArithmetic8);
    run(testArithmetic16);
    run(testLogical);
    run(testRotate);
    run(testBit);
    run(testJump);
    run(testCall);
    run(testMisc);

}