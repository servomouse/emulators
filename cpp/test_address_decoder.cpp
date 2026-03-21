#include "mock_device.hpp"
#include "address_decoder.hpp"
#include <iomanip>
#include <format>
#include <source_location>

template <typename T>
bool are_equal(const T& expected, const T& actual, 
               const std::source_location location = std::source_location::current()) {
    
    if (expected == actual) {
        return true;
    }

    // If we are dealing with bools, let's make sure they print as true/false
    // We use a local scope or just format logic here
    std::cerr << std::format("Error: expected value: {}, got: {}! File: {}:{}\n", 
                             expected, 
                             actual, 
                             location.file_name(), 
                             location.line());
    
    return false;
}

int main() {
    // 1. Instantiate Decoder with a 12-bit bus (Mask: 0xFFF)
    AddressDecoder decoder(12);
    
    MockDevice devA;
    MockDevice devB;

    // 2. Map Devices
    // devA: 0x100 to 0x1FF (Offset 0)
    // devB: 0x200 to 0x2FF (Offset 0x10)
    uint64_t rangeA[2] = {0x100, 0x1FF};
    uint64_t rangeB[2] = {0x200, 0x2FF};
    
    decoder.decoder_map(rangeA, devA, 0);
    decoder.decoder_map(rangeB, devB, 0x10);

    are_equal(1, 2);

    // --- TEST 1: Basic Read/Write ---
    std::cout << "Test 1: Routed Write... " << std::endl;
    decoder.write(0x105, 0xAA);
    std::cout << "devA.was_called: " << std::boolalpha << devA.was_called << std::endl;
    assert(devA.was_called);
    std::cout   << "devA.last_val: "
                << std::hex
                << std::showbase
                << (int)devA.last_val << std::endl;
    assert(devA.last_val == 0xAA);
    std::cout   << "devA.last_addr: "
                << std::hex
                << std::showbase
                << (int)devA.last_addr << std::endl;
    assert(devA.last_addr == 0x105);
    std::cout << "Passed." << std::endl;

    // --- TEST 2: Offset Logic ---
    std::cout << "Test 2: Offset Mapping... ";
    decoder.write(0x200, 0xBB); 
    // Address 0x200 should map to (0x200 - 0x200 + 0x10) = 0x10 on device B
    assert(devB.last_addr == 0x10);
    std::cout << "Passed." << std::endl;

    // --- TEST 3: Bus Masking ---
    // With 12-bit bus, address 0x1105 should be masked to 0x105
    std::cout << "Test 3: Bus Masking (12-bit)... ";
    devA.reset();
    decoder.write(0x1105, 0xCC);
    assert(devA.was_called && devA.last_val == 0xCC);
    std::cout << "Passed." << std::endl;

    // --- TEST 4: Unmapped Access ---
    std::cout << "Test 4: Unmapped Read... ";
    uint8_t val = decoder.read(0x500); // Nothing mapped here
    assert(val == 0xFF); 
    std::cout << "Passed." << std::endl;

    // --- TEST 5: Overlap Protection ---
    std::cout << "Test 5: Overlap Exception... ";
    uint64_t overlapRange[2] = {0x150, 0x250};
    try {
        decoder.decoder_map(overlapRange, devB, 0);
        std::cout << "Failed (No exception thrown)." << std::endl;
    } catch (const std::runtime_error& e) {
        std::cout << "Passed (Caught: " << e.what() << ")." << std::endl;
    }

    std::cout << "\nAll decoder tests passed successfully!" << std::endl;
    return 0;
}