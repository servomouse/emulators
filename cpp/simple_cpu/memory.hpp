#pragma once

#include <iostream>
#include <vector>
#include <cstdint>
#include <fstream>
#include "bus_device.hpp"

class Memory : public BusDevice {
    std::vector<uint8_t> data;
public:
    Memory(size_t size) : data(size, 0) {}

    uint8_t read(uint16_t address) override { return data[address]; }

    void write(uint16_t address, uint8_t data_val) override { data[address] = data_val; }

    // Completely replace memory content with file content
    bool load_binary(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        data.resize(static_cast<size_t>(size));
        if (file.read(reinterpret_cast<char*>(data.data()), size)) {
            return true;
        }
        return false;
    }

    // Save current memory state to a file
    bool save_binary(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) return false;

        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        return file.good();
    }

    // Load a file into memory at a specific offset without resizing
    bool map_image(const std::string& filename, uint16_t offset) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;

        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        // Ensure we don't overflow the existing memory buffer
        if (offset + fileSize > data.size()) {
            std::cerr << "Error: Image too large for memory at given offset.\n";
            return false;
        }

        file.read(reinterpret_cast<char*>(data.data() + offset), fileSize);
        return file.good();
    }
};