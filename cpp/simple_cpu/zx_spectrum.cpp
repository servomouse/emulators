#include <iostream>
#include <vector>
#include <cstdint>
#include "addr_decoder.hpp"
#include "memory.hpp"
#include "cpu.hpp"
#include "z80_utils.hpp"

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
    BC, DE, HL,
    _A, _F, _B, _C, _D, _E, _H, _L,
    _BC, _DE, _HL,
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
        uint16_t value = (1<<static_cast<int>(f_name)) ^ f_reg;
        set_reg(RegName::F, value);
    }

    uint16_t get_reg(RegName name) const {
        uint16_t value = 0;
        switch (name) {
            // Main set
            case RegName::A:  value = A; break;
            case RegName::F:  value = F; break;
            case RegName::B:  value = B; break;
            case RegName::C:  value = C; break;
            case RegName::BC:  value = (B<<8) | C; break;
            case RegName::D:  value = D; break;
            case RegName::E:  value = E; break;
            case RegName::DE:  value = (D<<8) | E; break;
            case RegName::H:  value = H; break;
            case RegName::L:  value = L; break;
            // Alternate set
            case RegName::_A:  value = _A; break;
            case RegName::_F:  value = _F; break;
            case RegName::_B:  value = _B; break;
            case RegName::_C:  value = _C; break;
            case RegName::_BC:  value = (_B<<8) | _C; break;
            case RegName::_D:  value = _D; break;
            case RegName::_E:  value = _E; break;
            case RegName::_DE:  value = (_D<<8) | _E; break;
            case RegName::_H:  value = _H; break;
            case RegName::_L:  value = _L; break;
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

    void set_reg(RegName name, uint16_t value) {
        // printf("Setting register %d to 0x%X\n", name, value);
        switch (name) {
            // Main set
            case RegName::A:  A = value&0xFF; break;
            case RegName::F:  F = value&0xFF; break;
            case RegName::B:  B = value&0xFF; break;
            case RegName::C:  C = value&0xFF; break;
            case RegName::BC: {C = value&0xFF; B = value>>8; break;}
            case RegName::D:  D = value&0xFF; break;
            case RegName::E:  E = value&0xFF; break;
            case RegName::DE: {E = value&0xFF; D = value>>8; break;}
            case RegName::H:  H = value&0xFF; break;
            case RegName::L:  L = value&0xFF; break;
            // Alternate set
            case RegName::_A:  _A = value&0xFF; break;
            case RegName::_F:  _F = value&0xFF; break;
            case RegName::_B:  _B = value&0xFF; break;
            case RegName::_C:  _C = value&0xFF; break;
            case RegName::_BC: {_C = value&0xFF; _B = value>>8; break;}
            case RegName::_D:  _D = value&0xFF; break;
            case RegName::_E:  _E = value&0xFF; break;
            case RegName::_DE: {_E = value&0xFF; _D = value>>8; break;}
            case RegName::_H:  _H = value&0xFF; break;
            case RegName::_L:  _L = value&0xFF; break;
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
            case 0x00:  // NOP
                printf("0x%04X: NOP | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                break;
            case 0x11: {// 0x11 N N (Load N N into DE), cycles: 10
                // Does not affect flags
                uint16_t new_val = mem_read_16b(pc+1);
                regs.set_reg(RegName::DE, new_val);
                printf("0x%04X: LD DE, 0x%04X | 0x%02X 0x%02X\n", pc, new_val, opcode, flags_in);
                num_bytes_read += 2;
                break;
            }
            case 0x19: {// 0x19 (ADD HL, DE - Add DE to HL), cycles: 11
                // C as defined
                // N reset
                // P/V unaffected
                // H as defined
                // Z unaffected
                // S unaffected
                uint16_t hl = regs.get_reg(RegName::HL);
                uint16_t de = regs.get_reg(RegName::DE);
                uint16_t res = z80_add(hl, de, false, true);

                printf("0x%04X: ADD HL, DE | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                regs.set_reg(RegName::HL, res);
                break;
            }
            case 0x20: {// 0x20 N (JR Z, N - If Z flag is set, add signed N to PC), cycles: 12/7
                // Does not affect flags
                int16_t pc_inc = static_cast<int16_t>(mem_bus->read(pc+1));
                int16_t temp = static_cast<int16_t>(pc) + pc_inc;
                uint16_t zf = regs.get_flag(FlagName::Z);
                printf("0x%04X: JR Z, 0x%02X (Z: %d) | 0x%02X 0x%02X\n", pc, pc_inc, zf, opcode, flags_in);
                if (zf) {
                    regs.set_reg(RegName::PC, static_cast<uint16_t>(temp));
                    return true;
                } else {
                    num_bytes_read += 1;
                }
                break;
            }
            case 0x23: {// 0x23 (INC HL), cycles: 6
                // Does not affect flags
                printf("0x%04X: INC HL | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                regs.inc_reg(RegName::HL);
                break;
            }
            case 0x2B: {// 0x2B (DEC HL), cycles: 6
                // Does not affect flags
                printf("0x%04X: DEC HL | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                regs.dec_reg(RegName::HL);
                break;
            }
            case 0x30: {// 0x30 (JR NC, N - If carry flag is unset, add signed N to PC), cycles: 12/7
                // Does not affect flags
                int16_t val = mem_bus->read(pc+1);
                uint16_t cf = regs.get_flag(FlagName::C);
                printf("0x%04X: JR NC, 0x%02X (C: %d) | 0x%02X 0x%02X\n", pc, val, cf, opcode, flags_in);
                if(!cf) {
                    regs.set_reg(RegName::PC, regs.get_reg(RegName::PC) + val);
                    return true;
                }
                num_bytes_read ++;
                break;
            }
            case 0x36: {// 0x36 (LD HL, N - Load N into HL), cycles: 10
                // Does not affect flags
                uint16_t val = mem_bus->read(pc+1);
                printf("0x%04X: LD HL, 0x%02X | 0x%02X 0x%02X\n", pc, val, opcode, flags_in);
                regs.set_reg(RegName::HL, val);
                num_bytes_read ++;
                break;
            }
            case 0x3E: {// 0x3E (LD A, N - Load N into A), cycles: 7
                // Does not affect flags
                uint16_t val = mem_bus->read(pc+1);
                printf("0x%04X: LD A, 0x%02X | 0x%02X 0x%02X\n", pc, val, opcode, flags_in);
                regs.set_reg(RegName::A, val);
                num_bytes_read ++;
                break;
            }
            case 0x47: {// 0x47 (LD B, A - Load A into B), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD B, A | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                regs.set_reg(RegName::B, regs.get_reg(RegName::A));
                break;
            }
            case 0x62: {// 0x62 (LD H, D - Load D into H), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD H, D | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                regs.set_reg(RegName::H, regs.get_reg(RegName::D));
                break;
            }
            case 0x6B: {// 0x6B (LD L, E - Load E into L), cycles: 4
                // Does not affect flags
                printf("0x%04X: LD L, E | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                regs.set_reg(RegName::L, regs.get_reg(RegName::E));
                break;
            }
            case 0xA7: {// 0xA7 AND A, A, cycles: 4
                printf("0x%04X: AND A, A | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                uint16_t a = regs.get_reg(RegName::A);
                bool par = parity(a);
                regs.clear_flag(FlagName::C);
                regs.clear_flag(FlagName::N);
                par? regs.set_flag(FlagName::P_V): regs.clear_flag(FlagName::P_V);
                regs.set_flag(FlagName::H);
                a == 0? regs.set_flag(FlagName::Z): regs.clear_flag(FlagName::Z);
                (a & 0x80) == 0x80? regs.set_flag(FlagName::S): regs.clear_flag(FlagName::S);
                break;
            }
            case 0xAF:  // XOR A, A, cycles: 4
                printf("0x%04X: XOR A, A | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                regs.clear_flag(FlagName::C);
                regs.clear_flag(FlagName::N);
                regs.set_flag(FlagName::P_V);   // 1 when result is 0
                regs.clear_flag(FlagName::H);
                regs.set_flag(FlagName::Z);
                regs.clear_flag(FlagName::S);
                regs.set_reg(RegName::A, 0);
                break;
            case 0xBC: {// 0xBC (CP H, A - Substract H from A and update flags. A stays unchanged), cycles: 4
                printf("0x%04X: CP H, A | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t h = regs.get_reg(RegName::H);
                uint16_t res = a - h;
                bool pvf = calculate_overflow(a, h, res, true, false);
                bool hf = calculate_half_carry(a, h, 0, true);
                (h > a)? regs.set_flag(FlagName::C): regs.clear_flag(FlagName::C);
                regs.set_flag(FlagName::N);
                pvf? regs.set_flag(FlagName::P_V): regs.clear_flag(FlagName::P_V);   // 1 when result is 0
                hf?  regs.set_flag(FlagName::H): regs.clear_flag(FlagName::H);
                res == 0? regs.set_flag(FlagName::Z): regs.clear_flag(FlagName::Z);
                (res&0x80)==0x80? regs.set_flag(FlagName::S): regs.clear_flag(FlagName::S);
                break;
            }
            case 0xC2: {// 0xC2 N N (JP NZ, N N - Load N N into PC if Zero flag is not set), cycles: 10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                uint16_t zf = regs.get_flag(FlagName::Z);
                printf("0x%04X: JP NZ, 0x%04X (Z: %d) | 0x%02X 0x%02X\n", pc, new_pc, zf, opcode, flags_in);
                if (!zf) {
                    regs.set_reg(RegName::PC, new_pc);
                    return true;
                } else {
                    num_bytes_read += 2;
                }
                break;
            }
            case 0xC3: {// 0xC3 N N (JP N N - Load N N into PC), cycles: 10
                // Does not affect flags
                uint16_t new_pc = mem_read_16b(pc+1);
                printf("0x%04X: JP 0x%04X | 0x%02X 0x%02X\n", pc, new_pc, opcode, flags_in);
                regs.set_reg(RegName::PC, new_pc);
                return true;
            }
            case 0xD3: {// 0xD3 N (OUT A:port, A), cycles: 11
                // Does not affect flags
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t port = (a << 8) | mem_bus->read(pc+1);
                printf("0x%04X: OUT 0x%04X, A (A: 0x%02X) | 0x%02X 0x%02X\n", pc, port, a, opcode, flags_in);
                io_bus->write(port, a);
                num_bytes_read ++;
                break;
            }
            case 0xED: {// 0xED: MISC intruction, read one more byte to get the actual opcode
                // uint16_t pc = regs.get_reg(RegName::PC);
                uint16_t new_opcode = mem_bus->read(pc+1);
                if (new_opcode == 0x47) {   // 0xED 0x47 LD I, A (Load A into I), cycles: 9
                    // Does not affect flags
                    printf("0x%04X: LD I, A | 0x%02X 0x%02X 0x%02X\n", pc, opcode, new_opcode, flags_in);
                    uint16_t a = regs.get_reg(RegName::A);
                    regs.set_reg(RegName::I, a);
                    num_bytes_read ++;
                } else if (new_opcode == 0x52) {   // 0xED 0x52 SBC HL, DE (Subtract DE and Carry from HL), cycles 15
                    printf("0x%04X: SBC HL, DE | 0x%02X 0x%02X 0x%02X\n", pc, opcode, new_opcode, flags_in);
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
                    bool hc = calculate_half_carry(hl, de, carry_in, true);
                    hc? regs.set_flag(FlagName::H): regs.clear_flag(FlagName::H);
                    res==0? regs.set_flag(FlagName::Z): regs.clear_flag(FlagName::Z);
                    (res&0x80)==0x80? regs.set_flag(FlagName::S): regs.clear_flag(FlagName::S);
                    num_bytes_read ++;
                } else {
                    printf("Unknown misc opcode: 0x%X\n", new_opcode);
                    return false;
                }
                break;
            }
            case 0xF3:  // DI (Disable Interrupts), cycles: 4
                // Does not affect flags
                printf("0x%04X: DI | 0x%02X 0x%02X\n", pc, opcode, flags_in);
                interrupts_enabled = false;
                break;
            default:
                printf("Unknown opcode: 0x%X\n", opcode);
                return false;
        }
        regs.set_reg(RegName::PC, pc+num_bytes_read);
        return true;
    }

    void tick() override {
        bool cont = true;
        uint32_t counter = 0;
        while(cont && counter < 256) {
            uint16_t pc = regs.get_reg(RegName::PC);
            uint8_t opcode = mem_bus->read(pc);
            // std::cout << "Ticking... Read Opcode: " << "0x" << std::uppercase << std::hex << (int)opcode << " at " << pc << "\n";
            cont = execute_opcode(opcode);
            // regs.inc_reg(RegName::PC);
            counter ++;
        }
    }

    void interrupt() override {
        // Push PC to stack, set interrupt flag, jump to vector
        std::cout << "Interrupt triggered.\n";
    }
};

int main() {
    Memory rom(0x4000);   // 16KB ROM
    Memory ram(0xC000);   // 48KB RAM
    
    AddressDecoder mem_decoder;
    AddressDecoder io_decoder;

    mem_decoder.set_memspace_name("mem_space");
    io_decoder.set_memspace_name("io_space");

    mem_decoder.disable_log();
    io_decoder.enable_log();

    mem_decoder.map_device(0x0000, 0x3FFF, &rom);
    mem_decoder.map_device(0x4000, 0xFFFF, &ram);

    if(rom.map_image("./zx_spectrum/spec48.rom", 0)) {
        std::cout << "ROM image successfully mapped\n";
    }

    Z80CPU z80_cpu(&mem_decoder, &io_decoder);

    z80_cpu.tick();

    return 0;
}