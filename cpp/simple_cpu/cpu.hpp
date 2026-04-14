#pragma once

#include <iostream>
#include <vector>
#include <cstdint>
#include "addr_decoder.hpp"
#include "memory.hpp"

struct Registers {
    uint16_t PC; // Program Counter
    uint16_t SP; // Stack Pointer
    uint8_t  A, X, Y; // Accumulator and Index registers
    uint8_t  Status;  // Flags
};

class CPU {
private:
    Registers reg;
    AddressDecoder* mem_bus;
    AddressDecoder* io_bus;

public:
    CPU(AddressDecoder* mem, AddressDecoder* io) 
        : mem_bus(mem), io_bus(io) {
        reset();
    }

    void reset() {
        reg.PC = 0x0000;
        reg.SP = 0xFD;
        reg.A = reg.X = reg.Y = 0;
        reg.Status = 0x00;
        std::cout << "CPU Reset performed.\n";
    }

    void tick() {
        // Fetch-Decode-Execute logic will go here
        // For now, just a placeholder increment
        uint8_t opcode = mem_bus->read(reg.PC);
        std::cout << "Ticking... Read Opcode: " << (int)opcode << " at " << reg.PC << "\n";
        reg.PC++;
    }

    void interrupt() {
        // Push PC to stack, set interrupt flag, jump to vector
        std::cout << "Interrupt triggered.\n";
    }
};