#include "device.hpp"

class AddressDecoder : public Device {
private:
    struct Mapping {
        uint64_t start;
        uint64_t end;
        uint64_t offset;
        Device* device; // Using raw pointer for "association" (non-owning)
    };

    std::vector<Mapping> mappings;
    uint64_t bus_mask;

    // Helper to check for overlaps
    bool is_overlapping(uint64_t start, uint64_t end) {
        for (const auto& m : mappings) {
            if (start <= m.end && end >= m.start) return true;
        }
        return false;
    }

public:
    // Constructor handles the bit-width logic
    AddressDecoder(int bus_width) {
        // e.g., 8-bit width -> 0xFF mask
        bus_mask = (bus_width >= 64) ? ~0ULL : (1ULL << bus_width) - 1;
        std::cout   << "bus_width: "
                    << (int)bus_width
                    << ", bus_mask: "
                    << std::hex
                    << std::showbase
                    << (int)bus_mask << std::endl;
    }

    void decoder_map(uint64_t range[2], Device& device, uint64_t offset) {
        if (is_overlapping(range[0], range[1])) {
            throw std::runtime_error("Address range overlap detected!");
        }
        mappings.push_back({range[0], range[1], offset, &device});
    }

    void decoder_demap(Device& device) {
        // Erase-remove idiom (modern C++ way to filter a collection)
        mappings.erase(std::remove_if(mappings.begin(), mappings.end(),
            [&](const Mapping& m) { return m.device == &device; }), 
            mappings.end());
    }

    // Overriding Device methods
    uint8_t read(uint64_t addr) override {
        uint64_t masked_addr = addr & bus_mask;
        std::cout   << "Reading from masked_addr: "
                    << std::hex
                    << std::showbase
                    << (int)masked_addr << std::endl;
        for (const auto& m : mappings) {
            if (masked_addr >= m.start && masked_addr <= m.end) {
                return m.device->read(masked_addr + m.offset);
            }
        }
        return 0xFF; // "Floating" bus return
    }

    void write(uint64_t addr, uint8_t data) override {
        uint64_t masked_addr = addr & bus_mask;
        std::cout   << "Writing to masked_addr: "
                    << std::hex
                    << std::showbase
                    << (int)masked_addr << std::endl;
        for (const auto& m : mappings) {
            if (masked_addr >= m.start && masked_addr <= m.end) {
                m.device->write(masked_addr + m.offset, data);
                return;
            }
        }
    }
};
