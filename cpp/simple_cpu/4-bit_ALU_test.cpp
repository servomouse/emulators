#include <iostream>
#include <cstdint>
#include "4-bit_ALU.hpp"

int main() {
    ALU alu;

    // Example: 0x000F + 0x0001 (Should prompt a carry into the next chunk, but let's look at final flags)
    // To test flags on the 4th chunk, let's use the highest nibble: 
    // op1 = 0x8000, op2 = 0x8000 -> 8 + 8 = 16 (Result 0, Carry 1, Zero 1)
    uint16_t ops[] = {0x8000, 0x8000};
    alu.set_op1(0x8000);
    alu.set_op2(0x8000);
    
    std::cout << "Inputs: op1 = 0x8000, op2 = 0x8000\n";
    alu.perform_operation(ALU_Op::ADD);

    std::cout << "Result: 0x" << std::hex << alu.get_res() << std::dec << "\n";
    std::cout << "Flags:\n";
    std::cout << "  Carry: " << alu.get_flag(ALU_Flags::C) << "\n";
    std::cout << "  Half-Carry: " << alu.get_flag(ALU_Flags::H) << "\n";
    std::cout << "  Sign: " << alu.get_flag(ALU_Flags::S) << "\n";
    std::cout << "  Zero: " << alu.get_flag(ALU_Flags::Z) << "\n";

    return 0;
}