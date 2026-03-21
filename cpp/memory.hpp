#include <iostream>
#include <vector>
#include "device.hpp"

// Simple RAM implementation
class Memory : public Device {
    std::vector<uint64_t> storage;
public:
    Memory(size_t size) : storage(size, 0) {}
    uint8_t read(uint64_t addr) override { return storage[addr]; }
    void write(uint64_t addr, uint8_t data) override { storage[addr] = data; }
};