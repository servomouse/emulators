class CPU {
    uint16_t pc = 0; // Program Counter
    uint8_t acc = 0; // Accumulator
    AddressDecoder& bus;

public:
    CPU(AddressDecoder& b) : bus(b) {}

    void step() {
        // 1. Fetch
        uint8_t opcode = bus.fetch(pc++);

        // 2. Decode & Execute (Simplified)
        switch (opcode) {
            case 0xA9: // LDA (Load Accumulator Immediate)
                acc = bus.fetch(pc++);
                break;
            case 0x8D: // STA (Store Accumulator to Address)
                {
                    uint16_t low = bus.fetch(pc++);
                    uint16_t high = bus.fetch(pc++);
                    bus.store((high << 8) | low, acc);
                }
                break;
            default:
                std::cout << "Unknown Opcode: " << (int)opcode << std::endl;
        }
    }
};