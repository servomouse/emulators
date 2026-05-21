#include <iostream>
#include <vector>
#include <cstdint>
#include "addr_decoder.hpp"
#include "memory.hpp"
#include "cpu.hpp"
#include "z80_utils.hpp"
#include "z80_io.hpp"

/** Memory map:
 * 0x0000-0x3FFF ROM
 * 0x4000-0x57FF Screen memory
 * 0x5800-0x5AFF Screen memory color data)
 * 0x5B00-0x5Bff Printer buffer
 * 0x5C00-0x5CBF System variables
 * 0x5CC0-0x5CCA Reserved
 * 0x5CCB-0xFF57 Available memory between PROG and RAMTOP)
 * 0xFF58-0xFFFF Reserved
 * 
 * Flags register:
 * 7 6 5  4 3  2   1 0
 * S Z F5 H F3 P\V N C
 * 
 * Bit 0 - C (Carry): Set (1) if the result of an addition exceeds 255 or a subtraction requires a borrow.
 * Bit 1 - N (Add/Subtract): Set (1) if the last operation was a subtraction.
 * Bit 2 - P/V (Parity/Overflow):
 *         Parity: Set (1) if the parity of the result is even.
 *         Overflow: Set (1) if a two's complement operation overflows (result too large/small for 8 bits).
 * Bit 3 - F3 (Undocumented): Copies bit 3 of the result.
 * Bit 4 - H (Half Carry): Set (1) if there is a carry from bit 3 to bit 4 (crucial for BCD).
 * Bit 5 - F5 (Undocumented): Copies bit 5 of the result.
 * Bit 6 - Z (Zero): Set (1) if the result of an operation is zero.
 * Bit 7 - S (Sign): Copies the MSB (bit 7) of the result. Set (1) if negative, Reset (0) if positive.
 * User manual, page 65
 */

enum class ALU_Op {
    ADD = 0,
    ADC,
    INC,
    SUB,
    SBC,
    DEC,
    AND,
    OR,
    XOR,
    RL,
    RLC,
    RR,
    RRC
};

enum class FlagName {
    C = 0,
    N,
    P_V,
    F3,
    H,
    F5,
    Z,
    S
};

enum class RegName {
    A = 0, F, B, C, D, E, H, L,
    AF, BC, DE, HL,
    _A, _F, _B, _C, _D, _E, _H, _L,
    _AF, _BC, _DE, _HL,
    I,
    R,
    IX,
    IY,
    PC,
    SP,
    TC  // Tick counter - not from spec
};

class Z80Registers {
private:
    uint8_t A, F, B, C, D, E, H, L;         // Main registers
    uint8_t _A, _F, _B, _C, _D, _E, _H, _L; // Alternate registers

    uint8_t I;  // Interrupt Page Address
    uint8_t R;  // Memory Refresh
    uint16_t IX;
    uint16_t IY;
    uint16_t PC;
    uint16_t SP;
    uint32_t TC;

public:
    Z80Registers() { reset(); }

    void reset() {
        A = F = B = C = D = E = H = L = 0;
        _A = _F = _B = _C = _D = _E = _H = _L = 0;
        I = R = 0;
        IX = IY = PC = SP = 0;
    }

    uint16_t get_flag(FlagName f_name) const {
        uint16_t f_reg = get_reg(RegName::F);
        uint16_t value = ((1<<static_cast<int>(f_name)) & f_reg) == 0? 0: 1;
        return value;
    }

    void set_flag(FlagName f_name) {
        uint16_t f_reg = get_reg(RegName::F);
        uint16_t value = (1<<static_cast<int>(f_name)) | f_reg;
        set_reg(RegName::F, value);
    }

    void clear_flag(FlagName f_name) {
        uint16_t f_reg = get_reg(RegName::F);
        uint16_t value = f_reg & ~(1<<static_cast<int>(f_name));
        set_reg(RegName::F, value);
    }

    void update_flag(FlagName f_name, bool val) {
        if (val) {
            set_flag(f_name);
        } else {
            clear_flag(f_name);
        }
    }

    uint16_t get_reg(RegName name) const {
        uint16_t value = 0;
        switch (name) {
            // Main set
            case RegName::A:  value = A; break;
            case RegName::F:  value = F; break;
            case RegName::AF:  value = (A<<8) | F; break;
            case RegName::B:  value = B; break;
            case RegName::C:  value = C; break;
            case RegName::BC:  value = (B<<8) | C; break;
            case RegName::D:  value = D; break;
            case RegName::E:  value = E; break;
            case RegName::DE:  value = (D<<8) | E; break;
            case RegName::H:  value = H; break;
            case RegName::L:  value = L; break;
            case RegName::HL:  value = (H<<8) | L; break;
            // Alternate set
            case RegName::_A:  value = _A; break;
            case RegName::_F:  value = _F; break;
            case RegName::_AF:  value = (_A<<8) | _F; break;
            case RegName::_B:  value = _B; break;
            case RegName::_C:  value = _C; break;
            case RegName::_BC:  value = (_B<<8) | _C; break;
            case RegName::_D:  value = _D; break;
            case RegName::_E:  value = _E; break;
            case RegName::_DE:  value = (_D<<8) | _E; break;
            case RegName::_H:  value = _H; break;
            case RegName::_L:  value = _L; break;
            case RegName::_HL:  value = (_H<<8) | _L; break;
            // 16-bit registers
            case RegName::I: value = I; break;
            case RegName::R: value = R; break;
            case RegName::IX: value = IX; break;
            case RegName::IY: value = IY; break;
            case RegName::PC: value = PC; break;
            case RegName::SP: value = SP; break;
            case RegName::TC: value = TC; break;
            default: value = 0;
        }
        // printf("Reading register %d, value: 0x%X\n", name, value);
        return value;
    }

    bool is_reg_16b(RegName reg_name) const {
        switch (reg_name) {
            // Main set
            case RegName::A:
            case RegName::F: return false;
            case RegName::AF: return true;
            case RegName::B:
            case RegName::C: return false;
            case RegName::BC: return true;
            case RegName::D:
            case RegName::E: return false;
            case RegName::DE: return true;
            case RegName::H:
            case RegName::L: return false;
            case RegName::HL: return true;
            // Alternate set
            case RegName::_A:
            case RegName::_F: return false;
            case RegName::_AF: return true;
            case RegName::_B:
            case RegName::_C: return false;
            case RegName::_BC: return true;
            case RegName::_D:
            case RegName::_E: return false;
            case RegName::_DE: return true;
            case RegName::_H:
            case RegName::_L: return false;
            case RegName::_HL: return true;
            // 16-bit registers
            case RegName::I:
            case RegName::R:
            case RegName::IX:
            case RegName::IY:
            case RegName::PC:
            case RegName::SP:
            case RegName::TC: return true;
            default:
                printf("Error: Unknown register: %d", reg_name);
                return false;
        }
    }

    void set_reg(RegName name, uint16_t value) {
        // printf("Setting register %d to 0x%X\n", name, value);
        switch (name) {
            // Main set
            case RegName::A:  A = value&0xFF; break;
            case RegName::F:  F = value&0xFF; break;
            case RegName::AF: {F = value&0xFF; A = value>>8; break;}
            case RegName::B:  B = value&0xFF; break;
            case RegName::C:  C = value&0xFF; break;
            case RegName::BC: {C = value&0xFF; B = value>>8; break;}
            case RegName::D:  D = value&0xFF; break;
            case RegName::E:  E = value&0xFF; break;
            case RegName::DE: {E = value&0xFF; D = value>>8; break;}
            case RegName::H:  H = value&0xFF; break;
            case RegName::L:  L = value&0xFF; break;
            case RegName::HL: {L = value&0xFF; H = value>>8; break;}
            // Alternate set
            case RegName::_A:  _A = value&0xFF; break;
            case RegName::_F:  _F = value&0xFF; break;
            case RegName::_AF: {_F = value&0xFF; _A = value>>8; break;}
            case RegName::_B:  _B = value&0xFF; break;
            case RegName::_C:  _C = value&0xFF; break;
            case RegName::_BC: {_C = value&0xFF; _B = value>>8; break;}
            case RegName::_D:  _D = value&0xFF; break;
            case RegName::_E:  _E = value&0xFF; break;
            case RegName::_DE: {_E = value&0xFF; _D = value>>8; break;}
            case RegName::_H:  _H = value&0xFF; break;
            case RegName::_L:  _L = value&0xFF; break;
            case RegName::_HL: {_L = value&0xFF; _H = value>>8; break;}
            // 16-bit registers
            case RegName::I: I = value; break;
            case RegName::R: R = value; break;
            case RegName::IX: IX = value; break;
            case RegName::IY: IY = value; break;
            case RegName::PC: PC = value; break;
            case RegName::SP: SP = value; break;
            case RegName::TC: TC = value; break;
        }
    }

    void inc_reg(RegName name) {
        set_reg(name, get_reg(name)+1);
    }

    void dec_reg(RegName name) {
        set_reg(name, get_reg(name)-1);
    }
};

class Z80CPU : public CPU {
    Z80Registers regs;
    bool interrupts_enabled;
public:
    Z80CPU(AddressDecoder* mem, AddressDecoder* io) 
        : CPU(mem, io) {
        reset();
    }

    void reset() override {
        regs.reset();
        std::cout << "CPU Reset performed.\n";
    }

    uint16_t mem_read_16b(uint16_t addr) {
        return (mem_bus->read(addr+1) << 8) + mem_bus->read(addr);
    }

    void mem_write_16b(uint16_t addr, uint16_t val) {
        mem_bus->write(addr+1, val >> 8);
        mem_bus->write(addr, val & 0xFF);
    }

    void push_reg(RegName reg_name) {
        // Decrement SP, then write the value
        if (regs.is_reg_16b(reg_name)) {
            uint16_t sp = regs.get_reg(RegName::SP)-2;
            regs.set_reg(RegName::SP, sp);
            uint16_t val = regs.get_reg(reg_name);
            mem_write_16b(sp, val);
            printf("\tStack operation::: PUSH 16b 0x%04X, new SP: 0x%04X\n", val, sp);
        } else {
            uint16_t sp = regs.get_reg(RegName::SP)-1;
            regs.set_reg(RegName::SP, sp);
            uint8_t val = regs.get_reg(reg_name);
            mem_bus->write(sp, regs.get_reg(reg_name));
            printf("\tStack operation::: PUSH 8b 0x%02X, new SP: 0x%04X\n", val, sp);
        }
    }

    void pop_reg(RegName reg_name) {
        // Read the value, then increment SP
        if (regs.is_reg_16b(reg_name)) {
            uint16_t sp = regs.get_reg(RegName::SP);
            uint16_t val = mem_read_16b(sp);
            regs.set_reg(reg_name, val);
            sp += 2;
            regs.set_reg(RegName::SP, sp);
            printf("\tStack operation::: POP 16b 0x%04X, new SP: 0x%04X\n", val, sp);
        } else {
            uint16_t sp = regs.get_reg(RegName::SP);
            uint8_t val = mem_bus->read(sp);
            regs.set_reg(reg_name, val);
            sp += 1;
            regs.inc_reg(RegName::SP);
            printf("\tStack operation::: POP 8b 0x%02X, new SP: 0x%04X\n", val, sp);
        }
    }
    
    uint16_t ALU(uint16_t op1, uint16_t op2, ALU_Op op, bool is_16b) {
        // ADD,
        // ADC,
        // INC,
        // SUB,
        // SBC,
        // DEC,
        // AND,
        // OR,
        // XOR,
        // RL,
        // RLC,
        // RR,
        // RRC
        
        // C = 0,
        // N,
        // P_V,
        // F3,
        // H,
        // F5,
        // Z,
        // S
        switch(op) {
            case ALU_Op::ADD:
            case ALU_Op::ADC:
            case ALU_Op::INC:
            {
                uint16_t carry_in = 0, oveflow = 0, half_carry = 0, z = 0, s = 0, res = 0;
                if (op == ALU_Op::ADC) {
                    carry_in = regs.get_flag(FlagName::C);
                }
                if (is_16b) {
                    ;
                } else {
                    uint16_t temp1 = op1&0x0F + op2&0x0F + carry_in;
                    if (temp1 > 0x0F) half_carry = 1;
                    uint16_t temp2 = op1&0xF0 + op2&0xF0 + (half_carry << 4);
                    res = temp1 | temp2;
                }
            }
            break;
        }
        return 0;
    }

    uint16_t z80_add(uint16_t a, uint16_t b, bool use_carry, bool is_16b) {
        uint16_t carry_in = (use_carry && regs.get_flag(FlagName::C)) ? 1 : 0;
        uint32_t result = (uint32_t)a + b + carry_in;
        
        // Mask for 8-bit vs 16-bit operation
        uint16_t mask = is_16b ? 0xFFFF : 0x00FF;
        uint16_t res_masked = (uint16_t)(result & mask);

        if(!is_16b) {
            // Sign Flag (S): Copy of the MSB of the result
            if ((res_masked >> (is_16b ? 15 : 7)) & 1)
                regs.set_flag(FlagName::S);
            else
                regs.clear_flag(FlagName::S);

            // Zero Flag (Z): Set if result is 0
            if (res_masked == 0)
                regs.set_flag(FlagName::Z);
            else
                regs.clear_flag(FlagName::Z);
        }

        // 3. Half Carry (H): Set if carry from bit 3 (8-bit)
        if (is_16b) {
            // Z80 standard 16-bit ADD (ADD HL, rr) does NOT use carry
            if (((a ^ b ^ res_masked) & 0x1000) != 0)
                regs.set_flag(FlagName::H);
            else
                regs.clear_flag(FlagName::H);
        } else {
            if (((a ^ b ^ res_masked) & 0x10) != 0)
                regs.set_flag(FlagName::H);
            else
                regs.clear_flag(FlagName::H);
        }

        // Carry Flag (C): Set if result exceeds mask
        if (result > mask)
            regs.set_flag(FlagName::C);
        else
            regs.clear_flag(FlagName::C);

        regs.clear_flag(FlagName::N);   // Add/Sub (N): Always 0 for addition

        if(!is_16b) {
            // Parity/Overflow (P/V): Set on signed overflow
            // Overflow = (SignA == SignB) && (SignResult != SignA)
            bool signA = (a >> (is_16b ? 15 : 7)) & 1;
            bool signB = (b >> (is_16b ? 15 : 7)) & 1;
            bool signRes = regs.get_flag(FlagName::S);
            if ((signA == signB) && (signRes != signA))
                regs.set_flag(FlagName::P_V);
            else
                regs.clear_flag(FlagName::P_V);
        }

        return res_masked;
    }

