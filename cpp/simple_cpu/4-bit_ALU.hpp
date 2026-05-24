#pragma once
#include <iostream>
#include <cstdint>

// Enums for Flags and Operations
enum class ALU_Flags {
    C,  // Carry
    H,  // Half-Carry
    S,  // Sign
    Z   // Zero
};

enum class ALU_Op {
    ADD
};

class ALU {
private:
    uint16_t op1 = 0;
    uint16_t op2 = 0;
    uint16_t res = 0;
    
    // Internal boolean array to track flag states
    bool flags[4] = {false, false, false, false}; 

    // Helper functions to map enum to array index
    int get_flag_index(ALU_Flags flag) const {
        return static_cast<int>(flag);
    }

public:
    // Setters
    void set_op1(uint16_t val) { op1 = val; }
    void set_op2(uint16_t val) { op2 = val; }
    
    void set_flag(ALU_Flags flag) {
        flags[get_flag_index(flag)] = true;
    }
    
    void clear_flag(ALU_Flags flag) {
        flags[get_flag_index(flag)] = false;
    }

    // Getters
    uint16_t get_res() const { return res; }
    
    bool get_flag(ALU_Flags flag) const {
        return flags[get_flag_index(flag)];
    }

    // Main execution method
    void perform_operation(ALU_Op op) {
        if (op == ALU_Op::ADD) {
            // Read initial carry status from the flags
            uint16_t carry = flags[get_flag_index(ALU_Flags::C)] ? 1 : 0;
            
            uint16_t final_res = 0;
            uint16_t last_chunk_carry_in = 0;
            uint16_t last_chunk_half_carry = 0;
            uint16_t last_chunk_res = 0;

            // Loop 4 times to process four 4-bit nibbles (simulating a 4-bit sequential ALU)
            for (int i = 0; i < 4; ++i) {
                // Extract the current 4-bit nibbles
                uint16_t n1 = (op1 >> (i * 4)) & 0x0F;
                uint16_t n2 = (op2 >> (i * 4)) & 0x0F;

                // Track conditions for the very last nibble to calculate H, S, and Z flags correctly
                if (i == 3) {
                    last_chunk_carry_in = carry;
                    // Half-carry within a 4-bit chunk happens when moving from bit 1 to bit 2
                    // We calculate it by adding the lowest 2 bits of the nibbles + incoming carry
                    last_chunk_half_carry = ((n1 & 0x03) + (n2 & 0x03) + last_chunk_carry_in);
                }

                // Perform 4-bit addition including the carry
                uint16_t chunk_res = n1 + n2 + carry;

                // Determine the next carry (1 if result exceeds 4 bits/15, else 0)
                carry = (chunk_res > 0x0F) ? 1 : 0;

                if (i == 3) {
                    last_chunk_res = chunk_res & 0x0F;
                }

                // Place the 4-bit result back into the final 16-bit structure
                final_res |= ((chunk_res & 0x0F) << (i * 4));
            }

            // Save final result
            res = final_res;

            // --- Update Flags based on the final 4-bit result (the highest nibble) ---
            
            // Carry Flag: Outflow from the final 4-bit operation
            flags[get_flag_index(ALU_Flags::C)] = (carry == 1);

            // Half-Carry Flag: Check if bit 1 overflowed into bit 2 during the last chunk addition
            flags[get_flag_index(ALU_Flags::H)] = (last_chunk_half_carry > 0x03);

            // Sign Flag: The highest bit (bit 3) of the final 4-bit result chunk
            flags[get_flag_index(ALU_Flags::S)] = (last_chunk_res & 0x08) != 0;

            // Zero Flag: Check if the final 4-bit result chunk is completely 0
            flags[get_flag_index(ALU_Flags::Z)] = (last_chunk_res == 0);
        }
    }
};