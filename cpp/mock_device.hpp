#include <iostream>
#include <cassert>
#include "device.hpp"

// A Mock Device to verify the Decoder's behavior
class MockDevice : public Device {
public:
    uint8_t last_val = 0;
    uint64_t last_addr = 0;
    bool was_called = false;

    uint8_t read(uint64_t addr) override {
        was_called = true;
        last_addr = addr;
        return last_val;
    }

    void write(uint64_t addr, uint8_t data) override {
        was_called = true;
        last_addr = addr;
        last_val = data;
    }

    void reset() { was_called = false; last_addr = 0; }
};
