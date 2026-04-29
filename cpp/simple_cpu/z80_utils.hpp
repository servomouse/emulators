#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Calculates the Overflow (P/V) flag for Z80-style arithmetic.
 * * @param op1       The first operand (8-bit or 16-bit).
 * @param op2       The second operand (8-bit or 16-bit).
 * @param result    The result of the operation.
 * @param is_sub    True if subtraction, False if addition.
 * @param is_16bit  True if 16-bit operation, False if 8-bit.
 * @return          True if overflow (V) occurred, False otherwise.
 */
bool calculate_overflow(uint32_t op1, uint32_t op2, uint32_t result, bool is_sub, bool is_16bit) {
    uint32_t sign_bit = is_16bit ? 0x8000 : 0x80;
    
    // Extract sign bits
    uint32_t s1 = (op1 & sign_bit);
    uint32_t s2 = (op2 & sign_bit);
    uint32_t sr = (result & sign_bit);

    if (is_sub) {
        // Subtraction overflow: 
        // Occurs if signs of operands were different, 
        // AND the sign of the result is different from the minuend (op1).
        return ((s1 != s2) && (sr != s1));
    } else {
        // Addition overflow: 
        // Occurs if signs of operands were the same, 
        // AND the sign of the result is different from the operand sign.
        return ((s1 == s2) && (sr != s1));
    }
}

/**
 * Calculates the Half-Carry (H) flag.
 * @param op1     The first operand (16-bit).
 * @param op2     The second operand (16-bit).
 * @param is_sub  True if subtraction, False if addition.
 * @return        True if Half-Carry occurred.
 */
bool calculate_half_carry(uint16_t op1, uint16_t op2, uint16_t carry_in, bool is_sub) {
    if (is_sub) {
        // Half-Borrow: true if the lower nibble of op1 is less than op2
        return (static_cast<int32_t>(op1 & 0x0F) - static_cast<int32_t>(op2 & 0x0F) - static_cast<int32_t>(carry_in & 0x0F)) < 0;
    } else {
        // Half-Carry: true if the sum of lower nibbles exceeds 0x0F
        return ((op1 & 0x0F) + (op2 & 0x0F) + (carry_in & 0x0F)) > 0x0F;
    }
}

bool z80_parity_flag(uint16_t a) {
    uint16_t result = a;
    for(int i=1; i<16; i++) {
        result ^= (a >> i);
    }
    result = !(result & 1);
    return result != 0;
    // return (!( (a ^ (a >> 1) ^ (a >> 2) ^ (a >> 3) ^ (a >> 4) ^ (a >> 5) ^ (a >> 6) ^ (a >> 7)) & 1 ));
}

bool z80_zero_flag(uint16_t a) {
    return a == 0;
}

bool z80_sign_flag(uint16_t a, bool is_16b) {
    if (is_16b) return (a & 0x8000) > 0;
    return (a & 0x80) > 0;
}
