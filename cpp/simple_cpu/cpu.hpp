#pragma once

#include <iostream>
#include <vector>
#include <cstdint>
#include "addr_decoder.hpp"
#include "memory.hpp"

class CPU {
public:
    AddressDecoder* mem_bus;
    AddressDecoder* io_bus;
    CPU(AddressDecoder* mem, AddressDecoder* io) 
        : mem_bus(mem), io_bus(io) { }

    virtual void reset()  = 0;
    virtual void tick()  = 0;
    virtual void interrupt() = 0;
};