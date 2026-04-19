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
        switch (name) {
            // Main set
            case RegName::A:  return A;
            case RegName::F:  return F;
            case RegName::B:  return B;
            case RegName::C:  return C;
            case RegName::BC:  return (B<<8) | C;
            case RegName::D:  return D;
            case RegName::E:  return E;
            case RegName::DE:  return (D<<8) | E;
            case RegName::H:  return H;
            case RegName::L:  return L;
            // Alternate set
            case RegName::_A:  return _A;
            case RegName::_F:  return _F;
            case RegName::_B:  return _B;
            case RegName::_C:  return _C;
            case RegName::_BC:  return (_B<<8) | _C;
            case RegName::_D:  return _D;
            case RegName::_E:  return _E;
            case RegName::_DE:  return (_D<<8) | _E;
            case RegName::_H:  return _H;
            case RegName::_L:  return _L;
            // 16-bit registers
            case RegName::I: return I;
            case RegName::R: return R;
            case RegName::IX: return IX;
            case RegName::IY: return IY;
            case RegName::PC: return PC;
            case RegName::SP: return SP;
            default: return 0;
        }
    }

    void set_reg(RegName name, uint16_t value) {
        switch (name) {
            // Main set
            case RegName::A:  A = value&0xFF;
            case RegName::F:  F = value&0xFF;
            case RegName::B:  B = value&0xFF;
            case RegName::C:  C = value&0xFF;
            case RegName::BC: {C = value&0xFF; B = value>>8;}
            case RegName::D:  D = value&0xFF;
            case RegName::E:  E = value&0xFF;
            case RegName::DE: {E = value&0xFF; D = value>>8;}
            case RegName::H:  H = value&0xFF;
            case RegName::L:  L = value&0xFF;
            // Alternate set
            case RegName::_A:  _A = value&0xFF;
            case RegName::_F:  _F = value&0xFF;
            case RegName::_B:  _B = value&0xFF;
            case RegName::_C:  _C = value&0xFF;
            case RegName::_BC: {_C = value&0xFF; _B = value>>8;}
            case RegName::_D:  _D = value&0xFF;
            case RegName::_E:  _E = value&0xFF;
            case RegName::_DE: {_E = value&0xFF; _D = value>>8;}
            case RegName::_H:  _H = value&0xFF;
            case RegName::_L:  _L = value&0xFF;
            // 16-bit registers
            case RegName::I: I = value;
            case RegName::R: R = value;
            case RegName::IX: IX = value;
            case RegName::IY: IY = value;
            case RegName::PC: PC = value;
            case RegName::SP: SP = value;
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
public:
    Z80CPU(AddressDecoder* mem, AddressDecoder* io) 
        : CPU(mem, io) {
        reset();
    }
    void reset() override {
        regs.reset();
        std::cout << "CPU Reset performed.\n";
    }

    void tick() override {
        uint8_t opcode = mem_bus->read(regs.get_reg(RegName::PC));
        std::cout << "Ticking... Read Opcode: " << "0x" << std::uppercase << std::hex << (int)opcode << " at " << regs.get_reg(RegName::PC) << "\n";
        regs.inc_reg(RegName::PC);
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

    mem_decoder.map_device(0x0000, 0x3FFF, &rom);
    mem_decoder.map_device(0x4000, 0xFFFF, &ram);

    if(rom.map_image("./zx_spectrum/spec48.rom", 0)) {
        std::cout << "ROM image successfully mapped\n";
    }

    Z80CPU z80_cpu(&mem_decoder, &io_decoder);

    z80_cpu.tick();

    return 0;
}