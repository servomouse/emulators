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
    bool print_log;
    const char *space_name;
    uint16_t unmapped_read_value = 0xFF;

public:
    void map_device(uint16_t start, uint16_t end, BusDevice* device) {
        regions.push_back({start, end, device});
    }
    uint8_t read(uint16_t address) override {
        for (auto& region : regions) {
            if (address >= region.start && address <= region.end) {
                uint16_t val = region.device->read(address - region.start);
                if (print_log) {
                    printf("\t%s: Read value 0x%X from 0x%X\n", space_name? space_name: "Mem_space", val, address);
                }
                return val;
            }
        }
        if (print_log) {
            printf("%s: Unmapped read from 0x%X, returning 0x%X\n", space_name? space_name: "Mem_space", address, unmapped_read_value);
        }
        return unmapped_read_value;
    }
    void write(uint16_t address, uint8_t data) override {
        if (print_log) {
            printf("\t%s: Writing 0x%X to 0x%X\n", space_name? space_name: "Mem_space", data, address);
        }
        for (auto& region : regions) {
            if (address >= region.start && address <= region.end) {
                region.device->write(address - region.start, data);
                return;
            }
        }
    }
    void enable_log(void) {
        print_log = true;
    }
    void disable_log(void) {
        print_log = false;
    }
    void set_memspace_name(const char *ms_name) {
        space_name = ms_name;
    }
};