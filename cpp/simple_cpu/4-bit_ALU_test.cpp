#include <iostream>
#include <cstdint>
#include <stdio.h>
#include "4-bit_ALU.hpp"

int main() {
    ALU alu;

    // Example: 0x000F + 0x0001 (Should prompt a carry into the next chunk, but let's look at final flags)
    // To test flags on the 4th chunk, let's use the highest nibble: 
    // op1 = 0x8000, op2 = 0x8000 -> 8 + 8 = 16 (Result 0, Carry 1, Zero 1)
    uint16_t ops[2] = {0x8000, 0x8000};
    alu.set_op1(ops[0]);
    alu.set_op2(ops[1]);
    alu.clear_all_flags();
    alu.perform_operation(ALU_Op:: ADD, 16);
    uint16_t res = alu.get_res();
    printf("0x%04X + 0x%04X = 0x%04X; Carry: %d, Half-Carry: %d, Sign: %d, Zero: %d, Parity: %d, Signed_Overflow: %d\n",
        ops[0],
        ops[1],
        res,
        alu.get_flag(ALU_Flags::C),
        alu.get_flag(ALU_Flags::H),
        alu.get_flag(ALU_Flags::S),
        alu.get_flag(ALU_Flags::Z),
        alu.get_flag(ALU_Flags::P),
        alu.get_flag(ALU_Flags::O)
    );
    ops[0] = 0x1030;
    ops[1] = 0x0204;
    alu.set_op1(ops[0]);
    alu.set_op2(ops[1]);
    alu.clear_all_flags();
    alu.perform_operation(ALU_Op:: ADD, 16);
    res = alu.get_res();
    printf("0x%04X + 0x%04X = 0x%04X; Carry: %d, Half-Carry: %d, Sign: %d, Zero: %d, Parity: %d, Signed_Overflow: %d\n",
        ops[0],
        ops[1],
        res,
        alu.get_flag(ALU_Flags::C),
        alu.get_flag(ALU_Flags::H),
        alu.get_flag(ALU_Flags::S),
        alu.get_flag(ALU_Flags::Z),
        alu.get_flag(ALU_Flags::P),
        alu.get_flag(ALU_Flags::O)
    );
    ops[0] = 0x8800;
    ops[1] = 0x0844;
    alu.set_op1(ops[0]);
    alu.set_op2(ops[1]);
    alu.clear_all_flags();
    alu.perform_operation(ALU_Op:: ADD, 16);
    res = alu.get_res();
    printf("0x%04X + 0x%04X = 0x%04X; Carry: %d, Half-Carry: %d, Sign: %d, Zero: %d, Parity: %d, Signed_Overflow: %d\n",
        ops[0],
        ops[1],
        res,
        alu.get_flag(ALU_Flags::C),
        alu.get_flag(ALU_Flags::H),
        alu.get_flag(ALU_Flags::S),
        alu.get_flag(ALU_Flags::Z),
        alu.get_flag(ALU_Flags::P),
        alu.get_flag(ALU_Flags::O)
    );
    ops[0] = 0x88;
    ops[1] = 0x88;
    alu.set_op1(ops[0]);
    alu.set_op2(ops[1]);
    alu.clear_all_flags();
    alu.perform_operation(ALU_Op:: ADD, 8);
    res = alu.get_res();
    printf("0x%04X + 0x%04X = 0x%04X; Carry: %d, Half-Carry: %d, Sign: %d, Zero: %d, Parity: %d, Signed_Overflow: %d\n",
        ops[0],
        ops[1],
        res,
        alu.get_flag(ALU_Flags::C),
        alu.get_flag(ALU_Flags::H),
        alu.get_flag(ALU_Flags::S),
        alu.get_flag(ALU_Flags::Z),
        alu.get_flag(ALU_Flags::P),
        alu.get_flag(ALU_Flags::O)
    );

    return 0;
}