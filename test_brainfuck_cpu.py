import ctypes
# import log_manager
import copy
import os
import glob
import zipfile
from datetime import datetime
from dll_loader import DllLoader
from dll_devices import Memory, Cpu, AddressDecoder
from dll_loader import DllLoader
from logger import Logger


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
    p_memory.memory_map_file("bf_hello_world.txt", 0)
    address_router.memory_map_device(d_memory, [0, 128])
    address_router.memory_map_device(p_memory, [0x8000, 0x8080])

    # memory = Memory("devices/bin/libmemory.dll", 0x8100, dll_loader, logger)
    io_mem = Memory("devices/bin/libmemory.dll", 10, dll_loader, logger)
    # memory.memory_map_file("bf_hello_world.txt", 0x8000)

    cpu = Cpu("Devices/bin/libbrainfuck_cpu.dll", dll_loader, logger)

    memory_iface = {
        "mem_read": address_router.device.memory_read,
        "mem_write": address_router.device.memory_write,
        "io_read": io_mem.device.memory_read,
        "io_write": io_write,
    }
    cpu.connect_memory(memory_iface)

    counter = 0
    while 0 == cpu.module_tick(counter):
        counter += 1
        # if counter == 950:
        #     p_memory.set_log_level(4)

    print(f"{counter = }")
    print(io_buf)
    output = [chr(i) for i in io_buf]
    print(output)


if __name__ == "__main__":
    main()
