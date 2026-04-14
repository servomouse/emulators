#pragma once

#include <iostream>
#include <vector>
#include <cstdint>
#include "bus_device.hpp"

struct Mapping {
    uint16_t start;
    uint16_t end;
    BusDevice* device;
};

class AddressDecoder : public BusDevice {
private:
    std::vector<Mapping> regions;

public:
    void map_device(uint16_t start, uint16_t end, BusDevice* device) {
        regions.push_back({start, end, device});
    }

    uint8_t read(uint16_t address) override {
        for (auto& region : regions) {
            if (address >= region.start && address <= region.end) {
                return region.device->read(address - region.start);
            }
        }
        return 0xFF; // Open bus behavior
    }

    void write(uint16_t address, uint8_t data) override {
        for (auto& region : regions) {
            if (address >= region.start && address <= region.end) {
                region.device->write(address - region.start, data);
                return;
            }
        }
    }
};