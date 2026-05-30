#pragma once
#include <iostream>
#include <cstdint>
#include <array>

// Enums for Flags and Operations
enum class ALU_Flags {
    C,  // Carry
    H,  // Half-Carry
    S,  // Sign
    Z,  // Zero
    P,  // Parity
    O,  // Signed Overflow
    COUNT
};

enum class ALU_Op {
    ADD,
    SUB,
    AND,
    XOR,
    OR,
    COUNT
};

class ALU {
private:
    uint16_t _op1 = 0;
    uint16_t _op2 = 0;
    uint16_t _res = 0;
    uint8_t _num_bits;
    static constexpr uint8_t _num_flags = static_cast<size_t>(ALU_Flags::COUNT);
    
    // Internal boolean array to track flag states
    std::array<bool, _num_flags> flags{}; 

    // Helper functions to map enum to array index
    int get_flag_index(ALU_Flags flag) const {
        return static_cast<size_t>(flag);
    }

    bool get_signed_overflow_flag(uint16_t s1, uint16_t s2, uint16_t sr, bool is_sub) {
        // Only valid for addition and subttraction
        if (is_sub) {
            return ((s1 != s2) && (sr != s1));
        }
        return ((s1 == s2) && (sr != s1));
    }

    void alu_execute_operation(ALU_Op operation) {
        uint8_t num_nibbles = _num_bits >> 2;
        uint8_t last_nibble = num_nibbles - 1;
        uint16_t sb1 = (_op1 >> (_num_bits-1)) & 1;
        uint16_t sb2 = (_op2 >> (_num_bits-1)) & 1;
        uint16_t carry = flags[get_flag_index(ALU_Flags::C)]? 1: 0;
        uint16_t half_carry = 0;
        uint16_t final_res = 0;
        uint16_t res = 0;

        // Parity calculation helper:
        //Nibble value:              0, 1, 2, 3, 4, 5, 6, 7, 8, 9, A, B, C, D, E, F
        uint8_t nibble_parity[16] = {0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0};
        uint8_t parity_flag = 0;

        for (int i = 0; i < num_nibbles; i++) {
            uint16_t nibble_offset = i * 4;
            uint16_t n1 = (_op1 >> nibble_offset) & 0x0F;
            uint16_t n2 = (_op2 >> nibble_offset) & 0x0F;
            uint16_t temp_res;

            switch (operation) {
                case ALU_Op::ADD:
                    temp_res = n1 + n2 + carry;
                    carry = (temp_res > 0x0F) ? 1 : 0;
                    break;
                case ALU_Op::SUB:
                    temp_res = n1 - (n2 + carry);
                    carry = ((n2 + carry) > n1) ? 1 : 0;
                    break;
                case ALU_Op::AND:
                    temp_res = n1 & n2;
                    break;
                case ALU_Op::XOR:
                    temp_res = n1 ^ n2;
                    break;
                case ALU_Op::OR:
                    temp_res = n1 | n2;
                    break;
                default:
                    printf("Error: Unknown ALU operation: 0x%X\n", static_cast<size_t>(operation));
                    exit(EXIT_FAILURE);
            }
            temp_res &= 0x0F;

            res |= temp_res << nibble_offset;
            parity_flag = (parity_flag + nibble_parity[temp_res]) & 1;
            if (i < last_nibble) {
                half_carry = carry;
            }
            if (i < last_nibble) {
                half_carry = carry;
            }
        }
        _res = res;

        // Update flags
        uint16_t sbr = (res >> (_num_bits-1)) & 1;

        flags[get_flag_index(ALU_Flags::C)] = carry == 1;
        flags[get_flag_index(ALU_Flags::H)] = half_carry == 1;
        flags[get_flag_index(ALU_Flags::S)] = sbr;
        flags[get_flag_index(ALU_Flags::Z)] = res == 0;
        flags[get_flag_index(ALU_Flags::P)] = parity_flag == 0; // 0 if even, 1 if odd
        flags[get_flag_index(ALU_Flags::O)] = get_signed_overflow_flag(sb1, sb2, sbr, operation==ALU_Op::SUB);
    }

    // --- ALU operations ---
    void op_add(void) {
        // Does ADD and ADC: set Carry prior the execution if needed
        alu_execute_operation(ALU_Op::ADD);
    }
    void op_sub(void) {
        // Does SUB and SBC: set Carry prior the execution if needed
        alu_execute_operation(ALU_Op::SUB);
    }
    void op_and(void) {
        alu_execute_operation(ALU_Op::AND);
    }
    void op_xor(void) {
        alu_execute_operation(ALU_Op::XOR);
    }
    void op_or(void) {
        alu_execute_operation(ALU_Op::OR);
    }
    // --- !ALU operations ---

    // --- The Function Pointer Type & Lookup Table ---
    
    // Define an alias for a pointer to an ALU member function taking no arguments and returning void
    using AluMethodPtr = void (ALU::*)();

    // Compile-time array mapping ALU_Op constants to actual methods
    // The syntax 'this->*alu_operations[...]' is required to call non-static member functions
    static constexpr std::array<AluMethodPtr, static_cast<size_t>(ALU_Op::COUNT)> alu_operations = {
        &ALU::op_add,
        &ALU::op_sub,
        &ALU::op_and,
        &ALU::op_xor,
        &ALU::op_or
        // When you add new operations (e.g., SUB, AND), just add their method pointers here!
    };

public:
    // Setters
    void set_op1(uint16_t val) { _op1 = val; }
    void set_op2(uint16_t val) { _op2 = val; }
    
    void set_flag(ALU_Flags flag) { flags[get_flag_index(flag)] = true; }
    
    void clear_flag(ALU_Flags flag) { flags[get_flag_index(flag)] = false; }

    void clear_all_flags(void) {
        for(size_t i=0; i<_num_flags; i++)
            flags[i] = false;
    }

    // Getters
    uint16_t get_res() const { return _res; }
    
    bool get_flag(ALU_Flags flag) const { return flags[get_flag_index(flag)]; }

    void perform_operation(ALU_Op op, uint8_t num_bits) {
        _num_bits = num_bits;
        (this->*alu_operations[static_cast<size_t>(op)])();
    }
};
constexpr std::array<ALU::AluMethodPtr, static_cast<size_t>(ALU_Op::COUNT)> ALU::alu_operations;