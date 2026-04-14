#pragma once

#include <iostream>
#include <vector>
#include <cstdint>

// Base class for anything sitting on a bus
class BusDevice {
public:
    virtual ~BusDevice() = default;
    virtual uint8_t read(uint16_t address) = 0;
    virtual void write(uint16_t address, uint8_t data) = 0;
};