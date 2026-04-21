#include <iostream>
#include <vector>
#include <cstdint>
#include "addr_decoder.hpp"
#include "memory.hpp"
#include "cpu.hpp"

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
 * 7 6 5 4 3  2  1 0
 * C Z X N X P\V N C
 * 
 * C -   Carry Flag
 * N -   Add/Subtract
 * P/V - Parity/Overflow Flag
 * H -   Half Carry Flag
 * Z -   Zero Flag
 * S -   Sign Flag
 * X -   Not Used
 */

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
    SP
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

public:
    Z80Registers() { reset(); }

    void reset() {
        A = F = B = C = D = E = H = L = 0;
        _A = _F = _B = _C = _D = _E = _H = _L = 0;
        I = R = 0;
        IX = IY = PC = SP = 0;
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
    bool execute_opcode(uint8_t opcode) {
        uint8_t num_bytes_read = 1;
        switch(opcode) {
            case 0x00:  // NOP
                break;
            case 0x11: {// 0x11 N N (Load N N into DE)
                uint16_t pc = regs.get_reg(RegName::PC);
                regs.set_reg(RegName::D, mem_bus->read(pc+2));
                regs.set_reg(RegName::E, mem_bus->read(pc+1));
                num_bytes_read += 2;
                break;
            }
            case 0x3E: {// 0x3E (LD A, N - Load N into A)
                uint16_t pc = regs.get_reg(RegName::PC);
                regs.set_reg(RegName::A, mem_bus->read(pc+1));
                num_bytes_read ++;
                break;
            }
            case 0x47: {// 0x47 (LD B, A - Load A into B)
                regs.set_reg(RegName::B, regs.get_reg(RegName::A));
                break;
            }
            case 0xAF:  // XOR A
                // TODO: Update flags!
                regs.set_reg(RegName::A, 0);
                break;
            case 0xC3: {// 0xC3 N N (JP N N - Load N N into PC)
                uint16_t pc = regs.get_reg(RegName::PC);
                uint16_t new_pc = (mem_bus->read(pc+2) << 8) + mem_bus->read(pc+1);
                regs.set_reg(RegName::PC, new_pc);
                return true;
            }
            case 0xD3: {// 0xD3 N (OUT A:port, A)
                uint16_t pc = regs.get_reg(RegName::PC);
                uint16_t a = regs.get_reg(RegName::A);
                uint16_t port = (a << 8) | mem_bus->read(pc+1);
                io_bus->write(port, a);
                num_bytes_read ++;
                break;
            }
            case 0xF3:  // DI (Disable Interrupts)
                interrupts_enabled = false;
                break;
            default:
                printf("Unknown opcode: 0x%X\n", opcode);
                return false;
        }
        regs.set_reg(RegName::PC, regs.get_reg(RegName::PC)+num_bytes_read);
        return true;
    }

    void tick() override {
        bool cont = true;
        uint32_t counter = 0;
        while(cont && counter < 10) {
            uint16_t pc = regs.get_reg(RegName::PC);
            uint8_t opcode = mem_bus->read(pc);
            std::cout << "Ticking... Read Opcode: " << "0x" << std::uppercase << std::hex << (int)opcode << " at " << pc << "\n";
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