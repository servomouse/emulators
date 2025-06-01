import ctypes
# import log_manager
import copy
import os
import glob
import zipfile
from datetime import datetime
import random
from dll_loader import DllLoader
from dll_devices import Memory
from logger import Logger

import platform
os_type = platform.system()
if os_type == 'Windows':
    import pefile   # pip install pefile
elif os_type == 'Linux':
    from elftools.elf.elffile import ELFFile


def test_multiple_instances(mem1, mem2):
    for i in range(100):
        addr = random.randint(0, 128)
        val1 = random.randint(0, 128)
        val2 = val1 + 1
        mem1.memory_write(addr, val1)
        mem2.memory_write(addr, val2)
        rval1 = mem1.memory_read(addr)
        rval2 = mem2.memory_read(addr)
        if (rval1 != val1) or (rval2 != val2):
            raise Exception(f"Error: Read values are not correct!")


def test_file_mapping(mem, filename):
    with open(filename) as f:
        data = f.read().encode()
    mem.memory_map_file(filename)
    for i in range(len(data)):
        rval = mem.memory_read(i)
        if rval != data[i]:
            raise Exception(f"Error: {rval = } != {data[i] = }")


def main():
    dll_loader = DllLoader()
    logger = Logger()

    memory1 = Memory("Devices/bin/libmemory.dll", 512, dll_loader, logger)
    memory2 = Memory("Devices/bin/libmemory.dll", 512, dll_loader, logger)
    test_multiple_instances(memory1, memory2)
    test_file_mapping(memory1, "bf_hello_world.txt")
    print("Memory test passed!")


if __name__ == "__main__":
    main()
