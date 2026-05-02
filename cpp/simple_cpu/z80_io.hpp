#pragma once

#include <iostream>
#include <vector>
#include <cstdint>
#include <fstream>
#include "bus_device.hpp"

class IO_Space : public BusDevice {
    std::vector<uint8_t> data;
    const char *log_file_name = "zexall_test_log.txt";

    void write_log(char c) {
        // std::ios::app ensures the file is created if missing 
        // and that we don't overwrite existing data.
        std::ofstream outFile(log_file_name, std::ios::app);

        if (outFile.is_open()) {
            outFile << c;
            outFile.close();
        } else {
            std::cerr << "Error: Could not open or create the file." << std::endl;
        }
    }
public:
    IO_Space(size_t size) : data(size, 0) {}

    uint8_t read(uint16_t address) override { return data[address]; }

    void write(uint16_t address, uint8_t data_val) override {
        if ((address & 0xFF) == 1) {
            write_log(data_val);
        }
        data[address] = data_val;
    }

    void end_log(void) {
        // std::ios::app ensures the file is created if missing 
        // and that we don't overwrite existing data.
        std::ofstream outFile(log_file_name, std::ios::app);

        if (outFile.is_open()) {
            outFile << "\n===========================================\n";
            outFile.close();
        } else {
            std::cerr << "Error: Could not open or create the file." << std::endl;
        }
    }
};