#include <iostream>
#include <vector>
#include <cstdint>
#include "addr_decoder.hpp"
#include "memory.hpp"
#include "cpu.hpp"

struct Registers {
    uint16_t PC; // Program Counter
    uint16_t SP; // Stack Pointer
    uint8_t  A, X, Y; // Accumulator and Index registers
    uint8_t  Status;  // Flags
};

class MyCPU : public CPU {
    Registers reg;
public:
    MyCPU(AddressDecoder* mem, AddressDecoder* io) 
        : CPU(mem, io) {
        reset();
    }
    void reset() override {
        reg.PC = 0x0000;
        reg.SP = 0xFD;
        reg.A = reg.X = reg.Y = 0;
        reg.Status = 0x00;
        std::cout << "CPU Reset performed.\n";
    }

    void tick() override {
        // Fetch-Decode-Execute logic will go here
        // For now, just a placeholder increment
        uint8_t opcode = mem_bus->read(reg.PC);
        std::cout << "Ticking... Read Opcode: " << (int)opcode << " at " << reg.PC << "\n";
        reg.PC++;
    }

    void interrupt() override {
        // Push PC to stack, set interrupt flag, jump to vector
        std::cout << "Interrupt triggered.\n";
    }
};

int main() {
    Memory ram(0x8000);   // 32KB RAM
    Memory rom(0x8000);   // 32KB ROM (using RAM class for simplicity)
    
    AddressDecoder mem_decoder;
    AddressDecoder io_decoder;

    mem_decoder.map_device(0x0000, 0x7FFF, &ram);
    mem_decoder.map_device(0x8000, 0xFFFF, &rom);

    MyCPU my_cpu(&mem_decoder, &io_decoder);

    my_cpu.tick();

    return 0;
}