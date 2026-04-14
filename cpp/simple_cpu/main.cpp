#include <iostream>
#include <vector>
#include <cstdint>
#include "addr_decoder.hpp"
#include "memory.hpp"
#include "cpu.hpp"

int main() {
    // 1. Create Hardware
    RAM main_ram(0x8000);   // 32KB RAM
    RAM bios_rom(0x8000);   // 32KB ROM (using RAM class for simplicity)
    
    AddressDecoder mem_decoder;
    AddressDecoder io_decoder;

    // 2. Map Memory: RAM at 0x0000, ROM at 0x8000
    mem_decoder.map_device(0x0000, 0x7FFF, &main_ram);
    mem_decoder.map_device(0x8000, 0xFFFF, &bios_rom);

    // 3. Connect CPU
    CPU my_cpu(&mem_decoder, &io_decoder);

    // Test a tick
    my_cpu.tick();

    return 0;
}