    bool execute_opcode(uint8_t opcode) {
        uint16_t pc = regs.get_reg(RegName::PC);
        uint16_t flags_in = regs.get_reg(RegName::F);
        uint8_t num_bytes_read = 1;
        switch(opcode) {
            case 0x00: {// 0x00     (NOP         - No Operation), cycles: 4
                printf("0x%04X: NOP | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                break;
            }
            case 0x01: {// 0x11 N N (LD BC, NN   - Load NN into BC), cycles: 10
                // Does not affect flags
                uint16_t new_val = mem_read_16b(pc+1);
                regs.set_reg(RegName::BC, new_val);
                printf("0x%04X: LD BC, 0x%04X || 0x%02X\n", pc, new_val, opcode);
                num_bytes_read += 2;
                break;
            }
            case 0x02: {// 0x02     (LD (BC), A  - Stores A into the memory location pointed to by BC), cycles: 7
                // Does not affect flags
                uint16_t addr = regs.get_reg(RegName::BC);
                uint16_t a = regs.get_reg(RegName::A);
                printf("0x%04X: LD (0x%04X), A || 0x%02X\n", pc, addr, opcode);
                mem_bus->write(addr, a);
                break;
            }
            case 0x03: {// 0x03     (INC BC      - Adds one to BC), cycles: 6
                // Does not affect flags
                uint16_t val = regs.get_reg(RegName::BC);
                printf("0x%04X: INC BC | 0x%02X 0x%02X\n", pc, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::BC, val+1);
                break;
            }
            case 0x04: {// 0x04     (INC B       - Add 1 to B), cycles: 4
                // C unaffected
                // N reset
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t val = regs.get_reg(RegName::B);
                uint16_t res = (val + 1) & 0xFF;
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(val, 1, res, false, false));
                regs.update_flag(FlagName::H, calculate_half_carry(val, 1, 0, false, false));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));

                printf("0x%04X: INC B :: (0x%02X -> 0x%02X) || 0x%02X 0x%02X\n", pc, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::B, res);
                break;
            }
            case 0x05: {// 0x05     (DEC B       - Subtract 1 from B), cycles: 4
                // C unaffected
                // N set
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t val = regs.get_reg(RegName::B);
                uint16_t res = val - 1;
                regs.set_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(val, 1, res, true, false));
                regs.update_flag(FlagName::H, calculate_half_carry(val, 1, 0, false, true));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));

                printf("0x%04X: DEC B : (0x%02X->0x%02X) || 0x%02X F=0x%02X\n", pc, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::B, res);
                break;
            }
            case 0x06: {// 0x06 N   (LD B, N     - Load N into B), cycles: 7
                // Does not affect flags
                uint16_t val = mem_bus->read(pc+1);
                printf("0x%04X: LD B, 0x%02X || 0x%02X\n", pc, val, opcode);
                regs.set_reg(RegName::B, val);
                num_bytes_read += 1;
                break;
            }
            case 0x07: {// 0x07     (RLCA        - Cyclic rotate A 1 position to the left), cycles 4
                // C as defined
                // N reset
                // P/V unaffected
                // H reset
                // Z unaffected
                // S unaffected
                // The contents of A are rotated left one bit position. The contents of bit 7 are copied to the carry flag and bit 0.
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t res = a << 1;
                if (res & 0x100) {
                    res |= 1;
                    regs.set_flag(FlagName::C);
                } else {
                    regs.clear_flag(FlagName::C);
                }
                regs.clear_flag(FlagName::N);
                regs.clear_flag(FlagName::H);
                printf("0x%04X: RLCA : (0x%02X -> 0x%02X) || 0x%02X F=0x%02X\n", pc, a, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::A, res);
                break;
            }
            case 0x08: {// 0x08     (EX AF, AF'  - Exchanges the 16-bit contents of AF and AF'), cycles 4
                // Does not affect flags
                uint16_t af = regs.get_reg(RegName::AF);
                uint16_t _af = regs.get_reg(RegName::_AF);
                printf("0x%04X: EX AF, AF' :: (0x%02X <-> 0x%02X) || 0x%02X\n", pc, af, _af, opcode);
                regs.set_reg(RegName::AF, _af);
                regs.set_reg(RegName::_AF, af);
                break;
            }
            case 0x09: {// 0x09     (ADD HL, BC  - Add BC to HL), cycles: 11
                // C as defined
                // N reset
                // P/V unaffected
                // H as defined
                // Z unaffected
                // S unaffected
                uint32_t hl = regs.get_reg(RegName::HL);
                uint32_t val = regs.get_reg(RegName::BC);
                uint32_t res = hl + val;
                regs.update_flag(FlagName::C, res > 0xFFFF);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::H, calculate_half_carry(hl, val, 0, true, false));
                printf("0x%04X: ADD HL, BC :: (0x%04X + 0x%04X = 0x%04X) || 0x%02X 0x%02X\n", pc, hl, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::HL, res);
                break;
            }
            case 0x0A: {// 0x0A     (LD A, (BC)  - Loads the value pointed to by BC into A), cycles: 7
                // Does not affect flags
                uint16_t addr = regs.get_reg(RegName::BC);
                uint16_t val = mem_bus->read(addr);
                printf("0x%04X: LD A, (BC) : (0x%02X) || 0x%02X\n", pc, val, opcode);
                regs.set_reg(RegName::A, val);
                break;
            }
            case 0x0B: {// 0x0B     (DEC BC      - Decrement BC), cycles: 6
                // 16-bit operation, does not affect flags
                regs.dec_reg(RegName::BC);
                printf("0x%04X: DEC BC || 0x%02X\n", pc, opcode);
                break;
            }
            case 0x0C: {// 0x0C     (INC C       - Add 1 to C), cycles: 4
                // C unaffected
                // N reset
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t val = regs.get_reg(RegName::C);
                uint16_t res = (val + 1) & 0xFF;
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(val, 1, res, false, false));
                regs.update_flag(FlagName::H, calculate_half_carry(val, 1, 0, false, false));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));

                printf("0x%04X: INC C :: (0x%02X -> 0x%02X) || 0x%02X 0x%02X\n", pc, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::C, res);
                break;
            }
            case 0x0D: {// 0x0D     (DEC C       - Subtracts one from C), cycles: 4
                // C unaffected
                // N set
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t c = regs.get_reg(RegName::C);
                uint16_t res = c - 1;
                regs.set_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, c==0);
                regs.update_flag(FlagName::H, calculate_half_carry(c, 1, 0, false, true));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));

                printf("0x%04X: DEC C : (0x%02X->0x%02X) || 0x%02X F=0x%02X\n", pc, c, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::C, res);
                break;
            }
            case 0x0E: {// 0x0E N   (LD C, N     - Load N into C), cycles: 7
                // Does not affect flags
                uint16_t val = mem_bus->read(pc+1);
                printf("0x%04X: LD C, 0x%02X || 0x%02X\n", pc, val, opcode);
                regs.set_reg(RegName::C, val);
                num_bytes_read += 1;
                break;
            }
            case 0x0F: {// 0x0F     (RRCA        - Cyclic rotate A 1 position to the right), cycles 4
                // C as defined
                // N reset
                // P/V unaffected
                // H reset
                // Z unaffected
                // S unaffected
                // The contents of A are rotated right one bit position. The contents of bit 1 are copied to the carry flag and bit 7.
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t res = a >> 1;
                if (a & 1) {
                    res |= 0x80;
                    regs.set_flag(FlagName::C);
                } else {
                    regs.clear_flag(FlagName::C);
                }
                regs.clear_flag(FlagName::N);
                regs.clear_flag(FlagName::H);
                printf("0x%04X: RRCA : (0x%02X -> 0x%02X) || 0x%02X 0x%02X\n", pc, a, res, opcode, flags_in);
                regs.set_reg(RegName::A, res);
                break;
            }
            case 0x10: {// 0x10 N   (DJNZ N      - Decrement B and relative jump if not zero), cycles: 13/8
                // The B register is decremented, and if not zero,
                // the signed value d is added to PC. The jump is
                // measured from the start of the instruction opcode
                // Does not affect flags
                int8_t pc_inc = static_cast<int8_t>(mem_bus->read(pc+1));
                uint16_t temp = static_cast<int32_t>(pc) + pc_inc;
                uint16_t b = regs.get_reg(RegName::B);
                uint16_t res = b - 1;
                regs.set_reg(RegName::B, res);
                if (res != 0) {
                    printf("0x%04X: DJNZ %d :: (Condition met, res: 0x%02X) || 0x%02X\n", pc, pc_inc, res, opcode);
                    regs.set_reg(RegName::PC, static_cast<uint16_t>(temp+2));
                    return true;
                } else {
                    printf("0x%04X: DJNZ %d :: (Condition didn't met, res == 0) || 0x%02X\n", pc, pc_inc, opcode);
                    num_bytes_read += 1;
                }
                break;
            }
            case 0x11: {// 0x11 N N (LD DE, NN   - Load N N into DE), cycles: 10
                // Does not affect flags
                uint16_t new_val = mem_read_16b(pc+1);
                regs.set_reg(RegName::DE, new_val);
                printf("0x%04X: LD DE, 0x%04X || 0x%02X\n", pc, new_val, opcode);
                num_bytes_read += 2;
                break;
            }
            case 0x12: {// 0x12     (LD (DE), A  - Load A into (DE)), cycles: 7
                // Does not affect flags
                uint16_t addr = regs.get_reg(RegName::DE);
                uint16_t a = regs.get_reg(RegName::A);
                printf("0x%04X: LD (DE), A : (0x%04X) || 0x%02X\n", pc, addr, opcode);
                mem_bus->write(addr, a);
                break;
            }
            case 0x13: {// 0x13     (INC DE      - Increment DE), cycles: 6
                // Does not affect flags
                regs.inc_reg(RegName::DE);
                printf("0x%04X: INC DE || 0x%02X 0x%02X\n", pc, opcode, flags_in);
                break;
            }
            case 0x14: {// 0x14     (INC D       - Add 1 to D), cycles: 4
                // C unaffected
                // N reset
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t val = regs.get_reg(RegName::D);
                uint16_t res = (val + 1) & 0xFF;
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(val, 1, res, false, false));
                regs.update_flag(FlagName::H, calculate_half_carry(val, 1, 0, false, false));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));

                printf("0x%04X: INC D :: (0x%02X -> 0x%02X) || 0x%02X 0x%02X\n", pc, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::D, res);
                break;
            }
            case 0x15: {// 0x15     (DEC D       - Subtract 1 from D), cycles: 4
                // C unaffected
                // N set
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t val = regs.get_reg(RegName::D);
                uint16_t res = val - 1;
                regs.set_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(val, 1, res, true, false));
                regs.update_flag(FlagName::H, calculate_half_carry(val, 1, 0, false, true));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));

                printf("0x%04X: DEC D :: (0x%02X->0x%02X) || 0x%02X F=0x%02X\n", pc, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::D, res);
                break;
            }
            case 0x16: {// 0x16     (LD D, N     - Load N into D), cycles: 7
                // Does not affect flags
                uint16_t val = mem_bus->read(pc+1);
                printf("0x%04X: LD D, 0x%02X || 0x%02X\n", pc, val, opcode);
                regs.set_reg(RegName::D, val);
                num_bytes_read += 1;
                break;
            }
            case 0x17: {// 0x17     (RLA         - Cyclic rotate A 1 position to the left), cycles 4
                // C as defined
                // N reset
                // P/V unaffected
                // H reset
                // Z unaffected
                // S unaffected
                // The contents of A are rotated left one bit position.
                // The contents of bit 7 are copied to the carry flag and
                // the previous contents of the carry flag are copied to bit 0.
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t res = a << 1;
                uint16_t carry_in = regs.get_flag(FlagName::C);
                if (res & 0x100) {
                    regs.set_flag(FlagName::C);
                } else {
                    regs.clear_flag(FlagName::C);
                }
                res |= carry_in;
                regs.clear_flag(FlagName::N);
                regs.clear_flag(FlagName::H);
                printf("0x%04X: RLA :: (0x%02X -> 0x%02X) || 0x%02X F=0x%02X\n", pc, a, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::A, res);
                break;
            }
            case 0x18: {// 0x18 N   (JR N        - Add signed N to PC), cycles: 12/7
                // Does not affect flags
                int8_t pc_inc = static_cast<int8_t>(mem_bus->read(pc+1));
                int16_t temp = static_cast<int16_t>(pc) + pc_inc;
                printf("0x%04X: JR 0x%02X || 0x%02X 0x%02X\n", pc, pc_inc, opcode, flags_in);
                regs.set_reg(RegName::PC, static_cast<uint16_t>(temp+2));
                return true;
            }
            case 0x19: {// 0x19     (ADD HL, DE  - Add DE to HL), cycles: 11
                // C as defined
                // N reset
                // P/V unaffected
                // H as defined
                // Z unaffected
                // S unaffected
                uint32_t hl = regs.get_reg(RegName::HL);
                uint32_t val = regs.get_reg(RegName::DE);
                uint32_t res = hl + val;
                regs.update_flag(FlagName::C, res > 0xFFFF);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::H, calculate_half_carry(hl, val, 0, true, false));
                printf("0x%04X: ADD HL, DE :: (0x%04X + 0x%04X = 0x%04X) || 0x%02X 0x%02X\n", pc, hl, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::HL, res);
                break;
            }
            case 0x1A: {// 0x1A     (LD A, (DE)  - Loads the value pointed to by DE into A), cycles: 7
                // Does not affect flags
                uint16_t addr = regs.get_reg(RegName::DE);
                uint16_t val = mem_bus->read(addr);
                printf("0x%04X: LD A, (DE) : (0x%02X) || 0x%02X\n", pc, val, opcode);
                regs.set_reg(RegName::A, val);
                break;
            }
            case 0x1B: {// 0x1B     (DEC DE      - Decrement DE), cycles: 6
                // 16-bit operation, does not affect flags
                regs.dec_reg(RegName::DE);
                printf("0x%04X: DEC DE || 0x%02X\n", pc, opcode);
                break;
            }
            case 0x1C: {// 0x1C     (INC E       - Add 1 to E), cycles: 4
                // C unaffected
                // N reset
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t val = regs.get_reg(RegName::E);
                uint16_t res = (val + 1) & 0xFF;
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(val, 1, res, false, false));
                regs.update_flag(FlagName::H, calculate_half_carry(val, 1, 0, false, false));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));

                printf("0x%04X: INC E :: (0x%02X -> 0x%02X) || 0x%02X 0x%02X\n", pc, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::E, res);
                break;
            }
            case 0x1D: {// 0x1D     (DEC E       - Subtracts one from E), cycles: 4
                // C unaffected
                // N set
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t val = regs.get_reg(RegName::E);
                uint16_t res = val - 1;
                regs.set_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(val, 1, res, true, false));
                regs.update_flag(FlagName::H, calculate_half_carry(val, 1, 0, false, true));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));

                printf("0x%04X: DEC E : (0x%02X->0x%02X) || 0x%02X F=0x%02X\n", pc, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::E, res);
                break;
            }
            case 0x1E: {// 0x1E     (LD E, N     - Load N into E), cycles: 7
                // Does not affect flags
                uint16_t val = mem_bus->read(pc+1);
                printf("0x%04X: LD E, 0x%02X || 0x%02X\n", pc, val, opcode);
                regs.set_reg(RegName::E, val);
                num_bytes_read += 1;
                break;
            }
            case 0x1F: {// 0x1F     (RRA         - Cyclic rotate A 1 position to the right), cycles 4
                // C as defined
                // N reset
                // P/V unaffected
                // H reset
                // Z unaffected
                // S unaffected
                // The contents of A are rotated right one bit position.
                // The contents of bit 0 are copied to the carry flag and
                // the previous contents of the carry flag are copied to bit 7.
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t res = a >> 1;
                uint16_t carry_in = regs.get_reg(RegName::C);
                if (a & 1) {
                    regs.set_flag(FlagName::C);
                } else {
                    regs.clear_flag(FlagName::C);
                }
                res |= carry_in;
                regs.clear_flag(FlagName::N);
                regs.clear_flag(FlagName::H);
                printf("0x%04X: RRA :: (0x%02X -> 0x%02X) || 0x%02X 0x%02X\n", pc, a, res, opcode, flags_in);
                regs.set_reg(RegName::A, res);
                break;
            }
            case 0x20: {// 0x20 N   (JR NZ, N    - If Z flag is unset, add signed N to PC), cycles: 12/7
                // Does not affect flags
                int16_t pc_inc = static_cast<int16_t>(mem_bus->read(pc+1));
                int16_t temp = static_cast<int16_t>(pc) + pc_inc;
                uint16_t zf = regs.get_flag(FlagName::Z);
                printf("0x%04X: JR NZ, 0x%02X :: (Z: %d) || 0x%02X 0x%02X\n", pc, pc_inc, zf, opcode, flags_in);
                if (!zf) {
                    regs.set_reg(RegName::PC, static_cast<uint16_t>(temp+2));
                    return true;
                } else {
                    num_bytes_read += 1;
                }
                break;
            }
            case 0x21: {// 0x21 N N (LD HL, NN   - Load N N into HL), cycles: 10
                // Does not affect flags
                uint16_t new_val = mem_read_16b(pc+1);
                regs.set_reg(RegName::HL, new_val);
                printf("0x%04X: LD HL, 0x%04X || 0x%02X\n", pc, new_val, opcode);
                num_bytes_read += 2;
                break;
            }
            case 0x22: {// 0x22 N N (LD (NN), HL  - Stores HL into the memory at address (NN)), cycles: 16
                // Does not affect flags
                uint16_t addr = mem_read_16b(pc+1);
                printf("0x%04X: LD 0x%04X, HL || 0x%02X\n", pc, addr, opcode);
                mem_write_16b(addr, regs.get_reg(RegName::HL));
                num_bytes_read += 2;
                break;
            }
            case 0x23: {// 0x23     (INC HL), cycles: 6
                // Does not affect flags
                uint16_t val = regs.get_reg(RegName::HL);
                printf("0x%04X: INC HL || 0x%02X 0x%02X\n", pc, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::HL, val+1);
                break;
            }
            case 0x24: {// 0x24     (INC H       - Add 1 to H), cycles: 4
                // C unaffected
                // N reset
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t val = regs.get_reg(RegName::H);
                uint16_t res = (val + 1) & 0xFF;
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(val, 1, res, false, false));
                regs.update_flag(FlagName::H, calculate_half_carry(val, 1, 0, false, false));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));

                printf("0x%04X: INC H :: (0x%02X -> 0x%02X) || 0x%02X 0x%02X\n", pc, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::H, res);
                break;
            }
            case 0x25: {// 0x25     (DEC H       - Subtract 1 from H), cycles: 4
                // C unaffected
                // N set
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t h = regs.get_reg(RegName::H);
                uint16_t res = h - 1;
                regs.set_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, h==0);
                regs.update_flag(FlagName::H, calculate_half_carry(h, 1, 0, false, true));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));

                printf("0x%04X: DEC H : (0x%02X->0x%02X) || 0x%02X F=0x%02X\n", pc, h, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::H, res);
                break;
            }
            case 0x26: {// 0x26     (LD H, N     - Load N into H), cycles: 7
                // Does not affect flags
                uint16_t val = mem_bus->read(pc+1);
                printf("0x%04X: LD H, 0x%02X | 0x%02X 0x%02X\n", pc, val, opcode, flags_in);
                regs.set_reg(RegName::H, val);
                num_bytes_read += 1;
                break;
            }
            case 0x27: {// 0x27     (DAA         - Adjusts A for BCD addition and subtraction operations), cycles: 4
                // C exceptional
                // N unaffected
                // P/V detects parity
                // H exceptional
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t res = a;
                uint8_t correction = 0;
                uint8_t flag_C = regs.get_flag(FlagName::C);
                uint8_t old_C = flag_C;
                uint8_t flag_N = regs.get_flag(FlagName::N);
                uint8_t flag_H = regs.get_flag(FlagName::H);

                uint8_t low_nibble = res & 0x0F;
                if ((low_nibble > 9) || flag_H) {
                    if (flag_N == 0) res += 0x6;
                    else             res -= 0x6;
                    regs.set_flag(FlagName::H);
                }
                uint8_t high_nibble = res >> 4;
                if ((high_nibble > 9) || flag_C) {
                    if (flag_N == 0) res += 0x60;
                    else             res -= 0x60;
                    regs.set_flag(FlagName::C);
                }

                // Update remaining flags
                regs.update_flag(FlagName::S, res & 0x80);
                regs.update_flag(FlagName::Z, res == 0);
                regs.update_flag(FlagName::P_V, z80_parity_flag(res));

                printf("0x%04X: DAA : (0x%02X -> 0x%02X) || 0x%02X F=0x%02X\n", pc, a, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::A, res);
                break;
            }
            case 0x28: {// 0x28 N   (JR Z, N     - If Z flag is set, add signed N to PC), cycles: 12/7
                // Does not affect flags
                int16_t pc_inc = static_cast<int16_t>(mem_bus->read(pc+1));
                int16_t temp = static_cast<int16_t>(pc) + pc_inc;
                uint16_t zf = regs.get_flag(FlagName::Z);
                printf("0x%04X: JR Z, 0x%02X :: (Z: %d) || 0x%02X 0x%02X\n", pc, pc_inc, zf, opcode, flags_in);
                if (zf) {
                    regs.set_reg(RegName::PC, static_cast<uint16_t>(temp+2));
                    return true;
                } else {
                    num_bytes_read += 1;
                }
                break;
            }
            case 0x29: {// 0x29     (ADD HL, HL  - Add HL to HL), cycles: 11
                // C as defined
                // N reset
                // P/V unaffected
                // H as defined
                // Z unaffected
                // S unaffected
                uint32_t hl = regs.get_reg(RegName::HL);
                uint32_t val = hl;
                uint32_t res = hl + val;
                regs.update_flag(FlagName::C, res > 0xFFFF);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::H, calculate_half_carry(hl, val, 0, true, false));
                printf("0x%04X: ADD HL, HL :: (0x%04X + 0x%04X = 0x%04X) || 0x%02X 0x%02X\n", pc, hl, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::HL, res);
                break;
            }
            case 0x2A: {// 0x2A     (LD HL, (NN) - Loads the value pointed to by nn into HL.), cycles: 16
                // Does not affect flags
                uint16_t addr = mem_read_16b(pc+1);
                uint16_t val = mem_read_16b(addr);
                regs.set_reg(RegName::HL, val);
                printf("0x%04X: LD HL (0x%04X) : (value: 0x%02X) || 0x%02X\n", pc, addr, val, opcode);
                num_bytes_read += 2;
                break;
            }
            case 0x2B: {// 0x2B     (DEC (HL), cycles: 6
                // Does not affect flags
                printf("0x%04X: DEC HL | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                regs.dec_reg(RegName::HL);
                break;
            }
            case 0x2C: {// 0x2C     (INC L       - Add 1 to L), cycles: 4
                // C unaffected
                // N reset
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t val = regs.get_reg(RegName::L);
                uint16_t res = (val + 1) & 0xFF;
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(val, 1, res, false, false));
                regs.update_flag(FlagName::H, calculate_half_carry(val, 1, 0, false, false));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));

                printf("0x%04X: INC L :: (0x%02X -> 0x%02X) || 0x%02X 0x%02X\n", pc, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::L, res);
                break;
            }
            case 0x2D: {// 0x2D     (DEC L       - Subtracts one from L), cycles: 4
                // C unaffected
                // N set
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t val = regs.get_reg(RegName::L);
                uint16_t res = val - 1;
                regs.set_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(val, 1, res, true, false));
                regs.update_flag(FlagName::H, calculate_half_carry(val, 1, 0, false, true));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));

                printf("0x%04X: DEC L :: (0x%02X->0x%02X) || 0x%02X F=0x%02X\n", pc, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::L, res);
                break;
            }
            case 0x2E: {// 0x2E N   (LD L, N     - Load N into L), cycles: 7
                // Does not affect flags
                uint16_t val = mem_bus->read(pc+1);
                printf("0x%04X: LD L, 0x%02X || 0x%02X\n", pc, val, opcode);
                regs.set_reg(RegName::L, val);
                num_bytes_read += 1;
                break;
            }
            case 0x2F: {// 0x2F     (CPL     - The contents of A are inverted), cycles: 4
                // C unaffected
                // N set
                // P/V unaffected
                // H set
                // Z unaffected
                // S unaffected
                uint16_t val = regs.get_reg(RegName::A);
                uint16_t res = (~val)&0xFF;
                regs.set_flag(FlagName::N);
                regs.set_flag(FlagName::H);
                printf("0x%04X: CPL :: (0x%02X -> 0x%02X) || 0x%02X, F=0x%02X\n", pc, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::A, res);
                break;
            }
            case 0x30: {// 0x30     (JR NC, N    - If carry flag is unset, add signed N to PC), cycles: 12/7
                // Does not affect flags
                int16_t pc_inc = static_cast<int16_t>(mem_bus->read(pc+1));
                int16_t temp = static_cast<int16_t>(pc) + pc_inc;
                uint16_t cf = regs.get_flag(FlagName::C);
                printf("0x%04X: JR NC, 0x%02X :: (C: %d) || 0x%02X 0x%02X\n", pc, pc_inc, cf, opcode, flags_in);
                if(!cf) {
                    regs.set_reg(RegName::PC, static_cast<uint16_t>(temp+2));
                    return true;
                }
                num_bytes_read ++;
                break;
            }
            case 0x31: {// 0x31 N N (LD SP, NN   - Load N N into Sp), cycles: 10
                // Does not affect flags
                uint16_t new_val = mem_read_16b(pc+1);
                regs.set_reg(RegName::SP, new_val);
                printf("0x%04X: LD SP, 0x%04X || 0x%02X\n", pc, new_val, opcode);
                num_bytes_read += 2;
                break;
            }
            case 0x32: {// 0x32 N N (LD (NN), A  - Stores A into the memory at address (NN)), cycles: 13
                // Does not affect flags
                uint16_t addr = mem_read_16b(pc+1);
                printf("0x%04X: LD (0x%04X), A || 0x%02X\n", pc, addr, opcode);
                mem_bus->write(addr, regs.get_reg(RegName::A));
                num_bytes_read += 2;
                break;
            }
            case 0x33: {// 0x33     (INC SP      - Adds one to SP), cycles: 6
                // Does not affect flags
                uint16_t val = regs.get_reg(RegName::SP);
                printf("0x%04X: INC SP || 0x%02X 0x%02X\n", pc, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::SP, val+1);
                break;
            }
            case 0x34: {// 0x34     (INC (HL)    - Adds one to (HL)), cycles: 11
                // C unaffected
                // N reset
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t addr = regs.get_reg(RegName::HL);
                uint16_t val = mem_bus->read(addr);
                uint16_t res = (val + 1) & 0xFF;
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(val, 1, res, false, false));
                regs.update_flag(FlagName::H, calculate_half_carry(val, 1, 0, false, false));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));

                printf("0x%04X: INC (HL) :: (0x%02X -> 0x%02X) || 0x%02X 0x%02X\n", pc, val, res, opcode, regs.get_reg(RegName::F));
                mem_bus->write(addr, res&0xFF);
                break;
            }
            case 0x35: {// 0x35     (DEC (HL)    - Subtracts one from (HL)), cycles: 11
                // C unaffected
                // N reset
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t addr = regs.get_reg(RegName::HL);
                uint16_t val = mem_bus->read(addr);
                uint16_t res = (val - 1) & 0xFF;
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(val, 1, res, true, false));
                regs.update_flag(FlagName::H, calculate_half_carry(val, 1, 0, false, true));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));

                printf("0x%04X: DEC (HL) :: (0x%02X -> 0x%02X) || 0x%02X 0x%02X\n", pc, val, res, opcode, regs.get_reg(RegName::F));
                mem_bus->write(addr, res&0xFF);
                break;
            }
            case 0x36: {// 0x36     (LD (HL), N  - Load N into (HL)), cycles: 10
                // Does not affect flags
                uint16_t val = mem_bus->read(pc+1);
                uint16_t addr = regs.get_reg(RegName::HL);
                printf("0x%04X: LD 0x%04X, 0x%02X || 0x%02X\n", pc, addr, val, opcode);
                mem_bus->write(addr, val);
                num_bytes_read ++;
                break;
            }
            case 0x37: {// 0x37     (SCF         - Sets the carry flag), cycles: 4
                // C set
                // N reset
                // P/V unaffected
                // H reset
                // Z unaffected
                // S unaffected
                regs.set_flag(FlagName::C);
                regs.clear_flag(FlagName::N);
                regs.clear_flag(FlagName::H);
                break;
            }
            case 0x38: {// 0x38     (JR C, N     - If carry flag is set, add signed N to PC), cycles: 12/7
                // Does not affect flags
                int16_t pc_inc = static_cast<int16_t>(mem_bus->read(pc+1));
                int16_t temp = static_cast<int16_t>(pc) + pc_inc;
                uint16_t cf = regs.get_flag(FlagName::C);
                printf("0x%04X: JR NC, 0x%02X :: (C: %d) || 0x%02X\n", pc, pc_inc, cf, opcode);
                if(cf) {
                    regs.set_reg(RegName::PC, static_cast<uint16_t>(temp+2));
                    return true;
                }
                num_bytes_read ++;
                break;
            }
            case 0x39: {// 0x39     (ADD HL, SP  - Add SP to HL), cycles: 11
                // C as defined
                // N reset
                // P/V unaffected
                // H as defined
                // Z unaffected
                // S unaffected
                uint32_t hl = regs.get_reg(RegName::HL);
                uint32_t val = regs.get_reg(RegName::SP);
                uint32_t res = hl + val;
                regs.update_flag(FlagName::C, res > 0xFFFF);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::H, calculate_half_carry(hl, val, 0, true, false));
                printf("0x%04X: ADD HL, SP : (0x%04X + 0x%04X = 0x%04X) | 0x%02X 0x%02X\n", pc, hl, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::HL, res);
                break;
            }
            case 0x3A: {// 0x3A N N (LD A, (NN)  - Loads the value pointed to by nn into A.), cycles: 13
                // Does not affect flags
                uint16_t addr = mem_read_16b(pc+1);
                uint16_t val = mem_bus->read(addr);
                regs.set_reg(RegName::A, val);
                printf("0x%04X: LD A, (0x%04X) : (value: 0x%02X) || 0x%02X\n", pc, addr, val, opcode);
                num_bytes_read += 2;
                break;
            }
            case 0x3B: {// 0x3B     (DEC (SP), cycles: 6
                // Does not affect flags
                printf("0x%04X: DEC SP || 0x%02X 0x%02X\n", pc, opcode, regs.get_reg(RegName::F));
                regs.dec_reg(RegName::SP);
                break;
            }
            case 0x3C: {// 0x3C     (INC A       - Add 1 to A), cycles: 4
                // C unaffected
                // N reset
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t val = regs.get_reg(RegName::A);
                uint16_t res = (val + 1) & 0xFF;
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(val, 1, res, false, false));
                regs.update_flag(FlagName::H, calculate_half_carry(val, 1, 0, false, false));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));

                printf("0x%04X: INC A :: (0x%02X -> 0x%02X) || 0x%02X 0x%02X\n", pc, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::A, res);
                break;
            }
            case 0x3D: {// 0x3D     (DEC A       - Subtracts one from A), cycles: 4
                // C unaffected
                // N set
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t val = regs.get_reg(RegName::A);
                uint16_t res = val - 1;
                regs.set_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(val, 1, res, true, false));
                regs.update_flag(FlagName::H, calculate_half_carry(val, 1, 0, false, true));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));

                printf("0x%04X: DEC A :: (0x%02X->0x%02X) || 0x%02X F=0x%02X\n", pc, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::A, res);
                break;
            }
            case 0x3E: {// 0x3E     (LD A, N     - Load N into A), cycles: 7
                // Does not affect flags
                uint16_t val = mem_bus->read(pc+1);
                printf("0x%04X: LD A, 0x%02X | 0x%02X 0x%02X\n", pc, val, opcode, flags_in);
                regs.set_reg(RegName::A, val);
                num_bytes_read ++;
                break;
            }
            case 0x3F: {// 0x3F     (CCF         - Inverts the carry flag), cycles: 4
                // C exceptional
                // N reset
                // P/V unaffected
                // H exceptional
                // Z unaffected
                // S unaffected
                uint16_t old_carry = regs.get_flag(FlagName::C);
                regs.update_flag(FlagName::C, !old_carry);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::H, old_carry);
                break;
            }
            case 0x40: {// 0x40     (LD B, B     - Load B into B), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD B, B || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::B, regs.get_reg(RegName::B));
                break;
            }
            case 0x41: {// 0x41     (LD B, C     - Load C into B), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD B, C || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::B, regs.get_reg(RegName::C));
                break;
            }
            case 0x42: {// 0x42     (LD B, D     - Load D into B), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD B, D || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::B, regs.get_reg(RegName::D));
                break;
            }
            case 0x43: {// 0x43     (LD B, E     - Load E into B), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD B, E || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::B, regs.get_reg(RegName::E));
                break;
            }
            case 0x44: {// 0x44     (LD B, H     - Load H into B), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD B, H || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::B, regs.get_reg(RegName::H));
                break;
            }
            case 0x45: {// 0x45     (LD B, L     - Load L into B), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD B, L || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::B, regs.get_reg(RegName::L));
                break;
            }
            case 0x46: {// 0x46     (LD B, (HL)  - The contents of (HL) are loaded into B), cycles: 7
                // Does not affect flags
                uint16_t hl = regs.get_reg(RegName::HL);
                uint16_t val = mem_bus->read(hl);
                printf("0x%04X: LD B, (HL) : (0x%02X) || 0x%02X\n", pc, val, opcode);
                regs.set_reg(RegName::B, val);
                break;
            }
            case 0x47: {// 0x47     (LD B, A     - Load A into B), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD B, A | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                regs.set_reg(RegName::B, regs.get_reg(RegName::A));
                break;
            }
            case 0x48: {// 0x48     (LD C, B     - Load B into C), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD C, B || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::C, regs.get_reg(RegName::B));
                break;
            }
            case 0x49: {// 0x49     (LD C, C     - Load C into C), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD C, B || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::C, regs.get_reg(RegName::C));
                break;
            }
            case 0x4A: {// 0x4A     (LD C, D     - Load D into C), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD C, D || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::C, regs.get_reg(RegName::D));
                break;
            }
            case 0x4B: {// 0x4B     (LD C, E     - Load E into C), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD C, E || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::C, regs.get_reg(RegName::E));
                break;
            }
            case 0x4C: {// 0x4C     (LD C, H     - Load H into C), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD C, H || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::C, regs.get_reg(RegName::H));
                break;
            }
            case 0x4D: {// 0x4D     (LD C, L     - Load L into C), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD C, L || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::C, regs.get_reg(RegName::L));
                break;
            }
            case 0x4E: {// 0x4E     (LD C, (HL)  - Load (HL) into C), cycles: 7
                // Does not affect flags
                uint16_t addr = regs.get_reg(RegName::HL);
                uint16_t val = mem_bus->read(addr);
                printf("0x%04X: LD C, (HL) (0x%04X) || 0x%02X\n", pc, val, opcode);
                regs.set_reg(RegName::C, val);
                break;
            }
            case 0x4F: {// 0x4F     (LD C, A     - Load A into C), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD C, A | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                regs.set_reg(RegName::C, regs.get_reg(RegName::A));
                break;
            }
            case 0x50: {// 0x50     (LD D, B     - Load B into D), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD D, B || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::D, regs.get_reg(RegName::B));
                break;
            }
            case 0x51: {// 0x51     (LD D, C     - Load C into D), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD D, C || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::D, regs.get_reg(RegName::C));
                break;
            }
            case 0x52: {// 0x52     (LD D, D     - Load D into D), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD D, D || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::D, regs.get_reg(RegName::D));
                break;
            }
            case 0x53: {// 0x53     (LD D, E     - Load E into D), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD D, E || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::D, regs.get_reg(RegName::E));
                break;
            }
            case 0x54: {// 0x54     (LD D, H     - Load H into D), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD D, H || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::D, regs.get_reg(RegName::H));
                break;
            }
            case 0x55: {// 0x55     (LD D, L     - Load L into D), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD D, L || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::D, regs.get_reg(RegName::L));
                break;
            }
            case 0x56: {// 0x56     (LD D, (HL)  - Load (HL) into D), cycles: 7
                // Does not affect flags
                uint16_t hl = regs.get_reg(RegName::HL);
                uint16_t val = mem_bus->read(hl);
                printf("0x%04X: LD D, (HL) : (0x%04X) || 0x%02X\n", pc, val, opcode);
                regs.set_reg(RegName::D, val);
                break;
            }
            case 0x57: {// 0x55     (LD D, A     - Load A into D), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD D, A || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::D, regs.get_reg(RegName::A));
                break;
            }
            case 0x58: {// 0x58     (LD E, B     - Load B into E), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD E, B || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::E, regs.get_reg(RegName::B));
                break;
            }
            case 0x59: {// 0x59     (LD E, C     - Load C into E), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD E, C || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::E, regs.get_reg(RegName::C));
                break;
            }
            case 0x5A: {// 0x5A     (LD E, D     - Load D into E), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD E, D || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::E, regs.get_reg(RegName::D));
                break;
            }
            case 0x5B: {// 0x5B     (LD E, E     - Load E into E), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD E, E || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::E, regs.get_reg(RegName::E));
                break;
            }
            case 0x5C: {// 0x5C     (LD E, H     - Load H into E), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD E, H || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::E, regs.get_reg(RegName::H));
                break;
            }
            case 0x5D: {// 0x5C     (LD E, L     - Load L into E), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD E, L | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                regs.set_reg(RegName::E, regs.get_reg(RegName::L));
                break;
            }
            case 0x5E: {// 0x5E     (LD E, (HL)  - Load (HL) into E), cycles: 7
                // Does not affect flags
                uint16_t hl = regs.get_reg(RegName::HL);
                uint16_t val = mem_bus->read(hl);
                printf("0x%04X: LD E, (HL) : (0x%04X) || 0x%02X\n", pc, val, opcode);
                regs.set_reg(RegName::E, val);
                break;
            }
            case 0x5F: {// 0x5F     (LD E, A     - Load A into E), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD E, A || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::E, regs.get_reg(RegName::A));
                break;
            }
            case 0x60: {// 0x60     (LD H, B     - Load B into H), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD H, B || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::H, regs.get_reg(RegName::B));
                break;
            }
            case 0x61: {// 0x61     (LD H, C     - Load C into H), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD H, C || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::H, regs.get_reg(RegName::C));
                break;
            }
            case 0x62: {// 0x62     (LD H, D     - Load D into H), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD H, D || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::H, regs.get_reg(RegName::D));
                break;
            }
            case 0x63: {// 0x63     (LD H, E     - Load E into H), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD H, E || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::H, regs.get_reg(RegName::E));
                break;
            }
            case 0x64: {// 0x62     (LD H, H     - Load H into H), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD H, H || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::H, regs.get_reg(RegName::H));
                break;
            }
            case 0x65: {// 0x62     (LD H, L     - Load L into H), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD H, L || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::H, regs.get_reg(RegName::L));
                break;
            }
            case 0x66: {// 0x66     (LD H, (HL)  - Load (HL) into H), cycles: 7
                // Does not affect flags
                uint16_t hl = regs.get_reg(RegName::HL);
                uint16_t val = mem_bus->read(hl);
                printf("0x%04X: LD H, (HL) (0x%04X) | 0x%02X 0x%02X\n", pc, val, opcode, flags_in);
                regs.set_reg(RegName::H, val);
                break;
            }
            case 0x67: {// 0x67     (LD H, A     - Load A into H), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD H, A || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::H, regs.get_reg(RegName::A));
                break;
            }
            case 0x68: {// 0x68     (LD L, B     - Load B into L), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD L, B || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::L, regs.get_reg(RegName::B));
                break;
            }
            case 0x69: {// 0x69     (LD L, C     - Load C into L), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD L, C || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::L, regs.get_reg(RegName::C));
                break;
            }
            case 0x6A: {// 0x6A     (LD L, D     - Load D into L), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD L, D || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::L, regs.get_reg(RegName::D));
                break;
            }
            case 0x6B: {// 0x6B     (LD L, E     - Load E into L), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD L, E || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::L, regs.get_reg(RegName::E));
                break;
            }
            case 0x6C: {// 0x6C     (LD L, H     - Load H into L), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD L, H || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::L, regs.get_reg(RegName::H));
                break;
            }
            case 0x6D: {// 0x6D     (LD L, L     - Load L into L), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD L, L || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::L, regs.get_reg(RegName::L));
                break;
            }
            case 0x6E: {// 0x6E     (LD L, (HL)  - Load (HL) into L), cycles: 7
                // Does not affect flags
                uint16_t hl = regs.get_reg(RegName::HL);
                uint16_t val = mem_bus->read(hl);
                printf("0x%04X: LD L, (HL) : (0x%04X) || 0x%02X\n", pc, val, opcode);
                regs.set_reg(RegName::L, val);
                break;
            }
            case 0x6F: {// 0x6F     (LD L, A     - Load A into L), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD L, A | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                regs.set_reg(RegName::B, regs.get_reg(RegName::L));
                break;
            }
            case 0x76: {// 0x76     (HALT        - Stop and wait for an interrupt)
                printf("0x%04X: HALT opcode detected || 0x%02X\n", pc, opcode);
                return false;
            }
            case 0x77: {// 0x77     (LD (HL), A  - Load A into (HL)), cycles: 7
                // Does not affect flags
                uint16_t addr = regs.get_reg(RegName::HL);
                uint16_t a = regs.get_reg(RegName::A);
                printf("0x%04X: LD (0x%04X), A | 0x%02X 0x%02X\n", pc, addr, opcode, flags_in);
                mem_bus->write(addr, a);
                break;
            }
            case 0x78: {// 0x78     (LD A, B     - Load B into A), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD A, B || 0x%02X 0x%02X\n", pc, opcode, flags_in);
                regs.set_reg(RegName::A, regs.get_reg(RegName::B));
                break;
            }
            case 0x79: {// 0x79     (LD A, C     - Load C into A), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD A, C || 0x%02X 0x%02X\n", pc, opcode, flags_in);
                regs.set_reg(RegName::A, regs.get_reg(RegName::C));
                break;
            }
            case 0x7A: {// 0x7A     (LD A, D     - Load D into A), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD A, D | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                regs.set_reg(RegName::A, regs.get_reg(RegName::D));
                break;
            }
            case 0x7B: {// 0x7B     (LD A, E     - Load E into A), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD A, E || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::A, regs.get_reg(RegName::E));
                break;
            }
            case 0x7C: {// 0x7C     (LD A, H     - Load H into A), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD A, H || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::A, regs.get_reg(RegName::H));
                break;
            }
            case 0x7D: {// 0x7D     (LD A, L     - Load L into A), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD A, L || 0x%02X\n", pc, opcode);
                regs.set_reg(RegName::A, regs.get_reg(RegName::L));
                break;
            }
            case 0x7E: {// 0x7E     (LD A, (HL)  - Load (HL) into A), cycles: 7
                // Does not affect flags
                uint16_t hl = regs.get_reg(RegName::HL);
                uint16_t val = mem_bus->read(hl);
                printf("0x%04X: LD A, (HL) : (0x%04X) || 0x%02X\n", pc, val, opcode);
                regs.set_reg(RegName::A, val);
                break;
            }
            case 0x82: {// 0x82     (ADD A, D    - Adds D to A), cycles: 4
                // C as defined
                // N reset
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t d = regs.get_reg(RegName::D);
                uint16_t res = a + d;
                regs.update_flag(FlagName::C, res > 0xFF);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(a, d, res, false, false));
                regs.update_flag(FlagName::H, calculate_half_carry(a, d, 0, false, false));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                printf("0x%04X: ADD A, D : (0x%02X+0x%02X=0x%02X) || 0x%02X F=0x%02X\n", pc, a, d, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::A, res);
                break;
            }
            case 0x8D: {// 0x8D     (ADC A, L    - Adds L and the carry flag to A), cycles: 4
                // C as defined
                // N reset
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t l = regs.get_reg(RegName::L);
                uint16_t res = a + l;
                if (regs.get_flag(FlagName::C)) {
                    res += 1;
                }
                regs.update_flag(FlagName::C, res > 0xFF);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(a, l, res, false, false));
                regs.update_flag(FlagName::H, calculate_half_carry(a, l, 0, false, false));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                printf("0x%04X: ADC A, L || 0x%02X F=0x%02X\n", pc, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::A, res);
                break;
            }
            case 0xA0: {// 0xA0     (AND A, B    - Bitwise AND on A with B), cycles: 4
                // C reset
                // N reset
                // P/V detects parity
                // H set
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t b = regs.get_reg(RegName::B);
                uint16_t res = a & b;
                regs.clear_flag(FlagName::C);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, z80_parity_flag(res));
                regs.set_flag(FlagName::H);
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                printf("0x%04X: AND A, B : (0x%02X & 0x%02X = 0x%02X) | 0x%02X 0x%02X\n", pc, a, b, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::A, res);
                break;
            }
            case 0xA1: {// 0xA1     (AND A, C    - Bitwise AND on A with C), cycles: 4
                // C reset
                // N reset
                // P/V detects parity
                // H set
                // Z as defined
                // S as defined
                printf("0x%04X: AND A, C | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t c = regs.get_reg(RegName::C);
                uint16_t res = a & c;
                regs.clear_flag(FlagName::C);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, z80_parity_flag(res));
                regs.set_flag(FlagName::H);
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                regs.set_reg(RegName::A, res);
                break;
            }
            case 0xA7: {// 0xA7     (AND A, A    - Bitwise AND on A with A), cycles: 4
                // C reset
                // N reset
                // P/V detects parity
                // H set
                // Z as defined
                // S as defined
                printf("0x%04X: AND A, A | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                uint16_t a = regs.get_reg(RegName::A);
                regs.clear_flag(FlagName::C);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, z80_parity_flag(a));
                regs.set_flag(FlagName::H);
                regs.update_flag(FlagName::Z, z80_zero_flag(a));
                regs.update_flag(FlagName::S, z80_sign_flag(a, false));
                break;
            }
            case 0xA8: {// 0xA8     (XOR A, B    - Bitwise XOR on A with B), cycles: 4
                // C reset
                // N reset
                // P/V detects parity
                // H reset
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t b = regs.get_reg(RegName::B);
                uint16_t res = a ^ b;
                regs.clear_flag(FlagName::C);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, z80_parity_flag(res));
                regs.clear_flag(FlagName::H);
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                regs.set_reg(RegName::A, res);
                printf("0x%04X: XOR A, B :: (0x%02X^0x%02X=0x%02X) || 0x%02X 0x%02X\n", pc, a, b, res, opcode, regs.get_reg(RegName::F));
                break;
            }
            case 0xA9: {// 0xA9     (XOR A, C    - Bitwise XOR on A with C), cycles: 4
                // C reset
                // N reset
                // P/V detects parity
                // H reset
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t c = regs.get_reg(RegName::C);
                uint16_t res = a ^ c;
                regs.clear_flag(FlagName::C);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, z80_parity_flag(res));
                regs.clear_flag(FlagName::H);
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                regs.set_reg(RegName::A, res);
                printf("0x%04X: XOR A, C :: (0x%02X^0x%02X=0x%02X) || 0x%02X 0x%02X\n", pc, a, c, res, opcode, regs.get_reg(RegName::F));
                break;
            }
            case 0xAA: {// 0xAA     (XOR A, D    - Bitwise XOR on A with D), cycles: 4
                // C reset
                // N reset
                // P/V detects parity
                // H reset
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t val = regs.get_reg(RegName::D);
                uint16_t res = a ^ val;
                regs.clear_flag(FlagName::C);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, z80_parity_flag(res));
                regs.clear_flag(FlagName::H);
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                regs.set_reg(RegName::A, res);
                printf("0x%04X: XOR A, D :: (0x%02X^0x%02X=0x%02X) || 0x%02X 0x%02X\n", pc, a, val, res, opcode, regs.get_reg(RegName::F));
                break;
            }
            case 0xAB: {// 0xAB     (XOR A, E    - Bitwise XOR on A with E), cycles: 4
                // C reset
                // N reset
                // P/V detects parity
                // H reset
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t val = regs.get_reg(RegName::E);
                uint16_t res = a ^ val;
                regs.clear_flag(FlagName::C);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, z80_parity_flag(res));
                regs.clear_flag(FlagName::H);
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                regs.set_reg(RegName::A, res);
                printf("0x%04X: XOR A, E :: (0x%02X^0x%02X=0x%02X) || 0x%02X 0x%02X\n", pc, a, val, res, opcode, regs.get_reg(RegName::F));
                break;
            }
            case 0xAC: {// 0xAC     (XOR A, H    - Bitwise XOR on A with H), cycles: 4
                // C reset
                // N reset
                // P/V detects parity
                // H reset
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t val = regs.get_reg(RegName::H);
                uint16_t res = a ^ val;
                regs.clear_flag(FlagName::C);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, z80_parity_flag(res));
                regs.clear_flag(FlagName::H);
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                regs.set_reg(RegName::A, res);
                printf("0x%04X: XOR A, H :: (0x%02X^0x%02X=0x%02X) || 0x%02X 0x%02X\n", pc, a, val, res, opcode, regs.get_reg(RegName::F));
                break;
            }
            case 0xAD: {// 0xAD     (XOR A, L    - Bitwise XOR on A with L), cycles: 4
                // C reset
                // N reset
                // P/V detects parity
                // H reset
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t val = regs.get_reg(RegName::L);
                uint16_t res = a ^ val;
                regs.clear_flag(FlagName::C);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, z80_parity_flag(res));
                regs.clear_flag(FlagName::H);
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                regs.set_reg(RegName::A, res);
                printf("0x%04X: XOR A, L :: (0x%02X^0x%02X=0x%02X) || 0x%02X 0x%02X\n", pc, a, val, res, opcode, regs.get_reg(RegName::F));
                break;
            }
            case 0xAE: {// 0xAE     (XOR A, (HL) - Bitwise XOR on A with (HL)), cycles: 7
                // C reset
                // N reset
                // P/V detects parity
                // H reset
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t addr = regs.get_reg(RegName::HL);
                uint16_t val = mem_bus->read(addr);
                uint16_t res = a ^ val;
                regs.clear_flag(FlagName::C);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, z80_parity_flag(res));
                regs.clear_flag(FlagName::H);
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                regs.set_reg(RegName::A, res);
                printf("0x%04X: XOR A, (HL) : (0x%02X^0x%02X=0x%02X) || 0x%02X 0x%02X\n", pc, a, val, res, opcode, regs.get_reg(RegName::F));
                break;
            }
            case 0xAF: {// 0xAF     (XOR A, A    - Bitwise XOR on A with A), cycles: 4
                // C reset
                // N reset
                // P/V detects parity
                // H reset
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t val = regs.get_reg(RegName::B);
                uint16_t res = a ^ val;
                regs.clear_flag(FlagName::C);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, z80_parity_flag(res));
                regs.clear_flag(FlagName::H);
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                regs.set_reg(RegName::A, res);
                printf("0x%04X: XOR A, A :: (0x%02X^0x%02X=0x%02X) || 0x%02X 0x%02X\n", pc, a, val, res, opcode, regs.get_reg(RegName::F));
                break;
            }
            case 0xB0: {// 0xB0     (OR A, B), cycles: 4
                int pc_inc = or_operation(pc, opcode);
                if (pc_inc == -1) return false;
                num_bytes_read += pc_inc;
                break;
            }
            case 0xB6: {// 0xB6     (OR A, (HL)), cycles: 7
                int pc_inc = or_operation(pc, opcode);
                if (pc_inc == -1) return false;
                num_bytes_read += pc_inc;
                break;
            }
            case 0xB7: {// 0xB7     (OR A, A), cycles: 7
                int pc_inc = or_operation(pc, opcode);
                if (pc_inc == -1) return false;
                num_bytes_read += pc_inc;
                break;
            }
            case 0xBC: {// 0xBC     (CP A, H     - Substract H from A and update flags. A stays unchanged), cycles: 4
                // C as defined
                // N set
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t h = regs.get_reg(RegName::H);
                uint16_t res = a - h;
                regs.update_flag(FlagName::C, h > a);
                regs.set_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(a, h, res, true, false));
                regs.update_flag(FlagName::H, calculate_half_carry(a, h, 0, false, true));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                printf("0x%04X: CP A, H : (0x%04X - 0x%04X = 0x%04X) | 0x%02X 0x%02X\n", pc, a, h, res, opcode, regs.get_reg(RegName::F));
                break;
            }
            case 0xC0: {// 0xC0     (RET NZ - If the zero flag is unset, the top stack entry is popped into PC), cycles: 11/5
                // Does not affect flags
                if (!regs.get_flag(FlagName::Z)) {
                    printf("0x%04X: RET NZ :: (Z=0, condition met) || 0x%02X\n", pc, opcode);
                    pop_reg(RegName::PC);
                    return true;
                }
                printf("0x%04X: RET NZ :: (Z=1, condition not met) || 0x%02X\n", pc, opcode);
                break;
            }
            case 0xC1: {// 0xC1     (POP BC), cycles: 10
                // Does not affect flags
                printf("0x%04X: POP BC || 0x%02X\n", pc, opcode);
                pop_reg(RegName::BC);
                break;
            }
            case 0xC2: {// 0xC2 N N (JP NZ, N N  - Load N N into PC if Zero flag is not set), cycles: 10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                uint16_t zf = regs.get_flag(FlagName::Z);
                if (!zf) {
                    printf("0x%04X: JP NZ, 0x%04X :: (Condition met, Z: %d) || 0x%02X 0x%02X\n", pc, new_pc, zf, opcode, flags_in);
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    printf("0x%04X: JP NZ, 0x%04X :: (Condition didn't meet, Z: %d) || 0x%02X 0x%02X\n", pc, new_pc, zf, opcode, flags_in);
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xC3: {// 0xC3 N N (JP N N      - Load N N into PC), cycles: 10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                printf("0x%04X: JP 0x%04X || 0x%02X 0x%02X\n", pc, new_pc, opcode, flags_in);
                regs.set_reg(RegName::PC, new_pc);
                return true;
            }
            case 0xC4: {// 0xC4     (CALL NZ NN  - If the zero flag is unset, the current PC value plus three is pushed onto the stack, then is loaded with nn.), cycles: 17/10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                if (!regs.get_flag(FlagName::Z)) {
                    regs.set_reg(RegName::PC, pc+3);    // Increment PC to point to the next instruction
                    printf("0x%04X: CALL NZ 0x%04X :: (Z=0, condition met) || 0x%02X\n", pc, new_pc, opcode);
                    push_reg(RegName::PC);
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    printf("0x%04X: CALL NZ 0x%04X :: (Z=1, condition not met) || 0x%02X\n", pc, new_pc, opcode);
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xC5: {// 0xC5     (PUSH BC), cycles: 11
                // Does not affect flags
                printf("0x%04X: PUSH BC || 0x%02X 0x%02X, SP: 0x%04X\n", pc, opcode, flags_in, regs.get_reg(RegName::SP));
                push_reg(RegName::BC);
                break;
            }
            case 0xC6: {// 0xC6 N   (ADD A, L    - Adds N to A), cycles: 7
                // C as defined
                // N reset
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t val = mem_bus->read(pc+1);
                uint16_t res = a + val;
                regs.update_flag(FlagName::C, res > 0xFF);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(a, val, res, false, false));
                regs.update_flag(FlagName::H, calculate_half_carry(a, val, 0, false, false));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                printf("0x%04X: ADD A, 0x%02X :: (0x%02X + 0x%02X = 0x%02X) || 0x%02X F=0x%02X\n", pc, val, a, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::A, res);
                num_bytes_read ++;
                break;
            }
            case 0xC8: {// 0xC8     (RET Z - If the zero flag is set, the top stack entry is popped into PC), cycles: 11/5
                // Does not affect flags
                if (regs.get_flag(FlagName::Z)) {
                    printf("0x%04X: RET Z :: (Z=1, condition met) || 0x%02X\n", pc, opcode);
                    pop_reg(RegName::PC);
                    return true;
                }
                printf("0x%04X: RET Z :: (Z=0, condition not met) || 0x%02X\n", pc, opcode);
                break;
            }
            case 0xC9: {// 0xC9     (RET), cycles: 10
                // Does not affect flags
                printf("0x%04X: RET || 0x%02X\n", pc, opcode);
                pop_reg(RegName::PC);
                return true;
            }
            case 0xCA: {// 0xCA N N (JP Z, N N   - Load N N into PC if Zero flag is set), cycles: 10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                if (regs.get_flag(FlagName::Z)) {
                    printf("0x%04X: JP Z, 0x%04X :: (Condition met: Z==1) || 0x%02X\n", pc, new_pc, opcode);
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    printf("0x%04X: JP Z, 0x%04X :: (Condition didn't met: Z==0) || 0x%02X\n", pc, new_pc, opcode);
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xCB: {// 0xCB     (BIT Instructions, read one more byte to get the actual opcode)
                uint16_t new_opcode = mem_bus->read(pc+1);
                if (new_opcode == 0x47) {   // 0xCB 0x47 (BIT 0, A - Tests bit 0 of A), cycles: 8
                    uint16_t res = bit_operation(0, regs.get_reg(RegName::A));
                    printf("0x%04X: BIT 0, A :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x4F) {   // 0xCB 0x4F (BIT 1, A - Tests bit 1 of A), cycles: 8
                    uint16_t res = bit_operation(1, regs.get_reg(RegName::A));
                    printf("0x%04X: BIT 1, A :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x57) {   // 0xCB 0x57 (BIT 2, A - Tests bit 2 of A), cycles: 8
                    uint16_t res = bit_operation(2, regs.get_reg(RegName::A));
                    printf("0x%04X: BIT 2, A :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x5F) {   // 0xCB 0x5F (BIT 3, A - Tests bit 3 of A), cycles: 8
                    uint16_t res = bit_operation(3, regs.get_reg(RegName::A));
                    printf("0x%04X: BIT 3, A :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x67) {   // 0xCB 0x67 (BIT 4, A - Tests bit 4 of A), cycles: 8
                    uint16_t res = bit_operation(4, regs.get_reg(RegName::A));
                    printf("0x%04X: BIT 4, A :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x6F) {   // 0xCB 0x6F (BIT 5, A - Tests bit 5 of A), cycles: 8
                    uint16_t res = bit_operation(5, regs.get_reg(RegName::A));
                    printf("0x%04X: BIT 5, A :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x77) {   // 0xCB 0x77 (BIT 6, A - Tests bit 6 of A), cycles: 8
                    uint16_t res = bit_operation(6, regs.get_reg(RegName::A));
                    printf("0x%04X: BIT 6, A :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x7F) {   // 0xCB 0x7F (BIT 7, A - Tests bit 7 of A), cycles: 8
                    uint16_t res = bit_operation(7, regs.get_reg(RegName::A));
                    printf("0x%04X: BIT 7, A :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x45) {   // 0xCB 0x45 (BIT 0, L - Tests bit 0 of L), cycles: 8
                    uint16_t res = bit_operation(0, regs.get_reg(RegName::L));
                    printf("0x%04X: BIT 0, L :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x55) {   // 0xCB 0x55 (BIT 2, L - Tests bit 2 of L), cycles: 8
                    uint16_t res = bit_operation(2, regs.get_reg(RegName::L));
                    printf("0x%04X: BIT 2, L :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x65) {   // 0xCB 0x65 (BIT 4, L - Tests bit 4 of L), cycles: 8
                    uint16_t res = bit_operation(4, regs.get_reg(RegName::L));
                    printf("0x%04X: BIT 4, L :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x75) {   // 0xCB 0x75 (BIT 6, L - Tests bit 6 of L), cycles: 8
                    uint16_t res = bit_operation(6, regs.get_reg(RegName::L));
                    printf("0x%04X: BIT 6, L :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x4D) {   // 0xCB 0x4D (BIT 1, L - Tests bit 1 of L), cycles: 8
                    uint16_t res = bit_operation(1, regs.get_reg(RegName::L));
                    printf("0x%04X: BIT 1, L :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x5D) {   // 0xCB 0x5D (BIT 3, L - Tests bit 3 of L), cycles: 8
                    uint16_t res = bit_operation(3, regs.get_reg(RegName::L));
                    printf("0x%04X: BIT 3, L :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x6D) {   // 0xCB 0x6D (BIT 5, L - Tests bit 5 of L), cycles: 8
                    uint16_t res = bit_operation(5, regs.get_reg(RegName::L));
                    printf("0x%04X: BIT 5, L :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x7D) {   // 0xCB 0x7D (BIT 7, L - Tests bit 7 of L), cycles: 8
                    uint16_t res = bit_operation(7, regs.get_reg(RegName::L));
                    printf("0x%04X: BIT 7, L :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x71) {   // 0xCB 0x71 (BIT 6, C - Tests bit 6 of C), cycles: 8
                    uint16_t res = bit_operation(6, regs.get_reg(RegName::C));
                    printf("0x%04X: BIT 6, C :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x61) {   // 0xCB 0x61 (BIT 4, C - Tests bit 4 of C), cycles: 8
                    uint16_t res = bit_operation(4, regs.get_reg(RegName::C));
                    printf("0x%04X: BIT 4, C :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x51) {   // 0xCB 0x51 (BIT 2, C - Tests bit 2 of C), cycles: 8
                    uint16_t res = bit_operation(2, regs.get_reg(RegName::C));
                    printf("0x%04X: BIT 2, C :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x41) {   // 0xCB 0x41 (BIT 0, C - Tests bit 0 of C), cycles: 8
                    uint16_t res = bit_operation(0, regs.get_reg(RegName::C));
                    printf("0x%04X: BIT 0, C :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x79) {   // 0xCB 0x79 (BIT 7, C - Tests bit 7 of C), cycles: 8
                    uint16_t res = bit_operation(7, regs.get_reg(RegName::C));
                    printf("0x%04X: BIT 7, C :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x69) {   // 0xCB 0x69 (BIT 5, C - Tests bit 5 of C), cycles: 8
                    uint16_t res = bit_operation(5, regs.get_reg(RegName::C));
                    printf("0x%04X: BIT 5, C :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x59) {   // 0xCB 0x59 (BIT 3, C - Tests bit 3 of C), cycles: 8
                    uint16_t res = bit_operation(3, regs.get_reg(RegName::C));
                    printf("0x%04X: BIT 3, C :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x49) {   // 0xCB 0x49 (BIT 1, C - Tests bit 1 of C), cycles: 8
                    uint16_t res = bit_operation(1, regs.get_reg(RegName::C));
                    printf("0x%04X: BIT 1, C :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x43) {   // 0xCB 0x43 (BIT 0, E - Tests bit 0 of E), cycles: 8
                    uint16_t res = bit_operation(0, regs.get_reg(RegName::E));
                    printf("0x%04X: BIT 0, E :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x53) {   // 0xCB 0x53 (BIT 2, E - Tests bit 2 of E), cycles: 8
                    uint16_t res = bit_operation(2, regs.get_reg(RegName::E));
                    printf("0x%04X: BIT 2, E :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x63) {   // 0xCB 0x63 (BIT 4, E - Tests bit 4 of E), cycles: 8
                    uint16_t res = bit_operation(4, regs.get_reg(RegName::E));
                    printf("0x%04X: BIT 4, E :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x73) {   // 0xCB 0x73 (BIT 6, E - Tests bit 6 of E), cycles: 8
                    uint16_t res = bit_operation(6, regs.get_reg(RegName::E));
                    printf("0x%04X: BIT 6, E :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x4B) {   // 0xCB 0x4B (BIT 1, E - Tests bit 1 of E), cycles: 8
                    uint16_t res = bit_operation(1, regs.get_reg(RegName::E));
                    printf("0x%04X: BIT 1, E :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x5B) {   // 0xCB 0x5B (BIT 3, E - Tests bit 3 of E), cycles: 8
                    uint16_t res = bit_operation(3, regs.get_reg(RegName::E));
                    printf("0x%04X: BIT 3, E :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x6B) {   // 0xCB 0x6B (BIT 5, E - Tests bit 5 of E), cycles: 8
                    uint16_t res = bit_operation(5, regs.get_reg(RegName::E));
                    printf("0x%04X: BIT 5, E :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else if (new_opcode == 0x7B) {   // 0xCB 0x7B (BIT 7, E - Tests bit 7 of E), cycles: 8
                    uint16_t res = bit_operation(7, regs.get_reg(RegName::E));
                    printf("0x%04X: BIT 7, E :: (res: %d) || 0x%02X 0x%02X F: 0x%02X\n", pc, res, opcode, new_opcode, regs.get_reg(RegName::F));
                    num_bytes_read += 1;
                } else {
                    printf("Unknown CB opcode: 0xCB 0x%X\n", new_opcode);
                    return false;
                }
                break;
            }
            case 0xCC: {// 0xCC     (CALL Z NN   - If the zero flag is set, the current PC value plus three is pushed onto the stack, then is loaded with nn), cycles: 17/10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                if (regs.get_flag(FlagName::Z)) {
                    regs.set_reg(RegName::PC, pc+3);    // Increment PC to point to the next instruction
                    printf("0x%04X: CALL Z 0x%04X :: (Z=1, condition met) || 0x%02X\n", pc, new_pc, opcode);
                    push_reg(RegName::PC);
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    printf("0x%04X: CALL Z 0x%04X :: (Z=0, condition not met) || 0x%02X\n", pc, new_pc, opcode);
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xCD: {// 0xCD     (CALL NN     - The current PC value plus three is pushed onto the stack, then is loaded with nn), cycles: 17
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                regs.set_reg(RegName::PC, pc+3);    // Increment PC to point to the next instruction
                printf("0x%04X: CALL 0x%04X || 0x%02X pushed PC: 0x%04X\n", pc, new_pc, opcode, regs.get_reg(RegName::PC));
                push_reg(RegName::PC);
                regs.set_reg(RegName::PC, new_pc);
                return true;
            }
            case 0xD0: {// 0xD0     (RET NC - If the carry flag is unset, the top stack entry is popped into PC), cycles: 11/5
                // Does not affect flags
                if (!regs.get_flag(FlagName::C)) {
                    printf("0x%04X: RET NC :: (C=0, condition met) || 0x%02X\n", pc, opcode);
                    pop_reg(RegName::PC);
                    return true;
                }
                printf("0x%04X: RET NC :: (C=1, condition not met) || 0x%02X\n", pc, opcode);
                break;
            }
            case 0xD1: {// 0xD1     (POP DE), cycles: 10
                // Does not affect flags
                printf("0x%04X: POP DE || 0x%02X\n", pc, opcode);
                pop_reg(RegName::DE);
                break;
            }
            case 0xD2: {// 0xD2 N N (JP NC, N N  - If the carry flag is unset, nn is copied to PC), cycles: 10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                uint16_t cf = regs.get_flag(FlagName::C);
                printf("0x%04X: JP NC, 0x%04X :: (C: %d) || 0x%02X 0x%02X\n", pc, new_pc, cf, opcode, regs.get_reg(RegName::F));
                if (!cf) {
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xD3: {// 0xD3 N   (OUT A:port, A), cycles: 11
                // Does not affect flags
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t port = (a << 8) || mem_bus->read(pc+1);
                printf("0x%04X: OUT 0x%04X, A (A: 0x%02X) | 0x%02X 0x%02X\n", pc, port, a, opcode, flags_in);
                io_bus->write(port, a);
                num_bytes_read ++;
                break;
            }
            case 0xD4: {// 0xD4     (CALL NC NN   - If the carry flag is unset, the current PC value plus three is pushed onto the stack, then is loaded with nn), cycles: 17/10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                if (!regs.get_flag(FlagName::C)) {
                    regs.set_reg(RegName::PC, pc+3);    // Increment PC to point to the next instruction
                    printf("0x%04X: CALL NC 0x%04X :: (C=0, condition met) || 0x%02X\n", pc, new_pc, opcode);
                    push_reg(RegName::PC);
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    printf("0x%04X: CALL NC 0x%04X :: (C=1, condition not met) || 0x%02X\n", pc, new_pc, opcode);
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xD5: {// 0xD5     (PUSH DE), cycles: 11
                // Does not affect flags
                printf("0x%04X: PUSH DE || 0x%02X, SP:  0x%04X\n", pc, opcode, regs.get_reg(RegName::SP));
                push_reg(RegName::DE);
                break;
            }
            case 0xD6: {// 0xD6 N   (SUB A, N     - Subtracts N from A), cycles: 7
                // C as defined
                // N set
                // P/V detects overflow
                // H as defined
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t val = mem_bus->read(pc+1);
                uint16_t res = a - val;
                regs.update_flag(FlagName::C, val > a);
                regs.set_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(a, val, res, true, false));
                regs.update_flag(FlagName::H, calculate_half_carry(a, val, 0, false, true));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                printf("0x%04X: SUB A, 0x%02X :: (0x%04X - 0x%04X = 0x%04X) || 0x%02X 0x%02X\n", pc, val, a, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::A, res);
                num_bytes_read ++;
                break;
            }
            case 0xD8: {// 0xD8     (RET C - If the carry flag is set, the top stack entry is popped into PC), cycles: 11/5
                // Does not affect flags
                if (regs.get_flag(FlagName::C)) {
                    printf("0x%04X: RET C :: (C=1, condition met) || 0x%02X\n", pc, opcode);
                    pop_reg(RegName::PC);
                    return true;
                }
                printf("0x%04X: RET C :: (C=0, condition not met) || 0x%02X\n", pc, opcode);
                break;
            }
            case 0xD9: {// 0xD9     (EXX  - Exchanges the 16-bit contents of BC, DE, and HL with BC', DE', and HL'), cycles 4
                // Does not affect flags
                uint16_t bc  = regs.get_reg(RegName::BC);
                uint16_t _bc = regs.get_reg(RegName::_BC);
                uint16_t de  = regs.get_reg(RegName::DE);
                uint16_t _de = regs.get_reg(RegName::_DE);
                uint16_t hl  = regs.get_reg(RegName::HL);
                uint16_t _hl = regs.get_reg(RegName::_HL);
                printf("0x%04X: EXX :: (0x%02X <-> 0x%02X, 0x%02X <-> 0x%02X, 0x%02X <-> 0x%02X) || 0x%02X\n", pc, bc, _bc, de, _de, hl, _hl, opcode);
                regs.set_reg(RegName::BC, _bc);
                regs.set_reg(RegName::_BC, bc);
                regs.set_reg(RegName::DE, _de);
                regs.set_reg(RegName::_DE, de);
                regs.set_reg(RegName::HL, _hl);
                regs.set_reg(RegName::_HL, hl);
                break;
            }
            case 0xDA: {// 0xDA N N (JP C, N N  - If the carry flag is set, NN is copied to PC), cycles: 10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                uint16_t cf = regs.get_flag(FlagName::C);
                printf("0x%04X: JP C, 0x%04X :: (C: %d) || 0x%02X 0x%02X\n", pc, new_pc, cf, opcode, regs.get_reg(RegName::F));
                if (cf) {
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xDC: {// 0xDC     (CALL C NN   - If the carry flag is set, the current PC value plus three is pushed onto the stack, then is loaded with nn), cycles: 17/10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                if (regs.get_flag(FlagName::C)) {
                    regs.set_reg(RegName::PC, pc+3);    // Increment PC to point to the next instruction
                    printf("0x%04X: CALL C 0x%04X :: (C=1, condition met) || 0x%02X\n", pc, new_pc, opcode);
                    push_reg(RegName::PC);
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    printf("0x%04X: CALL C 0x%04X :: (C=0, condition not met) || 0x%02X\n", pc, new_pc, opcode);
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xDD: {// 0xDD     (IX Instructions, read one more byte to get the actual opcode)
                uint16_t new_opcode = mem_bus->read(pc+1);
                if (new_opcode == 0x40) {   // 0xDD 0x40 (LD B, B - The contents of B are loaded into B), cycles: 8
                    printf("0x%04X: LD B, B || 0x%02X 0x%02X\n", pc, opcode, new_opcode);
                    regs.set_reg(RegName::B, regs.get_reg(RegName::B));
                    num_bytes_read += 1;
                } else if (new_opcode == 0xE1) {   // 0xDD 0xE1 (POP IX), cycles: 14
                    printf("0x%04X: POP IY || 0x%02X 0x%02X\n", pc, opcode, new_opcode);
                    pop_reg(RegName::IX);
                    num_bytes_read += 1;
                } else {
                    printf("Unknown IX opcode: 0xDD 0x%X\n", new_opcode);
                    return false;
                }
                break;
            }
            case 0xE0: {// 0xE0     (RET PO - If the parity/overflow flag is unset, the top stack entry is popped into PC), cycles: 11/5
                // Does not affect flags
                if (!regs.get_flag(FlagName::P_V)) {
                    printf("0x%04X: RET PO :: (P_V=0, condition met) || 0x%02X\n", pc, opcode);
                    pop_reg(RegName::PC);
                    return true;
                }
                printf("0x%04X: RET PO :: (P_V=1, condition not met) || 0x%02X\n", pc, opcode);
                break;
            }
            case 0xE1: {// 0xE1     (POP HL), cycles: 10
                // Does not affect flags
                printf("0x%04X: POP HL || 0x%02X\n", pc, opcode);
                pop_reg(RegName::HL);
                break;
            }
            case 0xE2: {// 0xE2 N N (JP PO, N N  - If the parity/overflow flag is unset, nn is copied to PC), cycles: 10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                uint16_t pvf = regs.get_flag(FlagName::P_V);
                printf("0x%04X: JP PO, 0x%04X :: (P_V: %d) || 0x%02X 0x%02X\n", pc, new_pc, pvf, opcode, regs.get_reg(RegName::F));
                if (!pvf) {
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xE4: {// 0xE4     (CALL PO NN   - If the parity/overflow flag is unset, the current PC value plus three is pushed onto the stack, then is loaded with nn), cycles: 17/10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                if (!regs.get_flag(FlagName::P_V)) {
                    regs.set_reg(RegName::PC, pc+3);    // Increment PC to point to the next instruction
                    printf("0x%04X: CALL PO 0x%04X :: (P_V=0, condition met) || 0x%02X\n", pc, new_pc, opcode);
                    push_reg(RegName::PC);
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    printf("0x%04X: CALL PO 0x%04X :: (P_V=1, condition not met) || 0x%02X\n", pc, new_pc, opcode);
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xE5: {// 0xE5     (PUSH HL), cycles: 11
                // Does not affect flags
                printf("0x%04X: PUSH HL || 0x%02X\n", pc, opcode);
                push_reg(RegName::HL);
                break;
            }
            case 0xE6: {// 0xE6     (AND A, N), cycles: 7
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t val = mem_bus->read(pc+1);
                uint16_t res = a & val;
                regs.clear_flag(FlagName::C);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, z80_parity_flag(res));
                regs.set_flag(FlagName::H);
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                printf("0x%04X: AND A, 0x%02X :: (0x%02X & 0x%02X = 0x%02X) || 0x%02X F=0x%02X\n", pc, val, a, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::A, res);
                num_bytes_read ++;
                break;
            }
            case 0xE8: {// 0xE8     (RET PE - If the parity/overflow flag is set, the top stack entry is popped into PC), cycles: 11/5
                // Does not affect flags
                if (regs.get_flag(FlagName::P_V)) {
                    printf("0x%04X: RET PE :: (P_V=1, condition met) || 0x%02X\n", pc, opcode);
                    pop_reg(RegName::PC);
                    return true;
                }
                printf("0x%04X: RET PE :: (P_V=0, condition not met) || 0x%02X\n", pc, opcode);
                break;
            }
            case 0xEA: {// 0xEA N N (JP PE, N N  - If the parity/overflow flag is set, NN is copied to PC), cycles: 10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                uint16_t pvf = regs.get_flag(FlagName::P_V);
                printf("0x%04X: JP PE, 0x%04X :: (P_V: %d) || 0x%02X 0x%02X\n", pc, new_pc, pvf, opcode, regs.get_reg(RegName::F));
                if (pvf) {
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xEB: {// 0xEB     (EX DE, HL   - Exchange the content of HL and DE), cycles: 4
                // Does not affect flags
                printf("0x%04X: EX DE, HL || 0x%02X \n", pc, opcode);
                uint16_t hl = regs.get_reg(RegName::HL);
                uint16_t de = regs.get_reg(RegName::DE);
                regs.set_reg(RegName::HL, de);
                regs.set_reg(RegName::DE, hl);
                break;
            }
            case 0xEC: {// 0xEC     (CALL PE NN   - If the parity/overflow flag is set, the current PC value plus three is pushed onto the stack, then is loaded with nn), cycles: 17/10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                if (regs.get_flag(FlagName::P_V)) {
                    regs.set_reg(RegName::PC, pc+3);    // Increment PC to point to the next instruction
                    printf("0x%04X: CALL PE 0x%04X :: (P_V=1, condition met) || 0x%02X\n", pc, new_pc, opcode);
                    push_reg(RegName::PC);
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    printf("0x%04X: CALL PE 0x%04X :: (P_V=0, condition not met) || 0x%02X\n", pc, new_pc, opcode);
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xED: {// 0xED     (MISC intruction, read one more byte to get the actual opcode)
                // uint16_t pc = regs.get_reg(RegName::PC);
                uint16_t new_opcode = mem_bus->read(pc+1);
                if (new_opcode == 0x43) {   // 0xED 0x43 N N (LD (NN), BC - Load BC into memory pointed by NN), cycles: 20
                    // Does not affect flags
                    uint16_t addr = mem_read_16b(pc+2);
                    printf("0x%04X: LD 0x%04X, BC || 0x%02X 0x%02X\n", pc, addr, opcode, new_opcode);
                    mem_write_16b(addr, regs.get_reg(RegName::BC));
                    num_bytes_read += 3;
                } else if (new_opcode == 0x47) {   // 0xED 0x47 LD I, A (Load A into I), cycles: 9
                    // Does not affect flags
                    printf("0x%04X: LD I, A || 0x%02X 0x%02X\n", pc, opcode, new_opcode);
                    regs.set_reg(RegName::I, regs.get_reg(RegName::A));
                    num_bytes_read ++;
                } else if (new_opcode == 0x52) {   // 0xED 0x52 SBC HL, DE (Subtract DE and Carry from HL), cycles 15
                    printf("0x%04X: SBC HL, DE || 0x%02X 0x%02X 0x%02X\n", pc, opcode, new_opcode, flags_in);
                    uint16_t hl = regs.get_reg(RegName::HL);
                    uint16_t de = regs.get_reg(RegName::DE);
                    uint16_t carry_in = regs.get_flag(FlagName::C)? 1: 0;
                    uint16_t res = hl - de - carry_in;
                    regs.set_reg(RegName::HL, res);
                    ((de+carry_in)>hl)?regs.set_flag(FlagName::C): regs.clear_flag(FlagName::C);
                    regs.set_flag(FlagName::N);
                    regs.set_reg(RegName::HL, res);
                    bool pv = calculate_overflow(hl, de+carry_in, res, true, true);
                    pv? regs.set_flag(FlagName::P_V): regs.clear_flag(FlagName::P_V);
                    bool hc = calculate_half_carry(hl, de, carry_in, false, true);
                    hc? regs.set_flag(FlagName::H): regs.clear_flag(FlagName::H);
                    res==0? regs.set_flag(FlagName::Z): regs.clear_flag(FlagName::Z);
                    (res&0x80)==0x80? regs.set_flag(FlagName::S): regs.clear_flag(FlagName::S);
                    num_bytes_read ++;
                } else if (new_opcode == 0x73) {   // 0xED 0x73 N N (LD (NN), SP - Load SP into memory pointed by NN), cycles: 20
                    // Does not affect flags
                    uint16_t addr = mem_read_16b(pc+2);
                    printf("0x%04X: LD (0x%04X), SP || 0x%02X 0x%02X\n", pc, addr, opcode, new_opcode);
                    mem_write_16b(addr, regs.get_reg(RegName::SP));
                    num_bytes_read += 3;
                } else if (new_opcode == 0x7B) {   // 0xED 0x7B N N (LD SP, (NN) - Loads the value pointed to by nn into SP), cycles: 20
                    // Does not affect flags
                    uint16_t addr = mem_read_16b(pc+2);
                    uint16_t val = mem_read_16b(addr);
                    printf("0x%04X: LD SP, 0x%04X : (addr: 0x%04X) || 0x%02X 0x%02X\n", pc, val, addr, opcode, new_opcode);
                    regs.set_reg(RegName::SP, val);
                    num_bytes_read += 3;
                } else if (new_opcode == 0xB0) {    // 0xED 0xB0 LDIR, cycles 21/16
                    // C unaffected
                    // N reset
                    // P/V reset
                    // H reset
                    // Z unaffected
                    // S unaffected
                    // Transfers a byte of data from the memory location pointed to by HL to the memory location pointed to by DE.
                    // Then HL and DE are incremented and BC is decremented. If BC is not zero, this operation is repeated. 
                    // Interrupts can trigger while this instruction is processing.
                    uint16_t bc = regs.get_reg(RegName::BC);
                    if (bc == 0) {
                        printf("0x%04X: LDIR (BC == 0)| 0x%02X 0x%02X\n", pc, opcode, new_opcode);
                    }
                    while (bc) {
                        uint16_t hl = regs.get_reg(RegName::HL);
                        uint16_t de = regs.get_reg(RegName::DE);
                        uint16_t val = mem_bus->read(hl);
                        printf("0x%04X: LDIR : (moving 0x%02X from 0x%04X to 0x%04X, BC: 0x%04X) || 0x%02X 0x%02X\n", pc, val, hl, de, bc, opcode, new_opcode);
                        mem_bus->write(de, val);
                        regs.inc_reg(RegName::HL);
                        regs.inc_reg(RegName::DE);
                        regs.dec_reg(RegName::BC);
                        bc = regs.get_reg(RegName::BC);
                    }
                    regs.clear_flag(FlagName::N);
                    regs.clear_flag(FlagName::P_V);
                    regs.clear_flag(FlagName::H);
                    num_bytes_read ++;
                } else {
                    printf("Unknown misc opcode: 0xED 0x%X\n", new_opcode);
                    return false;
                }
                break;
            }
            case 0xEE: {// 0xEE N   (XOR A, N - Bitwise XOR on A with N), cycles: 7
                // C reset
                // N reset
                // P/V detects parity
                // H reset
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t val = mem_bus->read(pc+1);
                uint16_t res = a ^ val;
                regs.clear_flag(FlagName::C);
                regs.clear_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, z80_parity_flag(res));
                regs.clear_flag(FlagName::H);
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                printf("0x%04X: XOR A, 0x%02X : (0x%02X^0x%02X=0x%02X) || 0x%02X 0x%02X\n", pc, val, a, val, res, opcode, regs.get_reg(RegName::F));
                regs.set_reg(RegName::A, res);
                num_bytes_read += 1;
                break;
            }
            case 0xF0: {// 0xF0     (RET P - If the sign flag is unset, the top stack entry is popped into PC), cycles: 11/5
                // Does not affect flags
                if (!regs.get_flag(FlagName::S)) {
                    printf("0x%04X: RET P :: (S=0, condition met) || 0x%02X\n", pc, opcode);
                    pop_reg(RegName::PC);
                    return true;
                }
                printf("0x%04X: RET P :: (S=1, condition not met) || 0x%02X\n", pc, opcode);
                break;
            }
            case 0xF1: {// 0xF1     (POP AF), cycles: 10
                // Does not affect flags
                printf("0x%04X: POP AF || 0x%02X\n", pc, opcode);
                pop_reg(RegName::AF);
                break;
            }
            case 0xF2: {// 0xF2 N N (JP P, N N  - If the sign flag is unset, nn is copied to PC), cycles: 10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                uint16_t sf = regs.get_flag(FlagName::S);
                printf("0x%04X: JP P, 0x%04X :: (S: %d) || 0x%02X 0x%02X\n", pc, new_pc, sf, opcode, regs.get_reg(RegName::F));
                if (!sf) {
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xF3: {// 0xF3     (DI          - Disable Interrupts), cycles: 4
                // Does not affect flags
                printf("0x%04X: DI || 0x%02X\n", pc, opcode);
                interrupts_enabled = false;
                break;
            }
            case 0xF4: {// 0xF4     (CALL P NN   - If the sign flag is unset, the current PC value plus three is pushed onto the stack, then is loaded with nn), cycles: 17/10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                if (!regs.get_flag(FlagName::S)) {
                    regs.set_reg(RegName::PC, pc+3);    // Increment PC to point to the next instruction
                    printf("0x%04X: CALL P 0x%04X :: (S=0, condition met) || 0x%02X\n", pc, new_pc, opcode);
                    push_reg(RegName::PC);
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    printf("0x%04X: CALL P 0x%04X :: (S=1, condition not met) || 0x%02X\n", pc, new_pc, opcode);
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xF5: {// 0xF5     (PUSH AF), cycles: 11
                // Does not affect flags
                printf("0x%04X: PUSH AF || 0x%02X\n", pc, opcode);
                push_reg(RegName::AF);
                break;
            }
            case 0xF8: {// 0xF8     (RET M - If the sign flag is set, the top stack entry is popped into PC), cycles: 11/5
                // Does not affect flags
                if (regs.get_flag(FlagName::S)) {
                    printf("0x%04X: RET M :: (S=1, condition met) || 0x%02X\n", pc, opcode);
                    pop_reg(RegName::PC);
                    return true;
                }
                printf("0x%04X: RET M :: (S=0, condition not met) || 0x%02X\n", pc, opcode);
                break;
            }
            case 0xF9: {// 0xF9     (LD SP, HL   - Load HL into SP), cycles: 6
                // Does not affect flags
                regs.set_reg(RegName::SP, regs.get_reg(RegName::HL));
                printf("0x%04X: LD SP, HL || 0x%02X\n", pc, opcode);
                break;
            }
            case 0xFA: {// 0xFA N N (JP M, N N  - If the sign flag is set, nn is copied to PC), cycles: 10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                uint16_t sf = regs.get_flag(FlagName::S);
                printf("0x%04X: JP M, 0x%04X :: (S: %d) || 0x%02X 0x%02X\n", pc, new_pc, sf, opcode, regs.get_reg(RegName::F));
                if (sf) {
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xFB: {// 0xFB     (EI          - Enable Interrupts - Sets both interrupt flip-flops, thus allowing maskable interrupts to occur), cycles: 4
                // Does not affect flags
                printf("0x%04X: EI || 0x%02X\n", pc, opcode);
                interrupts_enabled = true;
                break;
            }
            case 0xFC: {// 0xFC     (CALL M NN   - If the sign flag is set, the current PC value plus three is pushed onto the stack, then is loaded with nn), cycles: 17/10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                if (regs.get_flag(FlagName::S)) {
                    regs.set_reg(RegName::PC, pc+3);    // Increment PC to point to the next instruction
                    printf("0x%04X: CALL M 0x%04X :: (S=1, condition met) || 0x%02X\n", pc, new_pc, opcode);
                    push_reg(RegName::PC);
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    printf("0x%04X: CALL M 0x%04X :: (S=0, condition not met) || 0x%02X\n", pc, new_pc, opcode);
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xFD: {// 0xFD     (IY Instructions, read one more byte to get the actual opcode)
                uint16_t new_opcode = mem_bus->read(pc+1);
                if (new_opcode == 0xE1) {   // 0xFD 0xE1 (POP IY), cycles: 14
                    printf("0x%04X: POP IY || 0x%02X 0x%02X\n", pc, opcode, new_opcode);
                    pop_reg(RegName::IY);
                    num_bytes_read += 1;
                } else if (new_opcode == 0xE5) {   // 0xFD 0xE5 (PUSH IY), cycles: 15
                    printf("0x%04X: PUSH IY || 0x%02X 0x%02X\n", pc, opcode, new_opcode);
                    push_reg(RegName::IY);
                    num_bytes_read += 1;
                } else {
                    printf("Unknown IY opcode: 0xFD 0x%X\n", new_opcode);
                    return false;
                }
                break;
            }
            case 0xFE: {// 0xFE     (CP A, N     - Subtract N from A and update flags. A stays unchanged), cycles: 7
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t val = mem_bus->read(pc+1);
                uint16_t res = a - val;
                regs.update_flag(FlagName::C, val > a);
                regs.set_flag(FlagName::N);
                regs.update_flag(FlagName::P_V, calculate_overflow(a, val, res, true, false));
                regs.update_flag(FlagName::H, calculate_half_carry(a, val, 0, false, true));
                regs.update_flag(FlagName::Z, z80_zero_flag(res));
                regs.update_flag(FlagName::S, z80_sign_flag(res, false));
                printf("0x%04X: CP A, 0x%02X :: (0x%04X - 0x%04X = 0x%04X) || 0x%02X 0x%02X\n", pc, val, a, val, res, opcode, regs.get_reg(RegName::F));
                num_bytes_read ++;
                break;
            }
            case 0xFF: {// 0xFF     (RST 38H    - The current PC value plus one is pushed onto the stack, then is loaded with 56), cycles: 11
                // Does not affect flags
                regs.set_reg(RegName::PC, pc+1);    // Increment PC to point to the next instruction
                printf("0x%04X: RST 38H || 0x%02X\n", pc, opcode);
                push_reg(RegName::PC);
                regs.set_reg(RegName::PC, 0x38);
                return true;
            }
            default:
                printf("Unknown opcode: 0x%02X at 0x%04X\n", opcode, pc);
                return false;
        }
        regs.set_reg(RegName::PC, pc+num_bytes_read);
        return true;
    }

    int bit_operation(uint16_t bit, uint16_t val) {
        // C unaffected
        // N reset
        // P/V undefined
        // H set
        // Z as defined
        // S undefined
        regs.clear_flag(FlagName::N);
        regs.set_flag(FlagName::H);
        uint16_t res = ((1<<bit)&val) == 0? 0: 1;
        regs.update_flag(FlagName::Z, res? 0: 1);
        return res;
    }

    int or_operation(uint16_t pc, uint8_t opcode) {
        int ret_val = 0;
        bool is_16b = false;
        uint16_t res;
        switch(opcode) {
            case 0xB0: {// OR A, B, cycles: 4
                // C reset
                // N reset
                // P/V detects parity
                // H reset
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t b = regs.get_reg(RegName::B);
                res = (a | b) & 0xFF;
                regs.set_reg(RegName::A, res);
                printf("0x%04X: OR A, B :: (0x%02X | 0x%02X -> 0x%02X) || 0x%02X 0x%02X\n", pc, a, b, res, opcode, regs.get_reg(RegName::F));
                break;
            }
            case 0xB6: {// OR A, (HL), cycles: 7
                // C reset
                // N reset
                // P/V detects parity
                // H reset
                // Z as defined
                // S as defined
                uint16_t hl = regs.get_reg(RegName::HL);
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t val = mem_bus->read(hl);
                res = (a | val) & 0xFF;
                regs.set_reg(RegName::A, res);
                printf("0x%04X: OR A, (HL) (0x%02X | 0x%02X -> 0x%02X) | 0x%02X 0x%02X\n", pc, a, val, res, opcode, regs.get_reg(RegName::F));
                break;
            }
            case 0xB7: {// OR A, A, cycles: 7
                // C reset
                // N reset
                // P/V detects parity
                // H reset
                // Z as defined
                // S as defined
                uint16_t a = regs.get_reg(RegName::A);
                res = (a | a) & 0xFF;
                regs.set_reg(RegName::A, res);
                printf("0x%04X: OR A, A (0x%02X | 0x%02X -> 0x%02X) | 0x%02X 0x%02X\n", pc, a, a, res, opcode, regs.get_reg(RegName::F));
                break;
            }
            default:
                printf("Unknown OR opcode: 0x%02X at 0x%04X\n", opcode, pc);
                return -1;
        }
        regs.clear_flag(FlagName::C);
        regs.clear_flag(FlagName::N);
        regs.update_flag(FlagName::P_V, z80_parity_flag(res));
        regs.clear_flag(FlagName::H);
        regs.update_flag(FlagName::Z, z80_zero_flag(res));
        regs.update_flag(FlagName::S, z80_sign_flag(res, is_16b));
        return ret_val;
    }

    void tick() override {
        bool cont = true;
        // uint32_t counter = 0;
        // regs.set_reg(RegName::PC, 0x0100);
        // mem_bus->write(0x0000, 0x76);
        // mem_bus->write(0x0028, 0x76);
        // mem_bus->write(0x0005, 0xC3);
        // mem_bus->write(0x0006, 0x00);
        // mem_bus->write(0x0007, 0xF0);

        while(cont) {
            uint16_t pc = regs.get_reg(RegName::PC);
            uint8_t opcode = mem_bus->read(pc);
            // std::cout << "Ticking... Read Opcode: " << "0x" << std::uppercase << std::hex << (int)opcode << " at " << pc << "\n";
            cont = execute_opcode(opcode);
            // regs.inc_reg(RegName::PC);
            // counter ++;
            uint16_t sp = regs.get_reg(RegName::SP);
            if (sp > 0xF000) {
                printf("Error: Stack overflow! SP = 0x%04X\n", sp);
                break;
            }
            // if(mem_bus->read(0x0005) != 0xC3 || mem_bus->read(0x0006) != 0x00 || mem_bus->read(0x0007) != 0xF0) {
            //     printf("Error: Memory overriden!\n");
            //     break;
            // }
        }
    }

    void interrupt() override {
        // Push PC to stack, set interrupt flag, jump to vector
        std::cout << "Interrupt triggered.\n";
    }
};

int main() {
    uint16_t ROM_SIZE = 0x4000;// 16KB ROM
    uint16_t RAM_SIZE = 0xC000;// 48KB RAM
    uint16_t MEM_SIZE = 0xFFFF;// 64KB Memory space
    Memory rom(ROM_SIZE);
    Memory ram(RAM_SIZE);
    IO_Space io(MEM_SIZE);
    
    AddressDecoder mem_decoder;
    AddressDecoder io_decoder;

    mem_decoder.set_memspace_name("mem_space");
    io_decoder.set_memspace_name("io_space");

    mem_decoder.disable_log();
    io_decoder.enable_log();

    mem_decoder.map_device(0x0000, ROM_SIZE-1, &rom);
    mem_decoder.map_device(ROM_SIZE, MEM_SIZE, &ram);

    io_decoder.map_device(0x0000, MEM_SIZE, &io);

    // if(rom.map_image("./zx_spectrum/spec48.rom", 0)) {
    if(rom.map_image("./zx_spectrum/my_test_suite.bin", 0x0000)) {
        std::cout << "My test suite successfully mapped\n";
    }
    // if(rom.map_image("./zx_spectrum/zexall.bin", 0x0100)) {
    //     std::cout << "ZEXALL ROM image successfully mapped\n";
    // }
    // if(ram.map_image("./zx_spectrum/0xF000_proc.bin", 0xF000-ROM_SIZE)) {
    //     std::cout << "Print procedure ROM image successfully mapped\n";
    // }

    Z80CPU z80_cpu(&mem_decoder, &io_decoder);

    z80_cpu.tick();
    io.end_log();

    return 0;
}