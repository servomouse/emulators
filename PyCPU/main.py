from cpu import CPU
from memory import AddressSpace


def main():
    memory = AddressSpace(0x10000)
    io = AddressSpace(0x10000)
    cpu = CPU(mem_space=memory, io_space=io)
    print(cpu.registers["PC"])
    for _ in range(3):
        cpu.tick()
    print(cpu.registers["PC"])


if __name__ == "__main__":
    main()