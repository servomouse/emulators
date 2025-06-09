import ctypes
import copy
import os
import glob
import zipfile
from datetime import datetime
from dll_loader import DllLoader
from dll_devices import Memory, Cpu, AddressDecoder
from clock import Clock
from logger import Logger
from io_devices import IODevice


import platform
os_type = platform.system()
if os_type == 'Windows':
    import pefile   # pip install pefile
elif os_type == 'Linux':
    from elftools.elf.elffile import ELFFile


io_buf = []

def io_write(addr, val):
    print(f"Device print: {chr(val)}")
    io_buf.append(val)


def main():
    dll_loader = DllLoader()
    logger = Logger()

    address_router = AddressDecoder("devices/bin/libaddress_decoder.dll", dll_loader, logger)

    d_memory = Memory("devices/bin/libmemory.dll", 128, dll_loader, logger)
    p_memory = Memory("devices/bin/libmemory.dll", 128, dll_loader, logger)
    p_memory.memory_map_file("binaries/bf_hello_world.txt", 0)
    address_router.memory_map_device(d_memory, [0, 128])
    address_router.memory_map_device(p_memory, [0x8000, 0x8080])

    io_mem = Memory("devices/bin/libmemory.dll", 10, dll_loader, logger)
    io = IODevice()

    cpu = Cpu("Devices/bin/libbrainfuck_cpu.dll", dll_loader, logger)

    memory_iface = {
        "mem_read": address_router.device.memory_read,
        "mem_write": address_router.device.memory_write,
        "io_read": io.io_input,
        "io_write": io.io_output,
    }
    cpu.connect_memory(memory_iface)

    clock = Clock(100)
    clock.add_device(cpu)

    counter = 0
    while clock.tick():
        counter += 1

    print(f"{counter = }")
    # print(io_buf)
    # output = [chr(i) for i in io_buf]
    # print(output)
    io.print_buffer_char()


if __name__ == "__main__":
    main()
