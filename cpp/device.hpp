#pragma once

#include <iostream>
#include <vector>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <stdexcept>

// Base class for a device
class Device {
public:
    virtual ~Device() = default;
    virtual uint8_t read(uint64_t addr) = 0;
    virtual void write(uint64_t addr, uint8_t data) = 0;
};
