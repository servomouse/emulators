#pragma once

#include <iostream>
#include <vector>
#include <cstdint>
#include "bus_device.hpp"

class Memory : public BusDevice {
    std::vector<uint8_t> data;
public:
    Memory(size_t size) : data(size, 0) {}
    uint8_t read(uint16_t address) override { return data[address]; }
    void write(uint16_t address, uint8_t data_val) override { data[address] = data_val; }
};